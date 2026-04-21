// corepsstran.cpp
//
// PssTranCore implementation.
// See corepsstran.h for design description and algorithm context.

#include "corepsstran.h"
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
}


// ----------------------------------------------------------------
// onTimestepAccepted — inline Phi advancement
// ----------------------------------------------------------------

bool PssTranCore::onTimestepAccepted(double tSolve, double hk, Int order) {
    // Retrieve the LMS coefficients that were active for this step.
    // getIntegCoeffs() is valid here: TranCore has already called
    // compute() and scaleDifferentiator() before entering NR.
    const auto& ic  = getIntegCoeffs();
    const auto& bsc = ic.bScaled();

    // BDF methods have bScaled empty.  Methods with past-derivative terms
    // (e.g. trapezoidal) are not supported because xdot history is not
    // collected during the shoot.
    if (!bsc.empty()) {
        Simulator::err() << "PssTranCore: PSS sensitivity requires a BDF method "
                            "(no past-derivative terms); method has "
                         << bsc.size() << " bScaled entries at t=" << tSolve << "\n";
        return false;
    }

    double      alpha = ic.leadingCoeff();
    const auto& asc   = ic.aScaled();
    auto        n     = circuit.unknownCount();
    auto        nnz   = jacobian.nnz();

    // ----------------------------------------------------------------
    // 1. Capture Alr = G_k + alpha_k * C_k from the factored NR jacobian.
    //
    //    At this point jacobian.data() holds the Ax values of G + alpha*C
    //    that the NR loop evaluated and factored for the accepted step.
    //    Copy them directly so lastAlr_ contains the identical matrix —
    //    no additional evalAndLoad call is needed for Alr.
    // ----------------------------------------------------------------
    std::copy(jacobian.data(), jacobian.data() + nnz, lastAlr_.data());

    // ----------------------------------------------------------------
    // 2. Evaluate C_k alone.
    //
    //    C_k is needed to build the Phi RHS.  Using jacobian as the
    //    scratch buffer (zeroes it; TranCore will rebuild from scratch
    //    at the next NR iteration).
    // ----------------------------------------------------------------
    jacobian.zero();
    {
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
    }
    std::copy(jacobian.data(), jacobian.data() + nnz, scratchC_.data());
    jacobian.zero();  // leave clean; TranCore rebuilds for the next step

    // ----------------------------------------------------------------
    // 3. Factor lastAlr_.
    //
    //    refactor() reuses the symbolic factorisation from rebuild(),
    //    performing only the cheaper numeric factorisation each step.
    // ----------------------------------------------------------------
    if (!lastAlr_.refactor()) {
        Simulator::err() << "PssTranCore: Alr refactorisation failed at t="
                         << tSolve << "\n";
        return false;
    }
    lastAlpha_ = alpha;

    // ----------------------------------------------------------------
    // 4. Extend phiHist with identity matrices during order ramp-up.
    //
    //    asc.size() past Phi matrices are needed (index 0 = current, i>=1
    //    come from phiHist).  Fill any missing slots with I: before t0 the
    //    circuit is at the limit cycle so the sensitivity is the identity.
    // ----------------------------------------------------------------
    while (phiHist_.size() + 1 < asc.size()) {
        DenseMatrix<double> id(n, n, DenseMatrix<double>::Major::Column);
        id.identity();
        phiHist_.push_back(std::move(id));
    }

    // ----------------------------------------------------------------
    // 5. Build the RHS matrix (column-major n×n).
    //
    //    RHS[:,j] = C_k * phiSum[:,j]
    //    phiSum[:,j] = sum_{si=0}^{asc.size()-1} asc[si] * src[si][:,j]
    //    src[0] = phiCurrent_ (= Phi_{k}),  src[si>=1] = phiHist_[si-1].
    //
    //    Column-major layout lets the block solve (step 6) pass the raw
    //    data pointer directly to klu_solve with ldim = n.
    // ----------------------------------------------------------------
    DenseMatrix<double> phiSnap(phiCurrent_);  // snapshot before overwriting

    DenseMatrix<double> phiRhs(n, n, DenseMatrix<double>::Major::Column);

    Vector<double> colBuf(n);   // phiSum column j (contiguous scratch)
    Vector<double> cvec(n);     // C_k * colBuf (0-based, no bucket element)

    for (decltype(n) j = 0; j < n; j++) {
        // Accumulate phiSum[:,j]
        for (decltype(n) i = 0; i < n; i++) colBuf[i] = 0.0;
        for (size_t si = 0; si < asc.size(); si++) {
            DenseMatrix<double>* src = (si == 0) ? &phiSnap : &phiHist_[si - 1];
            auto src_col = src->column(j);
            for (decltype(n) i = 0; i < n; i++) colBuf[i] += asc[si] * src_col[i];
        }

        // RHS[:,j] = C_k * phiSum[:,j]
        // Column j of column-major phiRhs starts at offset j*n.
        double* rhs_col = phiRhs.data().data() + static_cast<size_t>(j) * n;
        if (!scratchC_.product(colBuf.data(), rhs_col)) {
            Simulator::err() << "PssTranCore: C*v product failed at t="
                             << tSolve << ", column " << j << "\n";
            return false;
        }
    }

    // ----------------------------------------------------------------
    // 6. Block solve: lastAlr_ * PhiT_new = phiRhs (all n columns).
    //
    //    klu_solve accepts nrhs > 1 with the RHS stored column-major
    //    (ldim = n).  After the call phiRhs contains the solution.
    // ----------------------------------------------------------------
    if (!lastAlr_.solveBlock(phiRhs.data().data(), static_cast<Int>(n))) {
        Simulator::err() << "PssTranCore: block Alr solve failed at t="
                         << tSolve << "\n";
        return false;
    }
    phiCurrent_ = phiRhs;   // phiRhs now holds PhiT at this step

    // ----------------------------------------------------------------
    // 7. Rotate phiHist (circular buffer, depth always >= 1).
    //
    //    phiSnap (= Phi before this step) becomes phiHist_[0].
    //    Trim to keepDepth so the buffer never exceeds p entries.
    //    Keeping at least 1 entry ensures a future BDF order ramp-up
    //    (e.g. BDF-1 → BDF-2) finds the real Phi_{k-1} rather than an
    //    identity placeholder.
    // ----------------------------------------------------------------
    phiHist_.push_front(std::move(phiSnap));
    size_t keepDepth = (asc.size() > 0) ? asc.size() - 1 : 0;
    if (keepDepth < 1) keepDepth = 1;
    while (phiHist_.size() > keepDepth) phiHist_.pop_back();

    phiValid_ = true;
    return true;
}


// ----------------------------------------------------------------
// integrateSensitivity
// ----------------------------------------------------------------

bool PssTranCore::integrateSensitivity(
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
    {
        std::stringstream ss;
        ss << std::scientific << std::setprecision(4);
        ss << "PSS: lastAlpha_= " << lastAlpha_ << "\n";
        ss << "\tx_laststep=[";
        for (decltype(n) i = 0; i < n; i++) ss << x_laststep[i] << " ";
        ss << "]\n";
        Simulator::dbg() << ss.str();
    }

    PsiT.resize(n + 1);
    PsiT[0] = 0.0;
    for (decltype(n) i = 0; i < n; i++) {
        PsiT[i + 1] = lastAlpha_ * x_laststep[i];
    }

    {
        std::stringstream ss;
        ss << std::scientific << std::setprecision(4);
        ss << "PSS: PsiT=[ ";
        for (decltype(n) i = 0; i < n; i++) ss << PsiT[i+1] << " ";
        ss << "]\n";
        Simulator::dbg() << ss.str();
    }

    return true;
}

} // namespace NAMESPACE
