#ifndef __COREPSS_DEFINED
#define __COREPSS_DEFINED

// corepss.h
//
// PssCore - outer Newton loop for the single-shooting PSS algorithm
//
// Reference:
//   T. Djurhuus and V. Krozer, "A Novel Phase-Noise Module for the
//   QUCS Circuit Simulator. Part I: the Periodic Steady-State",
//   arXiv:2512.10373v1, December 2025.
//
// PssCore is an AnalysisCore that finds the periodic steady state of
// an autonomous circuit by Newton-Raphson iteration on the shooting
// residual. It owns three subsidiary cores:
//
//   opCore_     - computes the DC operating point used as the starting
//                 point for the stabilisation transient.
//
//   stabilTran_ - runs a plain transient of length Tstab to let initial
//                 transients decay. The endpoint of this run is the
//                 initial guess x0 for the Newton loop.
//
//   pssTran_    - runs exactly one period T0 per Newton iteration,
//                 collecting the resistive Jacobian G(t) and reactive
//                 Jacobian C(t) at every accepted timestep. After each
//                 shoot it integrates the linearised circuit equations
//                 to produce the sensitivity matrix PhiT.
//
// All three cores share the same jac, solution, and states repositories
// owned by the enclosing PSS analysis.
//
// PssCore drives pssTran_ directly via run() rather than as a coroutine.
// This lets the Newton loop call pssTran_.run() multiple times, once per
// shooting iteration. The outer coroutine required by AnalysisCore is a
// thin wrapper around run().
//
// Newton loop outline:
//
//   Stabilise: integrate tstab seconds from xDC to obtain x0.
//
//   For l = 0, 1, ..., pss_itl:
//
//     Shoot: set solution = x0, run pssTran_ for T0 seconds.
//            pssTran_ collects G(t) and C(t) via onTimestepAccepted().
//            At end: xT = solution.
//
//     Residual: Fp = x0 - xT
//
//     Converged? if per-unknown |Fp[i]| < pss_tolscale*max(|x0[i]|*reltol, abstol[i]), store result and return.
//
//     Sensitivity: call pssTran_.integrateSensitivity() to obtain:
//       PhiT  - n x n state-transition matrix dxT/dx0
//       PsiT  - n x 1 period sensitivity vector -dxT/dT0
//
//     Build augmented (n+1) x (n+1) Newton system:
//
//       Jaug = | I - PhiT   PsiT |
//              | alpha^T    0    |
//
//       Frhs = | Fp           |
//              | alpha^T * x0 |
//
//     where alpha is the phase constraint vector (fixes the phase of
//     the solution so the system is not underdetermined).
//
//     Solve Jaug * [dx0; dT0] = Frhs using dense LU.
//
//     Update: x0 = x0 - dx0
//             T0 = T0 - dT0

#include "core.h"
#include "coreop.h"
#include "corepsstran.h"
#include "coretran.h"
#include "densematrix.h"
#include "output.h"
#include "outrawfile.h"
#include "common.h"

namespace NAMESPACE {

typedef struct PssParameters {
    Int  driven     {0};        // Non-autonomous (driven) circuit
    Real tper       {0.0};      // Initial period guess
    Real tstab      {0.0};      // Stabilization transient time
    Real stabstep   {0.0};      // Stabilization transient timestep
    Real maxacfreq  {0.0};      // Max AC frequency to resolve; limits maxstep to 1/(2*maxacfreq). Clipped to 40/tper if below that.
    Int  write      {1};        // Write output datasets
    Value ic {Value("")};       // Initial conditions

    // Parameters forwarded to subsidiary cores
    OperatingPointParameters opParams;
    TranParameters stabilParams;
    TranParameters shootParams;

    PssParameters() {
        opParams.write      = 0;
        stabilParams.write  = 0;
        shootParams.write   = 0;
    }
} PssParameters;


class PssCore : public AnalysisCore {
public:
    enum class PssError {
        OK,
        NoConvergence,
        SensitivityFailed,
        AdjointFailed,
        LinearSolveFailed,
        OutputError,
        TperInvalid,
        SingularJacobian,
        StabstepInvalid, 
        StabilisationTranFailed,
        OpFailed, 
        ShootingTranFailed, 
    };

    PssCore(
        OutputDescriptorResolver& parentResolver,
        PssParameters& params,
        Circuit& circuit,
        CommonData& commons,
        KluRealMatrix& jacobian,
        VectorRepository<double>& solution,
        VectorRepository<double>& states,
        OperatingPointCore& opCore,
        TranCore& stabilTran,
        PssTranCore& pssTran
    );

    PssCore           (const PssCore&)  = delete;
    PssCore           (      PssCore&&) = delete;
    PssCore& operator=(const PssCore&)  = delete;
    PssCore& operator=(      PssCore&&) = delete;

    // Format error, return false on error - this function is not cheap (works with strings)
    bool formatError(Status& s = Status::ignore) const;
    
    bool addDefaultOutputDescriptors(Status& s);
    
    bool rebuild(Status& s = Status::ignore);
    bool initializeOutputs(Id name, Status& s = Status::ignore);
    bool run(bool continuePrevious);
    CoreCoroutine coroutine(bool continuePrevious);
    bool finalizeOutputs(Status& s = Status::ignore);
    bool deleteOutputs(Id name, Status& s = Status::ignore);

    virtual bool storeState(size_t ndx);
    virtual bool restoreState(size_t ndx);
    
    void dump(std::ostream& os) const;

    // Converged period in seconds. Valid after a successful run().
    double convergedPeriod() const { return T0_converged_; }

    // Converged PSS initial condition vector xs(t0). Valid after a successful run().
    const Vector<double>& convergedInitialCondition() const { return x0_converged_; }

    // Converged monodromy matrix Phi. Valid after a successful run().
    const DenseMatrix<double>& convergedMonodromy() const { return phiT_; }

    // Converged adjoint monodromy matrix Omega. Valid after a successful run().
    const DenseMatrix<double>& convergedAdjointMonodromy() const { return omegaT_; }

protected:
    // Clear error
    void clearError() { AnalysisCore::clearError(); lastPssError = PssError::OK; }

    void setError(PssError e) { lastPssError = e; lastError = Error::OK; }
    PssError lastPssError;
    Id errorId;

    KluRealMatrix& jacobian;            // Resistive Jacobian
    VectorRepository<double>& solution; // Solution history
    VectorRepository<double>& states;   // Circuit states

    // Converged results. Populated on successful run().
    double         T0_converged_;
    Vector<double> x0_converged_;

    // Analysis name stored at initializeOutputs() time, used by runStabilisation().
    Id name_;

    CoreStateStorage* continueState;

private:
    OperatingPointCore& opCore_;
    TranCore&           stabilTran_;
    PssTranCore&        pssTran_;

    PssParameters&      params;

    // State transition matrices
    DenseMatrix<double> phiT_;
    DenseMatrix<double> omegaT_;    // adjoint

    // Run the DC operating point and the stabilisation transient.
    // On return, solution_.vector() holds the initial guess x0 for the
    // Newton loop.
    bool runStabilisation(double period);

    // Integrate one period T0 from the initial condition in solution_.vector().
    // On return, solution_.vector() holds xT, the endpoint of the shoot.
    // pssTran_.trajectory() is populated with G(t) and C(t) snapshots.
    bool runShoot(double T0);

    // Call pssTran_.integrateSensitivity() to produce PhiT and PsiT.
    // PhiT is the n x n sensitivity matrix dxT/dx0.
    // PsiT is the n x 1 period sensitivity vector -dxT/dT0.
    bool runSensitivity(
        DenseMatrix<double>& PhiT,
        Vector<double>&      PsiT
    );

    // Build the regular (driven) Newton system from PhiT, x0 and xT. 
    // Solve it and update x0 and T0.
    bool solveNewtonStep(
        Vector<double>&       x0,
        const Vector<double>& xT,
        DenseMatrix<double>&  PhiT,
        Status& s
    );

    // Build the augmented Newton system from PhiT, PsiT, x0, xT, and
    // the phase constraint vector alpha. Solve it and update x0 and T0.
    bool solveAugmentedNewtonStep(
        Vector<double>&       x0,
        double&               T0,
        const Vector<double>& xT,
        DenseMatrix<double>&  PhiT,
        const Vector<double>& PsiT,
        Status& s
    );

    // Compute the phase constraint vector alpha.
    // alpha fixes the phase of the PSS solution so the (n+1) x (n+1)
    // augmented system is square and non-singular. alpha is set
    // proportional to PsiT, which approximates xdot_s(t0) from the
    // circuit equations. Normalised to unit length for conditioning.
    void computePhaseConstraint(
        const Vector<double>& x0,
        double T0,
        const Vector<double>& PsiT,
        Vector<double>& alpha
    );
};

} // namespace NAMESPACE

#endif // __COREPSS_DEFINED
