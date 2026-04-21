#ifndef __COREPSSTRAN_DEFINED
#define __COREPSSTRAN_DEFINED

// corepsstran.h
//
// PssTranCore - TranCore subclass for PSS-SHOOT inline sensitivity integration.
//
// Reference:
//   T. Djurhuus and V. Krozer, "A Novel Phase-Noise Module for the
//   QUCS Circuit Simulator. Part I: the Periodic Steady-State",
//   arXiv:2512.10373v1, December 2025.
//
// The sensitivity matrix Phi(t) satisfies the linearised DAE:
//
//   C(t) * dPhi/dt + G(t) * Phi = 0,   Phi(t0) = I
//
// Discretised with the same BDF LMS scheme as the transient integrator:
//
//   Alr_k * Phi_k = C_k * sum_{i=0}^{p-1} asc[i] * Phi_{k-i-1}
//
// where Alr_k = G_k + alpha_k * C_k is the already-evaluated transient
// Jacobian (identical to the NR Jacobian at the converged timestep).
//
// Integration strategy:
//
//   onTimestepAccepted() is called by TranCore at each accepted step with
//   jacobian holding the factored Alr_k = G_k + alpha_k * C_k.
//
//   1. Alr_k: copied from jacobian.data() directly — no re-evaluation.
//
//   2. C_k: evaluated with a single evalAndLoad pass (needed for the RHS).
//
//   3. Alr_k is refactored from the copied Ax values (klu_refactor reuses
//      the symbolic factorisation stored in lastAlr_ from its rebuild()).
//
//   4. RHS[:,j] = C_k * phiSum[:,j]  for j = 0..n-1, where
//      phiSum = sum_{si} asc[si] * phiHist[si].
//
//   5. Single block solve: lastAlr_ * PhiT_new = RHS  (klu_solve, nrhs=n).
//      RHS is column-major so KLU can solve all n columns in one call.
//
//   6. Rotate phiHist (circular buffer, depth <= p).
//
// After the shoot completes integrateSensitivity() returns phiCurrent_
// as PhiT and evaluates PsiT = alpha_last * Alr_last^{-1} * g(xT).
//
// Memory:
//   phiHist: at most p n×n dense matrices = O(p * n^2) doubles.
//   lastAlr_, scratchC_: two sparse matrices matching jacobian's sparsity.
//   No per-step trajectory (G, C, pastTimesteps snapshots) is stored.

#include "coretran.h"
#include "densematrix.h"
#include "klumatrix.h"
#include "common.h"
#include <deque>

namespace NAMESPACE {

class PssTranCore : public TranCore {
public:
    PssTranCore(
        OutputDescriptorResolver& parentResolver,
        TranParameters& params,
        OperatingPointCore& opCore,
        Circuit& circuit,
        CommonData& commons,
        KluRealMatrix& jacobian,
        VectorRepository<double>& solution,
        VectorRepository<double>& states
    );

    // Hides TranCore::rebuild().  Calls TranCore::rebuild() then rebuilds
    // lastAlr_ and scratchC_ with the circuit sparsity pattern.
    bool rebuild(Status& s = Status::ignore);

    // Reset the sensitivity state before a new shooting iteration.
    // Sets phiCurrent_ = I and clears phiHist_.
    // Must be called by PssCore before each run().
    void clearTrajectory();

    // Populate preprocessedIc with all circuit unknowns from x0 so that
    // TranCore's UIC branch (nrSolver.setForces then solution = unknownValue_)
    // starts the shoot from x0 rather than zeros.
    // Must be called before run().
    void setShootIC(const Vector<double>& x0);

    // Return the inline-computed PhiT (= phiCurrent_) and evaluate PsiT.
    // Must be called immediately after run() completes, while the circuit
    // state still holds xT and lastAlr_ holds the factored Alr at xT.
    bool integrateSensitivity(
        DenseMatrix<double>& PhiT,
        Vector<double>&      PsiT,
        Vector<double>&      x_laststep
    );

protected:
    // Called by TranCore at every accepted timestep with jacobian holding
    // the factored Alr_k = G_k + alpha_k * C_k from the NR solve.
    // Evaluates C_k, advances phiCurrent_ through one BDF LMS step using
    // a block solve, and rotates phiHist_.
    virtual bool onTimestepAccepted(double tSolve, double hk, Int order);

private:
    // Current sensitivity matrix Phi(t), column-major n×n.
    // Initialised to I in clearTrajectory(), updated at every accepted step.
    // Equals PhiT after the shoot completes.
    DenseMatrix<double> phiCurrent_;

    // Circular history of past Phi matrices, depth <= p (BDF order).
    // phiHist_[0] = Phi at the previous accepted step, etc.
    // Stored column-major to match phiCurrent_.
    std::deque<DenseMatrix<double>> phiHist_;

    // Factored Alr = G + alpha*C from the most-recent accepted step.
    // Rebuilt in onTimestepAccepted(), reused in integrateSensitivity()
    // for the PsiT computation.  Same sparsity as jacobian.
    KluRealMatrix lastAlr_;

    // Scratch matrix for the unscaled reactive Jacobian C_k.
    // Filled by a single evalAndLoad pass in onTimestepAccepted().
    // Used to form the Phi RHS columns.  Same sparsity as jacobian.
    KluRealMatrix scratchC_;

    // Leading LMS coefficient alpha from the most-recent accepted step.
    double lastAlpha_;

    // True once onTimestepAccepted() has successfully processed at least
    // one step after the last clearTrajectory() call.
    bool phiValid_;
};

} // namespace NAMESPACE

#endif // __COREPSSTRAN_DEFINED
