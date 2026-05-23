// corepsstran.cpp
//
// PssTranCore implementation.
// See corepsstran.h for design description and algorithm context.

#include "corepsstran.h"
#include "ansupport.h"
#include "coretrancoef.h"
#include "densematrix.h"
#include "simulator.h"
#include "common.h"
#include <deque>
#include <utility>

namespace NAMESPACE {

PssTranCore::PssTranCore(
    OutputDescriptorResolver& parentResolver,
    TranParameters& params,
    OperatingPointCore& opCore,
    Circuit& circuit,
    CommonData& commons,
    KluRealMatrix& jacobian,
    VectorRepository<double>& solution,
    VectorRepository<double>& states
) : TranCore(parentResolver, params, opCore, circuit, commons,
             jacobian, solution, states),
    lastAlpha_(0.0),
    lastB1_(1.0),
    phiValid_(false),
    captureTrajectory_(false) {
}


// ----------------------------------------------------------------
// rebuild
// ----------------------------------------------------------------

bool PssTranCore::rebuild(Status& s) {
    if (!TranCore::rebuild(s)) return false;
    auto n = circuit.unknownCount();
    if (!lastAlr_.rebuild(circuit.sparsityMap(), n)) {
        s.set(Status::Analysis, "PssTranCore: failed to rebuild Alr scratch matrix.");
        return false;
    }

    // Resize AM scratch/history buffers to match Jacobian non-zero count.
    auto nnz = jacobian.nnz();

    if (!scratchC_.rebuild(circuit.sparsityMap(), n)) {
        s.set(Status::Analysis, "PssTranCore: failed to rebuild C scratch matrix.");
        return false;
    }

    if (!scratchC_.rebuild(circuit.sparsityMap(), n)) {
        s.set(Status::Analysis, "PssTranCore: failed to rebuild C scratch matrix.");
        return false;
    }
    return true;
}


// ----------------------------------------------------------------
// setShootIC
// ----------------------------------------------------------------

void PssTranCore::setShootIC(const Vector<double>& x0) {
    // Populate preprocessedIc with every circuit unknown so that TranCore's
    // UIC branch (setForces -> unknownValue_ -> solution) starts the shoot
    // from x0.  setForces zeros unknownValue_ before applying forces, so we
    // must supply ALL unknowns here — not just the user-visible IC nodes.
    preprocessedIc.clear();
    auto nNodes = circuit.nodeCount();
    for (NodeIndex i = 0; i < nNodes; i++) {
        Node* nd = circuit.node(i);
        auto u = nd->unknownIndex();
        if (u > 0) {
            preprocessedIc.nodes.push_back(nd);
            preprocessedIc.nodeValues.push_back(x0[u]);
        }
    }
}


// ----------------------------------------------------------------
// clearTrajectory
// ----------------------------------------------------------------

void PssTranCore::clearTrajectory(double T0) {
    T0_ = T0;

    phiHist_.clear();
    lastAlpha_ = 0.0;
    phiValid_  = false;

    auto n = circuit.unknownCount();
    phiCurrent_.resize(n, n, DenseMatrix<double>::Major::Column);
    phiCurrent_.identity();

    cHistData_.clear();
    gHistData_.clear();
    qHistData_.clear();
    prevCValid_ = false;

    psiHist_.clear();
    psiCurrent_.assign(n, 0.0);

    // Get C_0 and q_0 by evaluating the reactive jacobian and residual at this point
    Vector<double> qSnap(n + 1, 0.0);
    jacobian.zero();
    EvalSetup es = getNrSolver().evalSetup();
    es.evaluateResistiveJacobian = false;
    es.evaluateReactiveJacobian  = true;
    es.evaluateResistiveResidual = false;
    es.evaluateReactiveResidual  = true;
    es.storeReactiveState        = true;
    es.evaluateOutvars           = false;
    es.allowBypass               = false;

    LoadSetup ls;
    ls.loadReactiveJacobian   = true;
    ls.reactiveJacobianFactor = 1.0;
    ls.reactiveResidual       = qSnap.data();

    if (!circuit.evalAndLoad(commons, &es, &ls, nullptr)) {
        Simulator::err() << "PssTranCore: evalAndLoad(C) failed for C(x_0) \n";
        /// TODO: This function should return false so VACASK can abort
        return;
    }
    // Snapshot C_0 and add it to history
    Vector<double> cSnap(jacobian.data(), jacobian.data() + jacobian.nnz());
    cHistData_.push_front(std::move(cSnap));
    // Seed G history with zeros — G_0 unavailable at t=0 (no NR solve yet)
    Vector<double> gZero(jacobian.nnz(), 0.0);
    gHistData_.push_front(std::move(gZero));
    // Add q_0 to history
    qHistData_.push_front(std::move(qSnap));

    captureTrajectory_= false;
}


// ----------------------------------------------------------------
// onTimestepAccepted — inline Phi advancement
// ----------------------------------------------------------------
bool PssTranCore::onTimestepAccepted(double tSolve, double hk, Int order) {
    IntegratorCoeffs    integCoeffs = getIntegCoeffs();
    auto                n           = circuit.unknownCount();
    auto                nnz         = jacobian.nnz();

    // Expand phiHist with identity matrices during ramp-up
    while (phiHist_.size() < order) {
        DenseMatrix<double> id(n, n, DenseMatrix<double>::Major::Column);
        id.identity();
        phiHist_.push_front(std::move(id));
    }

    // Expand psiHist with zero matrices during ramp-up
    while (psiHist_.size() < order) {
        Vector<double> z(n, 0.0);
        psiHist_.push_front(std::move(z));
    }


    // Get Alr = G_k + alpha_k * C_k from the factored NR jacobian
    std::copy(jacobian.data(), jacobian.data() + nnz, lastAlr_.data());
    if (!lastAlr_.refactor()) {
        Simulator::err() << "PssTranCore: Alr refactorisation failed at t="
                         << tSolve << "\n";
        return false;
    }

    // Allocate buffer for q(x_k) with bucket at index 0
    Vector<double> qSnap(n + 1, 0.0);

    // Get the current C_k and q(x_k) by using evalAndLoad. C_k will be saved to jacobian
    jacobian.zero();
    EvalSetup es = getNrSolver().evalSetup();
    es.evaluateResistiveJacobian = false;
    es.evaluateReactiveJacobian  = true;
    es.evaluateResistiveResidual = false;
    es.evaluateReactiveResidual  = true;
    es.storeReactiveState        = true;
    es.evaluateOutvars           = false;
    es.allowBypass               = false;

    LoadSetup ls;
    ls.loadReactiveJacobian   = true;
    ls.reactiveJacobianFactor = 1.0;
    ls.reactiveResidual       = qSnap.data();

    if (!circuit.evalAndLoad(commons, &es, &ls, nullptr)) {
        Simulator::err() << "PssTranCore: evalAndLoad(C) failed at t="
                        << tSolve << "\n";
        return false;
    }
    // Snapshot current C_k
    Vector<double> cSnap(jacobian.data(), jacobian.data() + jacobian.nnz());

    double alpha = integCoeffs.leadingCoeff();
    Vector<double> a = integCoeffs.a();
    Vector<double> b = integCoeffs.b();

    // G_k = A_k - alpha_k * C_k  (lastAlr_.data() holds A_k; not modified by refactor)
    Vector<double> gSnap(nnz);
    for (Int j = 0; j < nnz; j++)
        gSnap[j] = lastAlr_.data()[j] - alpha * cSnap[j];

    // gammaC[p] = alpha * a[p]    — coefficient for the C term
    // gammaG[p] = -(b[p] / b1)   — coefficient for the G term (zero for BDF)
    Vector<double> gammaC(order, 0.0);
    Vector<double> gammaG(order, 0.0);
    for (int p = 0; !a.empty() && p < std::min(order, (Int)a.size()); p++)
        gammaC[p] = alpha * a[p];
    for (int p = 0; !b.empty() && p < std::min(order, (Int)b.size()); p++)
        gammaG[p] = -(b[p] / integCoeffs.b1());

    lastAlpha_ = alpha;
    lastB1_    = b.empty() ? 1.0 : integCoeffs.b1();

    // Build right-hand side: sum_p (gammaC[p]*C_{k-p} + gammaG[p]*G_{k-p}) * Phi_{k-p}
    DenseMatrix<double> rhs(n, n, DenseMatrix<double>::Major::Column);
    Vector<double> phi_colbuf(n);
    Vector<double> rhs_colbuf(n);
    for (int p = 0; p < order; p++) {
        DenseMatrix<double>& Phi_kmi = phiHist_[p];

        // C_{k-p} * Phi_{k-p} contribution
        std::copy(cHistData_[p].begin(), cHistData_[p].end(), scratchC_.data());
        for (decltype(n) j = 0; j < n; j++) {
            auto phi_col = Phi_kmi.column(j);
            for (decltype(n) i = 0; i < n; i++) phi_colbuf[i] = phi_col[i];
            double* rhs_col = rhs.data().data() + static_cast<size_t>(j) * n;
            if (!scratchC_.product(phi_colbuf.data(), rhs_colbuf.data())) {
                Simulator::err() << "PssTranCore: C*Phi product failed at t="
                                 << tSolve << ", column " << j << "\n";
                return false;
            }
            for (decltype(n) i = 0; i < n; i++) rhs_col[i] += gammaC[p] * rhs_colbuf[i];
        }

        // G_{k-p} * Phi_{k-p} contribution (AM methods only; gammaG[p]==0 for BDF)
        if (gammaG[p] != 0.0) {
            std::copy(gHistData_[p].begin(), gHistData_[p].end(), scratchC_.data());
            for (decltype(n) j = 0; j < n; j++) {
                auto phi_col = Phi_kmi.column(j);
                for (decltype(n) i = 0; i < n; i++) phi_colbuf[i] = phi_col[i];
                double* rhs_col = rhs.data().data() + static_cast<size_t>(j) * n;
                if (!scratchC_.product(phi_colbuf.data(), rhs_colbuf.data())) {
                    Simulator::err() << "PssTranCore: G*Phi product failed at t="
                                     << tSolve << ", column " << j << "\n";
                    return false;
                }
                for (decltype(n) i = 0; i < n; i++) rhs_col[i] += gammaG[p] * rhs_colbuf[i];
            }
        }
    }
    
    // Solve for Phi_k
    // Alr * Phi_k = sum_{i=1}^order (gamma_i * C_k-i * Phi_k-i)
    if (!lastAlr_.solveBlock(rhs.data().data(), static_cast<Int>(n))) {
        Simulator::err() << "PssTranCore: block Alr solve failed at t="
                         << tSolve << "\n";
        return false;
    }
    phiCurrent_ = rhs;   // rhs now holds PhiT at this step

    /// Psi integration
    Vector<double> psiRhs(n, 0.0);

    // First sum (history terms): sum_p (gammaC[p]*C_{k-p} + gammaG[p]*G_{k-p}) * psi_{k-p}
    Vector<double> c_psi(n, 0.0);
    for (int p = 0; p < order; p++) {
        // C_{k-p} * psi_{k-p} contribution
        std::copy(cHistData_[p].begin(), cHistData_[p].end(), scratchC_.data());
        if (!scratchC_.product(psiHist_[p].data(), c_psi.data())) {
            Simulator::err() << "PssTranCore: C*psi product failed at t=" << tSolve << "\n";
            return false;
        }
        for (decltype(n) i = 0; i < n; i++) psiRhs[i] += gammaC[p] * c_psi[i];

        // G_{k-p} * psi_{k-p} contribution (AM methods only)
        if (gammaG[p] != 0.0) {
            std::copy(gHistData_[p].begin(), gHistData_[p].end(), scratchC_.data());
            if (!scratchC_.product(psiHist_[p].data(), c_psi.data())) {
                Simulator::err() << "PssTranCore: G*psi product failed at t=" << tSolve << "\n";
                return false;
            }
            for (decltype(n) i = 0; i < n; i++) psiRhs[i] += gammaG[p] * c_psi[i];
        }
    }

    // Second sum: \sum_{i=0}^{p-1} \frac{\alpha}{T_0} a_i q_{k-i}
    for (int p=0; !a.empty() && p < std::min(order, (Int)a.size()); p++) {
        for (decltype(n) i = 0; i < n; i++) psiRhs[i] -= (alpha / T0_) * a[p] * qHistData_[p][i + 1];
    }

    // Reactive residual term
    for (decltype(n) i = 0; i < n; i++) psiRhs[i] += (alpha / T0_) * qSnap[i + 1];

    // Solve for \psi_{k+1}
    if (!lastAlr_.solve(psiRhs.data())) {
        Simulator::err() << "PssTranCore: psi solve failed at t=" << tSolve << "\n";
        return false;
    }
    psiCurrent_ = psiRhs;   // psiRhs now holds psi_{k+1}

    // If trajectory capture is enabled, store data needed for adjoint integration.
    // cSnap and gSnap must not be moved yet — the record copies them.
    if (captureTrajectory_) {
        StepRecord rec;
        rec.aData  = Vector<double>(lastAlr_.data(), lastAlr_.data() + nnz);
        rec.cData  = cSnap;
        rec.gData  = gSnap;
        rec.gammaC = gammaC;
        rec.gammaG = gammaG;
        rec.order  = order;
        trajectory_.push_back(std::move(rec));
    }

    /* Add values to history deques */

    // Add Phi to history
    DenseMatrix<double> phiSnap(phiCurrent_);
    phiHist_.push_front(std::move(phiSnap));

    // Add C_k and G_k to history
    cHistData_.push_front(std::move(cSnap));
    gHistData_.push_front(std::move(gSnap));

    // Add q(x_k) to history
    qHistData_.push_front(std::move(qSnap));

    // Add psi to history
    psiHist_.push_front(psiCurrent_);

    // Trim histories
    while (phiHist_.size() > order + 1) phiHist_.pop_back();
    while (cHistData_.size() > order + 1) cHistData_.pop_back();
    while (gHistData_.size() > order + 1) gHistData_.pop_back();
    while (qHistData_.size() > order + 1) qHistData_.pop_back();

    phiValid_ = true;
    return true;

}

// ----------------------------------------------------------------
// integrateSensitivity
// ----------------------------------------------------------------

bool PssTranCore::integrateSensitivity(
    DenseMatrix<double>& PhiT
) {
    if (!phiValid_) {
        Simulator::err() << "PssTranCore: no accepted steps since clearTrajectory(); "
                            "PhiT is not available.\n";
        return false;
    }
    // PhiT: the inline-computed sensitivity matrix is ready.
    PhiT = phiCurrent_;
    return true;
}


// ----------------------------------------------------------------
// integrateAugmentedSensitivity
// ----------------------------------------------------------------

bool PssTranCore::integrateAugmentedSensitivity(
    DenseMatrix<double>& PhiT,
    Vector<double>&      PsiT,
    Vector<double>&      x_laststep
) {
    if (!phiValid_) {
        Simulator::err() << "PssTranCore: no accepted steps since clearTrajectory(); "
                            "PhiT is not available.\n";
        return false;
    }

    // PhiT: the inline-computed sensitivity matrix is ready.
    PhiT = phiCurrent_;

    auto n = circuit.unknownCount();
    PsiT.resize(n + 1);
    PsiT[0] = 0.0;
    for (decltype(n) i = 0; i < n; i++) {
        PsiT[i + 1] = psiCurrent_[i];
    }

    return true;
}

// ----------------------------------------------------------------
// integrateAdjointMonodromy
// ----------------------------------------------------------------
bool PssTranCore::integrateAdjointMonodromy(DenseMatrix<double>& Omega){
    if (trajectory_.empty()) {
        Simulator::err() << "PssTranCore: no trajectory captured; "
                            "call enableTrajectoryCapture() before the final shoot.\n";
        return false;
    }

    auto n   = circuit.unknownCount();
    auto nnz = jacobian.nnz();

    // Omega_N = I  (initial condition at t=T0)
    Omega.resize(n, n, DenseMatrix<double>::Major::Column);
    Omega.identity();

    // History is built in this function and not needed anywhere else, so declare it here
    std::deque<DenseMatrix<double>> omegaHist;
    omegaHist.push_front(Omega);  // seed with Omega_N = I

    // Scratch KLU matrix for A_k (for tsolve) and C_k (for tproduct)
    KluRealMatrix scratchA;
    KluRealMatrix scratchC;
    KluRealMatrix scratchG;
    if (!scratchA.rebuild(circuit.sparsityMap(), n)) {
        Simulator::err() << "PssTranCore: failed to rebuild scratchA for Omega integration.\n";
        return false;
    }
    if (!scratchC.rebuild(circuit.sparsityMap(), n)) {
        Simulator::err() << "PssTranCore: failed to rebuild scratchC for Omega integration.\n";
        return false;
    }
    if (!scratchG.rebuild(circuit.sparsityMap(), n)) {
        Simulator::err() << "PssTranCore: failed to rebuild scratchG for Omega integration.\n";
        return false;
    }

    Int nSteps = static_cast<Int>(trajectory_.size());


    // 1. Find the absolute maximum LMS stencil depth used in this transient.
    // This allows us to trim the history buffer safely without making ANY
    // assumptions about the underlying integration method (BDF, AM, Gear, etc.).
    Int maxLMSOrder = 1;
    for (const auto& rec : trajectory_) {
        if (rec.order > maxLMSOrder) {
            maxLMSOrder = rec.order;
        }
    }

    // Walk trajectory in reverse (backward in time)
    for (Int k = nSteps - 1; k >= 0; k--) {
        // 1. Fetch A_k and C_k
        // trajectory_ stores step m+1 at index m. So A_k is at index k-1.
        // By periodicity, A_0 = A_N and C_0 = C_N, found at nSteps-1.
        Int acIdx = (k == 0) ? (nSteps - 1) : (k - 1);
        const StepRecord& acRec = trajectory_[acIdx];

        ///////////////
        bool printStep = true;

        // Load A_k into scratchA and refactor for tsolve
        std::copy(acRec.aData.begin(), acRec.aData.end(), scratchA.data());
        if (!scratchA.refactor()) {
            Simulator::err() << "PssTranCore: scratchA refactor failed at backward step k="
                             << k << "\n";
            return false;
        }

        // Load C_k and G_k for tproduct
        std::copy(acRec.cData.begin(), acRec.cData.end(), scratchC.data());
        std::copy(acRec.gData.begin(), acRec.gData.end(), scratchG.data());

        // Determine if any future record in the look-ahead window has nonzero gammaG
        Int histSize = static_cast<Int>(omegaHist.size());
        bool anyGammaG = false;
        for (Int i = 0; !anyGammaG && i < histSize && k + i < nSteps; i++) {
            const StepRecord& fr = trajectory_[k + i];
            if (i < fr.order && i < (Int)fr.gammaG.size() && fr.gammaG[i] != 0.0)
                anyGammaG = true;
        }

        // Build RHS column by column
        // RHS[:,j] = C_k^T * sum_i(gammaC_i * Omega_{k+i+1}[:,j])
        //          + G_k^T * sum_i(gammaG_i * Omega_{k+i+1}[:,j])   (AM only)
        DenseMatrix<double> rhs(n, n, DenseMatrix<double>::Major::Column);
        Vector<double> s_col(n);
        Vector<double> s_col_g(n);
        Vector<double> rhs_col(n);

        for (decltype(n) j = 0; j < n; j++) {
            std::fill(s_col.begin(), s_col.end(), 0.0);
            if (anyGammaG) std::fill(s_col_g.begin(), s_col_g.end(), 0.0);

            for (Int i = 0; i < histSize; i++) {
                if (k + i >= nSteps) break;
                const StepRecord& futureRec = trajectory_[k + i];
                if (i >= futureRec.order) continue;

                double gC = (i < (Int)futureRec.gammaC.size()) ? futureRec.gammaC[i] : 0.0;
                DenseMatrix<double>& Om = omegaHist[i];
                for (decltype(n) row = 0; row < n; row++)
                    s_col[row] += gC * Om.at(row, j);

                if (anyGammaG) {
                    double gG = (i < (Int)futureRec.gammaG.size()) ? futureRec.gammaG[i] : 0.0;
                    for (decltype(n) row = 0; row < n; row++)
                        s_col_g[row] += gG * Om.at(row, j);
                }
            }

            // Apply C_k^T
            if (!scratchC.tproduct(s_col.data(), rhs_col.data())) {
                Simulator::err() << "PssTranCore: C^T product failed at backward step k="
                                 << k << "\n";
                return false;
            }
            for (decltype(n) i = 0; i < n; i++) rhs.at(i, j) = rhs_col[i];

            // Apply G_k^T (AM methods only)
            if (anyGammaG) {
                if (!scratchG.tproduct(s_col_g.data(), rhs_col.data())) {
                    Simulator::err() << "PssTranCore: G^T product failed at backward step k="
                                     << k << "\n";
                    return false;
                }
                for (decltype(n) i = 0; i < n; i++) rhs.at(i, j) += rhs_col[i];
            }
        }

        // Solve A_k^T * Omega_k = rhs (overwrites rhs with solution)
        if (!scratchA.tsolveBlock(rhs.data().data(), static_cast<Int>(n))) {
            Simulator::err() << "PssTranCore: tsolveBlock failed at backward step k="
                             << k << "\n";
            return false;
        }

        // Push Omega_k to history
        DenseMatrix<double> omegaSnap(rhs);
        omegaHist.push_front(std::move(omegaSnap));

        // Trim history to depth order+1
        while (static_cast<Int>(omegaHist.size()) > maxLMSOrder + 1) omegaHist.pop_back();
    }

    // Omega_0 is the last computed value
    Omega = omegaHist.front();
    return true;
}

void PssTranCore::enableTrajectoryCapture() {
    captureTrajectory_ = true;
    trajectory_.clear();
        }

// ----------------------------------------------------------------
// PSS rawfile output
// ----------------------------------------------------------------

static std::string pssIntegratorName(Id m) {
    if (m == TranCore::methodTrapezoidal)                          return "am1";
    if (m == TranCore::methodBDF2 || m == TranCore::methodGear2)   return "bdf2";
    if (m == TranCore::methodBDF  || m == TranCore::methodGear)    return "bdf";
    if (m == TranCore::methodAM)                                   return "am";
    if (m == TranCore::methodEuler)                                return "bdf1";
    return std::string(m);
}

bool PssTranCore::addDefaultOutputDescriptors() {
    if (Simulator::noOutput()) return true;
    return addAllUnknowns(PTSave("default", Id(), Id()));
}

bool PssTranCore::initializeOutputs(Id name, Status& s) {
    if (!outfile) {
        outfile = new OutputRawfile(
            std::string(name), outputDescriptors, outputSources,
            (circuit.simulatorOptions().core().rawfile == SimulatorOptions::rawfileBinary
                ? OutputRawfile::Flags::Binary : OutputRawfile::Flags::None) |
            OutputRawfile::Flags::Padded);
        outfile->setTitle(circuit.title());
        Id method = circuit.simulatorOptions().core().tran_method;
        outfile->setPlotname("Periodic Steady State " + pssIntegratorName(method));
    }
    outfile->prologue(s);
    return true;
}

} // namespace NAMESPACE
