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
    phiValid_(false) {
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

void PssTranCore::clearTrajectory() {
    phiHist_.clear();
    lastAlpha_ = 0.0;
    phiValid_  = false;

    auto n = circuit.unknownCount();
    phiCurrent_.resize(n, n, DenseMatrix<double>::Major::Column);
    phiCurrent_.identity();

    cHistData_.clear();
    prevCValid_ = false;
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
        phiHist_.push_back(std::move(id));
    }

    // Get Alr = G_k + alpha_k * C_k from the factored NR jacobian
    std::copy(jacobian.data(), jacobian.data() + nnz, lastAlr_.data());
    if (!lastAlr_.refactor()) {
        Simulator::err() << "PssTranCore: Alr refactorisation failed at t="
                         << tSolve << "\n";
        return false;
    }

    // Get the current C_k by using evalAndLoad. Will be saved to jacobian
    jacobian.zero();
    EvalSetup es = getNrSolver().evalSetup();
    es.evaluateResistiveJacobian = false;
    es.evaluateReactiveJacobian  = true;
    es.evaluateResistiveResidual = false;
    es.evaluateReactiveResidual  = false;
    es.evaluateOutvars           = false;
    es.allowBypass               = false;

    LoadSetup ls;
    ls.loadReactiveJacobian   = true;
    ls.reactiveJacobianFactor = 1.0;

    if (!circuit.evalAndLoad(commons, &es, &ls, nullptr)) {
        Simulator::err() << "PssTranCore: evalAndLoad(C) failed at t="
                        << tSolve << "\n";
        return false;
    }
    // Snapshot current C_k
    Vector<double> cSnap(jacobian.data(), jacobian.data() + jacobian.nnz());

    // Add C_k to history
    /// TODO: Does jacobian sparsity change? It shouldn't
    cHistData_.push_front((std::move(cSnap)));

    // gamma = a_i + alpha * b_i
    Vector<double> gamma(order, 0.0);
    double alpha = integCoeffs.leadingCoeff();
    Vector<double> a = integCoeffs.a();
    Vector<double> b = integCoeffs.b();

    if (!a.empty()) {
        for (int p=0; p<std::min(order, (Int)a.size()); p++) {
            gamma[p] += a[p];
        }
    }
    if (!b.empty()) {
        for (int p=0; p<std::min(order, (Int)b.size()); p++) {
            gamma[p] += alpha * b[p];
        }
    }

    lastAlpha_ = alpha;
    lastB1_    = b.empty() ? 1.0 : integCoeffs.b1();

    // Build right-hand side sum(gamma_i * C_k-i * Phi_k-i)
    DenseMatrix<double> rhs(n, n, DenseMatrix<double>::Major::Column);
    Vector<double> C_kmi;
    DenseMatrix<double> Phi_kmi;
    Vector<double> colBuf(n); 
    for (int p=0; p < order; p++) {
        C_kmi   = cHistData_[p];
        Phi_kmi = phiHist_[p];
        
        // Put C_k-i values into scratchC_
        std::copy(C_kmi.begin(), C_kmi.end(), scratchC_.data());
        // Multiply column by column
        for (decltype(n) j = 0; j < n; j++) {
            auto phi_col = Phi_kmi.column(j);
            for (decltype(n) i = 0; i < n; i++) colBuf[i] = phi_col[i];
            double* rhs_col = rhs.data().data() + static_cast<size_t>(j) * n;
            if (!scratchC_.product(colBuf.data(), rhs_col)) {
                Simulator::err() << "PssTranCore: C_{k-1}*v product failed at t="
                                 << tSolve << ", column " << j << "\n";
                return false;
            }
            // Multiply by gamma_i
            for (decltype(n) i = 0; i < n; i++) rhs_col[i] *= gamma[p];
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

    // Add Phi to history
    DenseMatrix<double> phiSnap(phiCurrent_);
    phiHist_.push_front(std::move(phiSnap));

    // Trim Phi history
    while (phiHist_.size() > order) phiHist_.pop_back();
    // Trim C history
    while (cHistData_.size() > order) cHistData_.pop_back();

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
        PsiT[i + 1] = lastAlpha_ * lastB1_ * x_laststep[i];
    }

    return true;
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
