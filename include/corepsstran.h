#ifndef __COREPSSTRAN_DEFINED
#define __COREPSSTRAN_DEFINED

// corepsstran.h
//
// PssTranCore - TranCore subclass for PSS-SHOOT sensitivity collection
//
// Reference:
//   T. Djurhuus and V. Krozer, "A Novel Phase-Noise Module for the
//   QUCS Circuit Simulator. Part I: the Periodic Steady-State",
//   arXiv:2512.10373v1, December 2025.
//
// PssTranCore extends TranCore with two capabilities required by the
// PSS-SHOOT algorithm:
//
//   1. onTimestepAccepted()
//
//      Called at every accepted timestep by TranCore::coroutine() via
//      a virtual hook (to be added to TranCore as described in the port
//      assessment). At that point the NR solve has converged and the
//      circuit is at xs(tk). This method re-evaluates the circuit twice
//      with separate LoadSetup flags to obtain the resistive Jacobian
//      G(t) = dg/dx and the reactive Jacobian C(t) = dq/dx at the
//      converged state. Both matrices are stored in trajectory_.
//
//   2. integrateSensitivity()
//
//      Walks trajectory_ after a complete shoot and integrates the
//      linearised response equations
//
//         d/dt [C(t) dx(t)] + G(t) dx(t) = 0
//
//      using the same LMS method, timestep mesh, and order history as
//      the original nonlinear integration. This produces the n x n
//      sensitivity matrix PhiT = dxT/dx0. The per-step integrator
//      coefficients are replayed identically by storing a snapshot of
//      the pastTimesteps circular buffer at each accepted point.
//
// Two constraints are critical for correctness:
//
//   - G(t) and C(t) must be collected AFTER NR convergence at each
//     accepted timepoint, not during NR iteration, because the NR
//     Jacobian is zeroed and rebuilt every iteration.
//
//   - integrateSensitivity() must use exactly the same integCoeffs,
//     pastTimesteps, and order as the original transient run. This
//     is why those members must be promoted to protected in TranCore.
//
// Design note: G and C at each trajectory point are stored as
// KluRealMatrix objects rather than plain value vectors. This means
// each trajectory point owns a fully structured sparse matrix with
// its own AP, AI, and Ax arrays. The cost is one KLU rebuild per
// trajectory point (paid once during collectGC()), but the benefit
// is that integrateSensitivity() can call product() directly on G
// and C without needing any additional accessors on KluMatrixCore.

#include "coretran.h"
#include "densematrix.h"
#include "klumatrix.h"
#include "common.h"

namespace NAMESPACE {

// One record per accepted timestep along the shooting trajectory.
// Stores everything needed to replay the LMS step for the LR equations.
//
// G and C are stored as KluRealMatrix objects sharing the same sparsity
// pattern as jac. This allows integrateSensitivity() to call
// G.product() and C.product() directly without any additional
// index structure.
struct PssTrajectoryPoint {
    // Time and step size at this accepted point.
    double t;
    double hk;

    // LMS integration order used at this step.
    Int order;

    // Resistive Jacobian G(t) = dg/dx at the converged solution xs(t).
    // Built by a separate evalAndLoad call with loadResistiveJacobian.
    // Has the same sparsity pattern as jac (same rebuild call).
    // Stored by pointer because KluRealMatrix is non-movable.
    std::unique_ptr<KluRealMatrix> G;

    // Reactive Jacobian C(t) = dq/dx at the converged solution xs(t).
    // Built by a separate evalAndLoad call with loadReactiveJacobian,
    // with reactiveJacobianFactor = 1.0 (unscaled, not alpha * C).
    // Has the same sparsity pattern as jac (same rebuild call).
    // Stored by pointer because KluRealMatrix is non-movable.
    std::unique_ptr<KluRealMatrix> C;

    // Snapshot of the pastTimesteps circular buffer at this point.
    // Used to replay integCoeffs.compute() identically during
    // sensitivity integration.
    std::vector<double> pastTimestepsSnapshot;

    // Move-only: unique_ptr members make copy implicitly deleted and
    // move implicitly defined. trajectory_ uses push_back(std::move(pt)).
    PssTrajectoryPoint() = default;
    PssTrajectoryPoint(const PssTrajectoryPoint&)            = delete;
    PssTrajectoryPoint& operator=(const PssTrajectoryPoint&) = delete;
    PssTrajectoryPoint(PssTrajectoryPoint&&)                 = default;
    PssTrajectoryPoint& operator=(PssTrajectoryPoint&&)      = default;
};


class PssTranCore : public TranCore {
public:
    // Constructor mirrors TranCore exactly.
    // PssCore passes the same jac, solution, and states references it owns.
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

    // Clear the stored trajectory before starting a new shoot.
    // Must be called by PssCore at the start of each Newton iteration.
    void clearTrajectory();

    // Read-only access to the stored trajectory.
    // PssCore reads this after run() returns, before calling
    // integrateSensitivity().
    const std::vector<PssTrajectoryPoint>& trajectory() const {
        return trajectory_;
    }

    // Integrate the linearised response equations along the stored
    // trajectory to produce the sensitivity matrix PhiT and the
    // period sensitivity vector PsiT.
    //
    // Must be called after run() has completed and trajectory_ is full.
    //
    // PhiT (OUT): n x n state-transition matrix dxT/dx0.
    //   PssCore uses this to build the shooting Jacobian Jp = I - PhiT.
    //
    // PsiT (OUT): n x 1 vector -dxT/dT0.
    //   PssCore uses this as the extra column of the augmented Jacobian
    //   for autonomous circuits.
    //
    // Returns false if trajectory_ is empty or any integration step fails.
    bool integrateSensitivity(
        DenseMatrix<double>& PhiT,
        Vector<double>&      PsiT
    );

protected:
    // Virtual hook called by TranCore::coroutine() at every accepted
    // timestep, after solution.advance() and states.advance().
    //
    // tSolve: absolute time of the accepted point.
    // hk:     timestep used to reach this point.
    // order:  LMS integration order used at this step.
    //
    // This method calls collectGC() and appends a PssTrajectoryPoint
    // to trajectory_.
    virtual bool onTimestepAccepted(double tSolve, double hk, Int order);

private:
    std::vector<PssTrajectoryPoint> trajectory_;

    // Collect G(t) and C(t) at the current converged circuit state and
    // fill pt.G and pt.C. Called by onTimestepAccepted().
    // Does not disturb the main jac, which is left zeroed on return.
    bool collectGC(PssTrajectoryPoint& pt);

    // Copy the relevant prefix of the pastTimesteps circular buffer
    // into snapshot. The length copied is integCoeffs.pastStatesNeeded().
    void snapshotPastTimesteps(std::vector<double>& snapshot);
};

} // namespace NAMESPACE

#endif // __COREPSSTRAN_DEFINED
