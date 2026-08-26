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
    VectorRepository<double>& opSolution,
    VectorRepository<double>& solution,
    VectorRepository<double>& states
) : TranCore(parentResolver, params, opCore, circuit, commons,
             jacobian, opSolution, solution, states),
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

    // Size the q/qdot/Phi ring buffers for the worst-case order: they need
    // up to 'order' past points plus the current one (order+1 slots), and
    // onTimestepAccepted() additionally needs a distinct "future" slot to
    // write the new point into before promoting it - so capacity is
    // order+2, matching the same "+2" margin TranCore uses when sizing its
    // own solution/states repositories for the future/current/past window.
    auto maxOrder = circuit.simulatorOptions().core().tran_maxord;
    qHist_.upsize(maxOrder + 2, n + 1);
    qDotHist_.upsize(maxOrder + 2, n + 1);
    cHistData_.upsize(maxOrder + 2, nnz);
    gHistData_.upsize(maxOrder + 2, nnz);

    // phiHist_ is a plain CircularBuffer<DenseMatrix<double>>, not a
    // VectorRepository, so upsize() only grows the slot array - each
    // matrix must be sized to n x n explicitly (once; upsize() moves
    // existing slots rather than reallocating them, so this is safe to
    // call again on a later rebuild() with a larger n too).
    phiHist_.upsize(maxOrder + 2);
    for (decltype(phiHist_.size()) i = 0; i < phiHist_.size(); i++) {
        phiHist_.at(i).resize(n, n, DenseMatrix<double>::Major::Column);
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

bool PssTranCore::clearTrajectory() {
    phiValid_  = false;

    auto n = circuit.unknownCount();

    // Reset every phiHist_ slot to Identity - Phi(t0) = I exactly, not a
    // placeholder (see corepsstran.h). Looping at(0)..at(size-1) already
    // covers the future slot too: at(size-1) and at(-1) are the same
    // physical slot (walking size-1 steps into the past wraps around to
    // exactly one step into the future), so no separate future-slot reset
    // is needed here.
    for (decltype(phiHist_.size()) i = 0; i < phiHist_.size(); i++) {
        phiHist_.at(i).identity();
    }

    prevCValid_ = false;

    // Reset the C/G/q/qdot ring buffers: zero every slot. Same at(size-1)==
    // at(-1) identity as phiHist_ above means this already covers the
    // future slot too - no separate zeroFuture() needed.
    for (decltype(qHist_.size()) i = 0; i < qHist_.size(); i++) {
        cHistData_.zero(i);
        gHistData_.zero(i);
        qHist_.zero(i);
        qDotHist_.zero(i);
    }

    psiCurrent_.assign(n, 0.0);

    firstStepX_.clear();
    firstStepH_ = 0.0;

    rhs_colbuf.resize(n);

    // Get C_0 and q_0 by evaluating the reactive jacobian and residual at
    // this point. q_0 is written directly into qHist_'s own future slot,
    // then promoted - no separate temporary. qdot_0 is left at zero
    // (unavailable at t=0, no step has been taken yet - same reasoning as
    // the G_0 zero-seed below; confirmed harmless empirically, since qdot_0
    // is never actually read by the recursion before it's overwritten).
    jacobian.zero();
    EvalSetup es = solver().evalSetup();
    es.evaluateResistiveJacobian = false;
    es.evaluateReactiveJacobian  = true;
    es.evaluateResistiveResidual = false;
    es.evaluateReactiveResidual  = true;
    es.storeReactiveState        = true;
    es.evaluateOutvars           = false;
    es.allowBypass               = false;
    // OpenVAF-Reloaded generated idt(a,b) computes the correct reactive residual only
    // when EnableIntegration is true, which requires icEnabled false here
    // (see the analogous fix in coretran.cpp's esInit).
    es.icEnabled                 = false;

    LoadSetup ls;
    ls.loadReactiveJacobian   = true;
    ls.reactiveJacobianFactor = 1.0;
    ls.reactiveResidual       = qHist_.futureData();   // future slot already zeroed above

    if (!circuit.evalAndLoad(commons, &es, &ls, nullptr)) {
        setError(PssTranError::EvalCFailed);
        pssErrorTime = 0;
        return false;
    }
    qHist_.advance();

    // Snapshot C_0 directly into cHistData_'s future slot, then promote -
    // no separate temporary (plain std::copy, not an accumulating load
    // target, so no zeroing needed first).
    std::copy(jacobian.data(), jacobian.data() + jacobian.nnz(), cHistData_.futureData());
    cHistData_.advance();
    // Seed G_0 with zeros — G_0 unavailable at t=0 (no NR solve yet).
    // Already zeroed by the full-buffer reset above; just promote it.
    gHistData_.advance();

    captureTrajectory_= false;

    return true;
}


// ----------------------------------------------------------------
// onTimestepAccepted — inline Phi advancement
// ----------------------------------------------------------------
bool PssTranCore::onTimestepAccepted(double tSolve, double hk, Int order) {
    // Read-only for the whole function - every use below (leadingCoeff(),
    // a(), b(), b1(), aScaled(), bScaled() via differentiate()) only reads
    // the coefficients TranCore already computed for this step, so a
    // reference avoids a copy on every accepted step. (computePsiT() is the
    // only place that ever needs a mutable copy, made once, after the shoot.)
    const IntegratorCoeffs& integCoeffs = getIntegCoeffs();
    auto                    n           = circuit.unknownCount();
    auto                    nnz         = jacobian.nnz();

    // First accepted step since clearTrajectory(): capture x1 and h0 for
    // PssCore's phase-vector estimate alpha ~= (x1-x0)/h0 (pss.md, "Choosing alpha").
    // phiValid_ is still false here; it is only set true at the end of this
    // function, so !phiValid_ identifies the first call after a reset.
    if (!phiValid_) {
        firstStepH_ = hk;
        firstStepX_ = solution.vector();
    }

    // Get Alr = G_k + alpha_k * C_k from the factored NR jacobian
    std::copy(jacobian.data(), jacobian.data() + nnz, lastAlr_.data());
    if (!lastAlr_.refactor()) {
        setError(PssTranError::AlrFactorizationFailed);
        pssErrorTime = tSolve;
        return false;
    }

    // Get the current C_k and q(x_k) by using evalAndLoad. C_k will be saved
    // to jacobian; q(x_k) is written directly into qHist_'s own future slot
    // (promoted to current further down, after qdot_k is derived from it) -
    // no separate temporary.
    jacobian.zero();
    EvalSetup es = solver().evalSetup();
    es.evaluateResistiveJacobian = false;
    es.evaluateReactiveJacobian  = true;
    es.evaluateResistiveResidual = false;
    es.evaluateReactiveResidual  = true;
    es.storeReactiveState        = true;
    es.evaluateOutvars           = false;
    es.allowBypass               = false;
    // See the C_0/q_0 block above: icEnabled must be false for OpenVAF's
    // idt(a,b) to compute the correct reactive residual here too.
    es.icEnabled                 = false;

    LoadSetup ls;
    ls.loadReactiveJacobian   = true;
    ls.reactiveJacobianFactor = 1.0;
    qHist_.zeroFuture();   // load targets accumulate (+=); the ring slot may hold stale data from size_ steps ago
    ls.reactiveResidual       = qHist_.futureData();

    if (!circuit.evalAndLoad(commons, &es, &ls, nullptr)) {
        setError(PssTranError::EvalCFailed);
        pssErrorTime = tSolve;
        return false;
    }
    // Snapshot current C_k directly into cHistData_'s future slot - no
    // separate temporary (plain std::copy, not an accumulating load target).
    std::copy(jacobian.data(), jacobian.data() + nnz, cHistData_.futureData());
    const Vector<double>& cSnap = cHistData_.futureVector();

    double alpha = integCoeffs.leadingCoeff();
    const Vector<double>& a = integCoeffs.a();
    const Vector<double>& b = integCoeffs.b();

    // G_k = A_k - alpha_k * C_k (lastAlr_.data() holds A_k; not modified by
    // refactor), written directly into gHistData_'s future slot.
    Vector<double>& gSnap = gHistData_.futureVector();
    for (Int j = 0; j < nnz; j++)
        gSnap[j] = lastAlr_.data()[j] - alpha * cSnap[j];

    // gammaC_[p] = alpha * a[p]    — coefficient for the C term
    // gammaG_[p] = -(b[p] / b1)   — coefficient for the G term (zero for BDF)
    // Resized (not reallocated when order is unchanged from the last step,
    // the common case) rather than freshly constructed every call.
    gammaC_.assign(order, 0.0);
    gammaG_.assign(order, 0.0);
    for (int p = 0; !a.empty() && p < std::min(order, (Int)a.size()); p++)
        gammaC_[p] = alpha * a[p];
    for (int p = 0; !b.empty() && p < std::min(order, (Int)b.size()); p++)
        gammaG_[p] = -(b[p] / integCoeffs.b1());

    // Retain this step's size - computePsiT() needs h_{N-1} for whichever
    // step turns out to be the shoot's last. The coefficients themselves
    // don't need saving here: TranCore's own integCoeffs member stays valid
    // for this same step until the next run(), so computePsiT() reads it
    // fresh via getIntegCoeffs() when it actually needs a mutable copy.
    lastStepH_ = hk;

    // Build right-hand side: sum_p (gammaC[p]*C_{k-p} + gammaG[p]*G_{k-p}) * Phi_{k-p}
    // Accumulated directly into phiHist_'s future slot (distinct from every
    // at(p) read below, p=0..order-1 - see corepsstran.h), so it can be
    // solved in place and then advance()d straight into the current slot,
    // with no separate phiCurrent_ member and no snapshot-and-push.
    DenseMatrix<double>& phiFuture = phiHist_.at(-1);
    phiFuture.zero();
    VectorView rhsColBufView(rhs_colbuf);
    for (int p = 0; p < order; p++) {
        DenseMatrix<double>& Phi_kmi = phiHist_.at(p);

        // C_{k-p} * Phi_{k-p} contribution
        std::copy(cHistData_.at(p).begin(), cHistData_.at(p).end(), scratchC_.data());
        for (decltype(n) j = 0; j < n; j++) {
            auto rhs_col = phiFuture.column(j);
            if (!scratchC_.product(Phi_kmi.column(j), rhs_colbuf)) {
                setError(PssTranError::CPhiProductFailed);
                pssErrorTime = tSolve;
                pssErrorColumn = j;
                return false;
            }
            rhs_col.addScaled(rhsColBufView, gammaC_[p]);
        }

        // G_{k-p} * Phi_{k-p} contribution (AM methods only; gammaG_[p]==0 for BDF)
        if (gammaG_[p] != 0.0) {
            std::copy(gHistData_.at(p).begin(), gHistData_.at(p).end(), scratchC_.data());
            for (decltype(n) j = 0; j < n; j++) {
                auto rhs_col = phiFuture.column(j);
                if (!scratchC_.product(Phi_kmi.column(j), rhs_colbuf)) {
                    setError(PssTranError::GPhiProductFailed);
                    pssErrorTime = tSolve;
                    pssErrorColumn = j;
                    return false;
                }
                rhs_col.addScaled(rhsColBufView, gammaG_[p]);
            }
        }
    }

    // Solve for Phi_k
    // Alr * Phi_k = sum_{i=1}^order (gamma_i * C_k-i * Phi_k-i)
    if (!lastAlr_.solveBlock(phiFuture.data().data(), static_cast<Int>(n))) {
        setError(PssTranError::BlockAlrSolveFailed);
        pssErrorTime = tSolve;
        return false;
    }
    phiHist_.advance();   // phiHist_.at(0) now holds PhiT at this step

    // qdot_{k+1} via the standard LMS derivative-form reconstruction
    // (numint.md, "Derivative at the new timepoint"): needed only for the
    // final, last-step Psi_T formula (pss.md, "Computing Psi_T"), evaluated
    // once after the whole shoot finishes by computePsiT(). q_{k+1} was
    // already written directly into qHist_'s future slot above (the
    // evalAndLoad target); qdot_{k+1} is written directly into qDotHist_'s
    // future slot here - no temporaries either way. Both ring buffers are
    // promoted (old future becomes new current) right after.
    integCoeffs.differentiate(qHist_, qDotHist_, qDotHist_.futureVector());
    qHist_.advance();
    qDotHist_.advance();

    // If trajectory capture is enabled, store data needed for adjoint
    // integration. rec.cData/gData copy cSnap/gSnap (references into the
    // ring buffers' future slots); StepRecord needs its own independent
    // copy regardless, since those slots get reused later in the shoot.
    if (captureTrajectory_) {
        StepRecord rec;
        rec.aData  = Vector<double>(lastAlr_.data(), lastAlr_.data() + nnz);
        rec.cData  = cSnap;
        rec.gData  = gSnap;
        rec.gammaC = gammaC_;
        rec.gammaG = gammaG_;
        rec.order  = order;
        trajectory_.push_back(std::move(rec));
    }

    // Phi/C_k/G_k/q_k/qdot_k were all built directly into their ring
    // buffers' future slots above; promote C_k/G_k now (the others were
    // already advance()d where they were produced).
    cHistData_.advance();
    gHistData_.advance();

    phiValid_ = true;
    return true;
}

// ----------------------------------------------------------------
// integrateAdjointMonodromy
// ----------------------------------------------------------------
bool PssTranCore::integrateAdjointMonodromy(DenseMatrix<double>& Omega){
    if (trajectory_.empty()) {
        setError(PssTranError::NoTrajectory);
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
        setError(PssTranError::ScratchRebuild);
        return false;
    }
    if (!scratchC.rebuild(circuit.sparsityMap(), n)) {
        setError(PssTranError::ScratchRebuild);
        return false;
    }
    if (!scratchG.rebuild(circuit.sparsityMap(), n)) {
        setError(PssTranError::ScratchRebuild);
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

        // Load A_k into scratchA and refactor for tsolve
        std::copy(acRec.aData.begin(), acRec.aData.end(), scratchA.data());
        if (!scratchA.refactor()) {
            setError(PssTranError::ScratchRefactorFailed);
            pssErrorIndexK = k;
            return false;
        }

        // Build RHS: rhs[:,j] = sum_i [ C_{k+i}^T * gammaC_i * Omega_{k+i}[:,j]
        //                             + G_{k+i}^T * gammaG_i * Omega_{k+i}[:,j] ]
        // Each history slot i uses its own C and G matrices from trajectory_[k+i].
        DenseMatrix<double> rhs(n, n, DenseMatrix<double>::Major::Column);
        Vector<double> om_col(n);
        Vector<double> rhs_col(n);

        Int histSize = static_cast<Int>(omegaHist.size());
        for (Int i = 0; i < histSize; i++) {
            if (k + i >= nSteps) break;
            const StepRecord& futureRec = trajectory_[k + i];
            if (i >= futureRec.order) continue;

            double gC = (i < (Int)futureRec.gammaC.size()) ? futureRec.gammaC[i] : 0.0;
            double gG = (i < (Int)futureRec.gammaG.size()) ? futureRec.gammaG[i] : 0.0;

            DenseMatrix<double>& Om = omegaHist[i];

            if (gC != 0.0)
                std::copy(futureRec.cData.begin(), futureRec.cData.end(), scratchC.data());
            if (gG != 0.0)
                std::copy(futureRec.gData.begin(), futureRec.gData.end(), scratchG.data());

            for (decltype(n) j = 0; j < n; j++) {
                for (decltype(n) row = 0; row < n; row++) om_col[row] = Om.at(row, j);
                if (gC != 0.0) {
                    if (!scratchC.tproduct(om_col.data(), rhs_col.data())) {
                        setError(PssTranError::CTProductFailed);
                        pssErrorIndexK = k;
                        pssErrorIndexI = i;
                        return false;
                    }
                    for (decltype(n) row = 0; row < n; row++) rhs.at(row, j) += gC * rhs_col[row];
                }
                if (gG != 0.0) {
                    if (!scratchG.tproduct(om_col.data(), rhs_col.data())) {
                        setError(PssTranError::GTProductFailed);
                        pssErrorIndexK = k;
                        pssErrorIndexI = i;
                        return false;
                    }
                    for (decltype(n) row = 0; row < n; row++) rhs.at(row, j) += gG * rhs_col[row];
                }
            }
        }

        // Solve A_k^T * Omega_k = rhs (overwrites rhs with solution)
        if (!scratchA.tsolveBlock(rhs.data().data(), static_cast<Int>(n))) {
            setError(PssTranError::TSolveBlockFailed);
            pssErrorIndexK = k;
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
// computePsiT
// ----------------------------------------------------------------
bool PssTranCore::computePsiT() {
    auto n = circuit.unknownCount();
    psiCurrent_.assign(n, 0.0);

    if (!phiValid_) {
        setError(PssTranError::NoAcceptedSteps);
        return false;
    }

    // Coefficient sensitivities to h_{N-1} (the last step's size), using the
    // generic any-order/any-method formulae of numint.md - not the BDF1/
    // trapezoidal closed forms pss.md derives only as illustrative special
    // cases. A fresh copy of TranCore's own integCoeffs is made here, once,
    // because computeSensitivities()/scaleDifferentiatorSensitivities()
    // mutate it and getIntegCoeffs() only hands out a const reference; the
    // live object itself stays valid and unchanged for this same step
    // until the next run(), so nothing here depends on the (possibly
    // since-mutated) live transient timestep history.
    IntegratorCoeffs ic = getIntegCoeffs();
    if (!ic.computeSensitivities(lastStepH_) || !ic.scaleDifferentiatorSensitivities(lastStepH_)) {
        setError(PssTranError::PsiSensitivityFailed);
        return false;
    }

    const auto& aSens   = ic.aScaledSens();
    const auto& bSens   = ic.bScaledSens();
    double      aM1Sens = ic.leadingCoeffSens();

    // d(qdot_N)/d(h_{N-1}) at fixed x_N (pss.md, "Computing Psi_T"), expressed
    // in terms of the codebase's own scaled coefficients: aScaledSens_/
    // bScaledSens_ already are d(aScaled_)/dh_{N-1}, d(bScaled_)/dh_{N-1}
    // (verified by scaleDifferentiatorSensitivities()'s own finite-difference
    // self-test, checkDiffSens() in coretrancoef.cpp), so no separate
    // un-differentiated bScaled term or extra h_{N-1} factor belongs here:
    //   leading_'*q_N + sum_i aScaled_'[i]*q_{N-1-i} + sum_i bScaled_'[i]*qdot_{N-1-i}
    // qHist_.at(0) / qDotHist_.at(0) are q_N / qdot_N; at(p+1) is
    // q_{N-1-p} / qdot_{N-1-p} - exactly the indexing the formula needs.
    psiTrhs_.assign(n, 0.0);
    VectorView(psiTrhs_).scaledVector(VectorView(qHist_.at(0), 1, n, 1), aM1Sens);
    for (Int p = 0; p < (Int)aSens.size(); p++) {
        VectorView(psiTrhs_).addScaled(VectorView(qHist_.at(p + 1), 1, n, 1), aSens[p]);
    }
    for (Int p = 0; p < (Int)bSens.size(); p++) {
        VectorView(psiTrhs_).addScaled(VectorView(qDotHist_.at(p + 1), 1, n, 1), bSens[p]);
    }

    // Psi_T = -J_N^{-1} * d(qdot_N)/d(h_{N-1})   (J_N = lastAlr_, already
    // factored from the last accepted step's Newton solve)
    if (!lastAlr_.solve(psiTrhs_.data())) {
        setError(PssTranError::PsiSolveFailed);
        return false;
    }
    VectorView(psiCurrent_).scaledVector(VectorView(psiTrhs_), -1.0);

    return true;
}

// ----------------------------------------------------------------
// PSS rawfile output
// ----------------------------------------------------------------

// Override to change the output file name
bool PssTranCore::initializeOutputs(Id name, Status& s) {
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    if (!outfile) {
        outfile = new OutputRawfile(
            name, outputSources,
            (circuit.simulatorOptions().core().rawfile==SimulatorOptions::rawfileBinary ? OutputRawfile::Flags::Binary : OutputRawfile::Flags::None) |
                OutputRawfile::Flags::Padded);
        outfile->setTitle(circuit.title());
        auto& options = circuit.simulatorOptions().core();
        auto method = std::string(options.tran_method);
        if (
            options.tran_method==TranCore::methodGear || 
            options.tran_method==TranCore::methodBDF ||
            options.tran_method==TranCore::methodAM
        ) {
            method += std::to_string(options.tran_maxord);
        }
        outfile->setPlotname("Periodic Steady State " + method);
    }
    outfile->prologue(s);
    return true;
}

bool PssTranCore::formatError(Status& s) const {
    // First, handle TranCore and AnalysisCore errors
    if (lastTranError!=TranCore::TranError::OK || lastError!=Error::OK) {
        TranCore::formatError(s);
        return false;
    }
    
    // Then handle PssTranCore errors
    switch (lastPssTranError) {
        case PssTranError::EvalCFailed:
            s.set(Status::Analysis, "PssTranCore: evalAndLoad(C) failed at t="+std::to_string(pssErrorTime)+".");
            break;
        case PssTranError::AlrFactorizationFailed:
            s.set(Status::Analysis, "PssTranCore: Alr refactorisation failed at t="+std::to_string(pssErrorTime)+".");
            break;
        case PssTranError::CPhiProductFailed:
            s.set(Status::Analysis, 
                "PssTranCore: C*Phi product failed at t="+std::to_string(pssErrorTime)+
                ", column "+std::to_string(pssErrorColumn)+"."
            );
            break;   
        case PssTranError::GPhiProductFailed:
            s.set(Status::Analysis, 
                "PssTranCore: G*Phi product failed at t="+std::to_string(pssErrorTime)+
                ", column "+std::to_string(pssErrorColumn)+"."
            );
            break;
        case PssTranError::BlockAlrSolveFailed:
            s.set(Status::Analysis, "PssTranCore: block Alr solve failed at t="+std::to_string(pssErrorTime)+".");
            break;
        case PssTranError::PsiSensitivityFailed:
            s.set(Status::Analysis, "PssTranCore: Psi_T coefficient sensitivity computation failed.");
            break;
        case PssTranError::PsiSolveFailed:
            s.set(Status::Analysis, "PssTranCore: Psi_T solve failed.");
            break;
        case PssTranError::NoAcceptedSteps:
            s.set(Status::Analysis, "PssTranCore: no accepted steps since clearTrajectory(). PhiT is not available.");
            break;
        case PssTranError::NoTrajectory:
            s.set(Status::Analysis, "PssTranCore: no trajectory captured. Call enableTrajectoryCapture() before the final shoot.");
            break;
        case PssTranError::ScratchRebuild:
            s.set(Status::Analysis, "PssTranCore: failed to rebuild scratch matrix for Omega integration.");
            break;
        case PssTranError::ScratchRefactorFailed:
            s.set(Status::Analysis, "PssTranCore: scratchA refactor failed at backward step k="+std::to_string(pssErrorIndexK)+".");
            break;
        case PssTranError::CTProductFailed:
            s.set(Status::Analysis, 
                "PssTranCore: C^T product failed at backward step k="+std::to_string(pssErrorIndexK)+
                " i="+std::to_string(pssErrorIndexI)+"."
            );
            break;
        case PssTranError::GTProductFailed:
            s.set(Status::Analysis, 
                "PssTranCore: G^T product failed at backward step k="+std::to_string(pssErrorIndexK)+
                " i="+std::to_string(pssErrorIndexI)+"."
            );
            break;
        
        case PssTranError::TSolveBlockFailed:
            s.set(Status::Analysis, 
                "PssTranCore: tsolveBlock failed at backward step k="+std::to_string(pssErrorIndexK)+"."
            );
            break;
        default:
            return true;
    }
    return false;
}


} // namespace NAMESPACE
