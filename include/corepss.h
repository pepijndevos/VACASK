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
//   Stabilise: integrate Tstab seconds from xDC to obtain x0.
//
//   For l = 0, 1, ..., MaxItr:
//
//     Shoot: set solution = x0, run pssTran_ for T0 seconds.
//            pssTran_ collects G(t) and C(t) via onTimestepAccepted().
//            At end: xT = solution.
//
//     Residual: Fp = x0 - xT
//
//     Converged? if norm(Fp) < EpsMax, store result and return.
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
#include "densematrix.h"
#include "output.h"
#include "outrawfile.h"
#include "common.h"

namespace NAMESPACE {

typedef struct PssParameters {
    // Initial period guess in seconds. Required.
    // The Newton loop converges to the true period starting from this value.
    Real Tper   {0.0};

    // Stabilisation time in seconds before shooting begins.
    // Must be long enough for initial transients to decay.
    // Recommended: Tstab >= 10 * Tper.
    Real Tstab  {0.0};

    // Maximum number of shooting Newton iterations.
    Int  MaxItr {20};

    // Convergence tolerance on the shooting residual norm(x0 - xT).
    Real EpsMax {1e-12};

    // Write output datasets (1) or suppress output (0).
    Int  write  {1};

    // Initial conditions for the stabilisation transient.
    // Same format as TranParameters::ic: a list ["node"; value; ...].
    // When non-empty, the stabilisation transient uses icmodeUic so the
    // oscillation can start away from the (potentially degenerate) DC
    // operating point.  Example: ic=["1"; 0.9] sets V(1)=0.9 at t=0.
    Value ic {Value("")};

    // Parameters forwarded to the operating point core.
    OperatingPointParameters opParams;

    // Parameters forwarded to the stabilisation and shooting transients.
    // PssCore fills step, stop, and maxstep automatically from Tper and Tstab.
    TranParameters stabilParams;

    PssParameters() {
        opParams.write    = 0;
        stabilParams.write = 0;
    }
} PssParameters;


class PssCore : public AnalysisCore {
public:
    enum class PssError {
        OK,
        NoConvergence,
        StabilisationFailed,
        ShootFailed,
        SensitivityFailed,
        LinearSolveFailed,
        OutputError,
    };

    PssCore(
        OutputDescriptorResolver& parentResolver,
        PssParameters& params,
        Circuit& circuit,
        CommonData& commons,
        KluRealMatrix& jacobian,
        VectorRepository<double>& solution,
        VectorRepository<double>& states
    );

    ~PssCore();

    PssCore           (const PssCore&)  = delete;
    PssCore           (      PssCore&&) = delete;
    PssCore& operator=(const PssCore&)  = delete;
    PssCore& operator=(      PssCore&&) = delete;

    // AnalysisCore interface
    bool formatError(Status& s = Status::ignore) const;

    bool addCoreOutputDescriptors();
    bool addDefaultOutputDescriptors();
    bool resolveOutputDescriptors(bool strict, Status& s = Status::ignore);

    // Output descriptor proxy methods.
    // PSS output is produced by the shooting transient (pssTran_).  These
    // shadowing methods route all save directives directly to pssTran_ so
    // that its resolver sees them when resolveOutputDescriptors() is called.
    void clearOutputDescriptors();
    bool addOutputDescriptor(const OutputDescriptor& desc);
    bool addAllUnknowns(const PTSave& save);
    bool addAllNodes(const PTSave& save);
    bool addNode(const PTSave& save);
    bool addFlow(const PTSave& save);
    bool addInstanceOutvar(const PTSave& save);

    bool rebuild(Status& s = Status::ignore);
    bool initializeOutputs(Id name, Status& s = Status::ignore);
    bool run(bool continuePrevious);
    CoreCoroutine coroutine(bool continuePrevious);
    bool finalizeOutputs(Status& s = Status::ignore);
    bool deleteOutputs(Id name, Status& s = Status::ignore);

    void dump(std::ostream& os) const;

    // Converged period in seconds. Valid after a successful run().
    double convergedPeriod() const { return T0_converged_; }

    // Converged PSS initial condition vector xs(t0).
    // Valid after a successful run().
    const Vector<double>& convergedInitialCondition() const { return x0_converged_; }

protected:
    void clearError() { AnalysisCore::clearError(); lastPssError_ = PssError::OK; }
    void setError(PssError e) { lastPssError_ = e; lastError = Error::OK; }

    PssError lastPssError_;

    // Jacobian and solution/state repositories shared by all cores.
    // Owned by the enclosing PSS analysis and passed here by reference.
    KluRealMatrix&              jac_;
    VectorRepository<double>&   solution_;
    VectorRepository<double>&   states_;

    // DC operating point core used to compute the initial DC solution.
    OperatingPointCore opCore_;

    // Stabilisation transient: lets initial transients decay before shooting.
    // Uses plain TranCore because G(t) and C(t) collection is not needed here.
    TranCore stabilTran_;

    // Shooting transient: integrates exactly one period T0 per Newton iteration.
    // Uses PssTranCore to collect G(t) and C(t) and integrate the LR equations.
    PssTranCore pssTran_;

    // Output file handle for the converged PSS trajectory.
    OutputRawfile* outfile_;

    // Converged results. Populated on successful run().
    double         T0_converged_;
    Vector<double> x0_converged_;

private:
    PssParameters& params_;

    // Run the DC operating point and the stabilisation transient.
    // On return, solution_.vector() holds the initial guess x0 for the
    // Newton loop.
    bool runStabilisation(Status& s);

    // Integrate one period T0 from the initial condition in solution_.vector().
    // On return, solution_.vector() holds xT, the endpoint of the shoot.
    // pssTran_.trajectory() is populated with G(t) and C(t) snapshots.
    bool runShoot(double T0, Status& s);

    // Call pssTran_.integrateSensitivity() to produce PhiT and PsiT.
    // PhiT is the n x n sensitivity matrix dxT/dx0.
    // PsiT is the n x 1 period sensitivity vector -dxT/dT0.
    bool runSensitivity(
        DenseMatrix<double>& PhiT,
        Vector<double>&      PsiT,
        Status& s
    );

    // Build the augmented Newton system from PhiT, PsiT, x0, xT, and
    // the phase constraint vector alpha. Solve it and update x0 and T0.
    bool solveNewtonStep(
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
