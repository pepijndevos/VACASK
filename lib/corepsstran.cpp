// corepsstran.cpp
//
// PssTranCore implementation.
// See corepsstran.h for design description and corepss.h for the
// algorithm context.

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
             jacobian, solution, states) {
}


// ----------------------------------------------------------------
// Trajectory management
// ----------------------------------------------------------------

void PssTranCore::clearTrajectory() {
    trajectory_.clear();
}


// ----------------------------------------------------------------
// Hook called at every accepted timestep
// ----------------------------------------------------------------

bool PssTranCore::onTimestepAccepted(double tSolve, double hk, Int order) {
    // The NR solve has converged. solution.at(0) holds xs(tk+1).
    // Re-evaluate the circuit to extract G(t) and C(t) separately
    // into the new trajectory point.

    PssTrajectoryPoint pt;
    pt.t     = tSolve;
    pt.hk    = hk;
    pt.order = order;

    if (!collectGC(pt)) {
        return false;
    }

    // Snapshot pastTimesteps so integrateSensitivity() can replay the
    // LMS coefficients for this step exactly.
    snapshotPastTimesteps(pt.pastTimestepsSnapshot);

    trajectory_.push_back(std::move(pt));
    return true;
}


// ----------------------------------------------------------------
// Collect G(t) and C(t) at the current converged circuit state
// ----------------------------------------------------------------

bool PssTranCore::collectGC(PssTrajectoryPoint& pt) {
    // Allocate and rebuild pt.G and pt.C with the circuit sparsity pattern.
    // This is the only rebuild needed - sparsity is fixed for the
    // entire analysis.
    pt.G = std::make_unique<KluRealMatrix>();
    if (!pt.G->rebuild(circuit.sparsityMap(), circuit.unknownCount())) {
        Simulator::err() << "PssTranCore: failed to rebuild G matrix.\n";
        return false;
    }
    pt.C = std::make_unique<KluRealMatrix>();
    if (!pt.C->rebuild(circuit.sparsityMap(), circuit.unknownCount())) {
        Simulator::err() << "PssTranCore: failed to rebuild C matrix.\n";
        return false;
    }

    // evalSetup_ from nrSolver already points solution and states at
    // the current converged state. Copy it and override the flags.

    // --- Resistive Jacobian G = dg/dx ---
    // Stamp into jac (using the existing circuit binding), then copy
    // the nonzero values into pt.G. The sparsity structure of pt.G
    // and jac are identical so the Ax arrays have the same layout.
    jacobian.zero();
    {
        EvalSetup esG = getNrSolver().evalSetup();
        esG.evaluateResistiveJacobian = true;
        esG.evaluateReactiveJacobian  = false;
        esG.evaluateResistiveResidual = false;
        esG.evaluateReactiveResidual  = false;
        esG.evaluateOutvars           = false;
        esG.allowBypass               = false;

        LoadSetup lsG;
        lsG.loadResistiveJacobian = true;

        if (!circuit.evalAndLoad(commons, &esG, &lsG, nullptr)) {
            return false;
        }

        // Copy Ax values from jacobian into pt.G.
        // AP and AI are identical in both matrices (same sparsity rebuild),
        // so entry k in jacobian.data() corresponds to entry k in pt.G.data().
        auto nnz = jacobian.nnz();
        std::copy(jacobian.data(), jacobian.data() + nnz, pt.G->data());
    }

    // --- Reactive Jacobian C = dq/dx (unscaled, not alpha * C) ---
    jacobian.zero();
    {
        EvalSetup esC = getNrSolver().evalSetup();
        esC.evaluateResistiveJacobian = false;
        esC.evaluateReactiveJacobian  = true;
        esC.evaluateResistiveResidual = false;
        esC.evaluateReactiveResidual  = false;
        esC.evaluateOutvars           = false;
        esC.allowBypass               = false;

        LoadSetup lsC;
        lsC.loadReactiveJacobian   = true;
        lsC.reactiveJacobianFactor = 1.0;

        if (!circuit.evalAndLoad(commons, &esC, &lsC, nullptr)) {
            return false;
        }

        auto nnz = jacobian.nnz();
        std::copy(jacobian.data(), jacobian.data() + nnz, pt.C->data());
    }

    // Leave jacobian zeroed. TranCore rebuilds it at the next NR iteration.
    jacobian.zero();
    return true; 
}


// ----------------------------------------------------------------
// Snapshot pastTimesteps for LMS replay
// ----------------------------------------------------------------

void PssTranCore::snapshotPastTimesteps(std::vector<double>& snapshot) {
    // After pastTimesteps.add(hk), at(0)=hk, at(1)=h_{k-1}, ...
    // snapshot[0] = hk (= pt.hk, passed as newStep to compute()).
    // snapshot[1..] = h_{k-1}, h_{k-2}, ... (past steps for the replay).
    // See the replay loop in integrateSensitivity() for how they are used.
    auto needed = getIntegCoeffs().pastStatesNeeded();
    snapshot.resize(needed);
    for (decltype(needed) i = 0; i < needed; i++) {
        snapshot[i] = getPastTimesteps().at(i);
    }
}


// ----------------------------------------------------------------
// Sensitivity matrix integration
// ----------------------------------------------------------------

bool PssTranCore::integrateSensitivity(
    DenseMatrix<double>& PhiT,
    Vector<double>&      PsiT
) {
    if (trajectory_.empty()) {
        Simulator::err() << "PssTranCore: trajectory is empty, cannot integrate sensitivity.\n";
        return false;
    }

    auto n = circuit.unknownCount();

    // PhiT starts as the n x n identity: PhiT(t0) = I.
    // Column j of PhiT represents the response to a unit perturbation
    // in direction ej at t0.
    PhiT.resize(n, n);
    for (decltype(n) i = 0; i < n; i++) {
        for (decltype(n) j = 0; j < n; j++) {
            PhiT.at(i, j) = (i == j) ? 1.0 : 0.0;
        }
    }

    // Alr = G + alpha * C at each step.
    // Rebuilt from stored trajectory snapshots, never via evalAndLoad.
    KluRealMatrix Alr;
    if (!Alr.rebuild(circuit.sparsityMap(), n)) {
        Simulator::err() << "PssTranCore: failed to rebuild Alr matrix.\n";
        return false;
    }

    // Working vectors for the sparse matrix-vector product and the solve.
    // col_buf: flat copy of a PhiT column for product() — DenseMatrix columns
    //   are strided in row-major layout and cannot be passed directly as a
    //   contiguous array pointer.
    // Cv:      result of C * col_buf, 0-based, no bucket.
    // rhs:     RHS for Alr solve, 1-based, with bucket.
    Vector<double> col_buf(n);  // contiguous copy of one PhiT column
    Vector<double> Cv(n);       // result of C * col_buf, 0-based, no bucket
    Vector<double> rhs(n + 1);  // RHS for Alr solve, 1-based, with bucket

    // alphaLast is captured each iteration so it is available after the loop
    // for the PsiT solve, which reuses the final factored Alr.
    double alphaLast = 0.0;

    // History of past PhiT matrices for multi-step LMS integration.
    // phiHist[i] = PhiT at step k-i-1 (i.e. dx_{k-1-i}).
    // Extended with identity matrices during order ramp-up (the circuit
    // was at steady state before t0, so the sensitivity is the identity).
    std::deque<DenseMatrix<double>> phiHist;

    // Snapshot of PhiT (= dx_k) taken before overwriting it column by column.
    // Saved into phiHist[0] at the end of each step.
    DenseMatrix<double> PhiT_snap;

    for (const auto& pt : trajectory_) {
        // Replay integrator coefficients for this step using the stored
        // pastTimesteps snapshot to get the same alpha as the original run.
        IntegratorCoeffs replayCoeffs = getIntegCoeffs();
        replayCoeffs.setOrder(pt.order);

        // Reconstruct the past-step circular buffer from the snapshot.
        // snapshot[0] == pt.hk — passed as newStep below, not a past step.
        // snapshot[1..] == h_{k-1}, h_{k-2}, ... — add in reverse order so
        // that replaySteps.at(0) == h_{k-1} (most recent past step), matching
        // the state of pastTimesteps before pastTimesteps.add(hk) was called.
        CircularBuffer<double> replaySteps;
        auto snapSize = static_cast<Int>(pt.pastTimestepsSnapshot.size());
        replaySteps.upsize(snapSize + 1);
        for (int si = snapSize - 1; si >= 1; si--) {
            replaySteps.add(pt.pastTimestepsSnapshot[si]);
        }
        if (!replayCoeffs.compute(replaySteps, pt.hk)) {
            Simulator::err() << "PssTranCore: failed to compute integrator coefficients at t="
                             << pt.t << "\n";
            return false;
        }
        if (!replayCoeffs.scaleDifferentiator(pt.hk)) {
            Simulator::err() << "PssTranCore: failed to scale differentiator at t="
                             << pt.t << "\n";
            return false;
        }

        double alpha = replayCoeffs.leadingCoeff();
        alphaLast = alpha;

        const auto& asc = replayCoeffs.aScaled();
        const auto& bsc = replayCoeffs.bScaled();

        // Methods with past-derivative terms (e.g. trapezoidal) require an
        // xdot history that is not collected during the transient run.
        // BDF and Adams-Moulton without past derivatives are supported.
        if (!bsc.empty()) {
            Simulator::err() << "PssTranCore: sensitivity integration does not support "
                             << "LMS methods with past-derivative terms (e.g. trapezoidal) "
                             << "at t=" << pt.t << "\n";
            return false;
        }

        // Build Alr = G_k + alpha * C_k element-wise.
        // pt.G and pt.C share the same sparsity as Alr, so their
        // data() arrays are aligned entry-for-entry.
        auto nnz = Alr.nnz();
        for (decltype(nnz) k = 0; k < nnz; k++) {
            Alr.data()[k] = pt.G->data()[k] + alpha * pt.C->data()[k];
        }

        if (!Alr.factor()) {
            Simulator::err() << "PssTranCore: LR factorisation failed at t="
                             << pt.t << "\n";
            return false;
        }

        // Extend phiHist with identity matrices to cover newly needed history
        // depth during order ramp-up. asc.size()-1 past PhiT matrices are
        // needed; slots not yet present are filled with the identity.
        while (phiHist.size() + 1 < asc.size()) {
            DenseMatrix<double> id;
            id.resize(n, n);
            for (decltype(n) row = 0; row < n; row++)
                for (decltype(n) col = 0; col < n; col++)
                    id.at(row, col) = (row == col) ? 1.0 : 0.0;
            phiHist.push_back(std::move(id));
        }

        // Snapshot PhiT (= dx_k) BEFORE overwriting it column by column.
        // It will become phiHist[0] after the column updates.
        PhiT_snap = PhiT;

        // Advance each column j of PhiT through this LMS step.
        //
        // The discretised LR equation is:
        //   Alr * dx_{k+1} = C * sum_{i=0}^{p-1} asc[i] * dx_{k-i}
        //
        // where asc = aScaled_ from the replayed integrator coefficients.
        // dx_{k-0} = PhiT_snap (PhiT before this update).
        // dx_{k-i} for i >= 1 = phiHist[i-1].
        //
        // For BDF-1 (p=1): asc[0]=1, so RHS = alpha*C*dx_k (as before).
        // For BDF-2/Gear-2 (p=2): RHS = C*(asc[0]*dx_k + asc[1]*dx_{k-1}).
        for (decltype(n) j = 0; j < n; j++) {
            auto col_j = PhiT.column(j);

            // Accumulate RHS = C * sum_i asc[i] * hist[i][:, j].
            // Source matrices are row-major so columns are strided; copy each
            // to col_buf before passing to C->product() (which expects a
            // contiguous array). Write-back via col_j[i] is correct because
            // VectorView::operator[] applies the stride automatically.
            rhs[0] = 0.0;
            for (decltype(n) i = 0; i < n; i++) rhs[i + 1] = 0.0;

            for (size_t si = 0; si < asc.size(); si++) {
                DenseMatrix<double>* srcPtr =
                    (si == 0) ? &PhiT_snap : &phiHist[si - 1];
                auto src_col = srcPtr->column(j);
                for (decltype(n) i = 0; i < n; i++) col_buf[i] = src_col[i];

                if (!pt.C->product(col_buf.data(), Cv.data())) {
                    Simulator::err() << "PssTranCore: C*v product failed at t="
                                     << pt.t << ", column " << j
                                     << ", history slot " << si << "\n";
                    return false;
                }

                for (decltype(n) i = 0; i < n; i++) rhs[i + 1] += asc[si] * Cv[i];
            }

            // Solve Alr * dx_{k+1} = rhs in place.
            // dataWithoutBucket returns rhs.data() + 1, skipping the bucket.
            if (!Alr.solve(dataWithoutBucket(rhs))) {
                Simulator::err() << "PssTranCore: LR solve failed at t="
                                 << pt.t << ", column " << j << "\n";
                return false;
            }

            // Store dx_{k+1} back into column j of PhiT.
            for (decltype(n) i = 0; i < n; i++) {
                col_j[i] = rhs[i + 1];
            }
        }

        // Rotate history: PhiT_snap (dx_k) becomes phiHist[0] for the next
        // step. Keep at least 1 entry so that a future order ramp-up (e.g.
        // BDF-1 → BDF-2) finds the real dx_{k-1} rather than the identity
        // placeholder filled in by the while-loop above.
        phiHist.push_front(std::move(PhiT_snap));
        size_t keepDepth = (asc.size() > 0) ? asc.size() - 1 : 0;
        if (keepDepth < 1) keepDepth = 1;
        while (phiHist.size() > keepDepth) {
            phiHist.pop_back();
        }
    }

    // Compute PsiT = -dxT/dT0, the period sensitivity vector.
    //
    // PsiT is the negated circuit velocity at the end of the trajectory:
    //   PsiT = -xdot_s(t0 + T0)
    //
    // From the circuit equations: g(xT) + C(xT)*xdot = 0
    //   => PsiT = C(xT)^{-1} * g(xT)
    //
    // C(xT) alone is singular for purely resistive nodes (zero rows).
    // Instead we reuse Alr = G + alphaLast*C from the last trajectory step,
    // which is already factored above. The approximation:
    //
    //   PsiT = alphaLast * Alr^{-1} * g(xT)
    //
    // is exact when alphaLast*C >> G (oscillating circuits at their operating
    // frequency) and degenerates correctly to zero for purely resistive
    // circuits where g(xT) = 0 at the DC operating point.

    // Evaluate g(xT) at the current circuit state. solution.at(0) = xT
    // because integrateSensitivity() is called immediately after the shoot.
    Vector<double> gxT(n + 1, 0.0);
    {
        EvalSetup esV = getNrSolver().evalSetup();
        esV.evaluateResistiveJacobian = false;
        esV.evaluateReactiveJacobian  = false;
        esV.evaluateResistiveResidual = true;
        esV.evaluateReactiveResidual  = false;
        esV.evaluateOutvars           = false;
        esV.allowBypass               = false;

        LoadSetup lsV;
        lsV.resistiveResidual = gxT.data();

        if (!circuit.evalAndLoad(commons, &esV, &lsV, nullptr)) {
            return false;
        }
    }

    // PsiT = alphaLast * Alr^{-1} * g(xT).
    // Load the scaled g(xT) into PsiT (1-based), then solve in place.
    PsiT.resize(n + 1);
    PsiT[0] = 0.0;
    for (decltype(n) i = 0; i < n; i++) {
        PsiT[i + 1] = alphaLast * gxT[i + 1];
    }
    if (!Alr.solve(dataWithoutBucket(PsiT))) {
        Simulator::err() << "PssTranCore: PsiT solve failed.\n";
        return false;
    }

    return true;
}

} // namespace NAMESPACE
