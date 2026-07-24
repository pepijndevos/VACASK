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
//   For l = 0, 1, ..., pss_itl-1 (pss_itl attempts total):
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
    Int  driven     {0};    // Non-autonomous (driven) circuit
    Real tper       {0.0};  // Initial period guess
    Real tstab      {0.0};  // Stabilization transient time
    Real stabstep   {0.0};  // Stabilization transient timestep
    Id   icmode     {Id()}; // IC mode for stabilisation transient (op by default)
    Real maxacfreq  {0.0};  // Max AC frequency to resolve; limits maxstep to 1/(2*maxacfreq). Clipped to 40/tper if below that.
    String store    {""};   // Name of stored solution slot to write
    Int adjoint     {0};    // Enable adjoint monodromy computation
    Int  write      {1};    // Write output datasets
                            // nodeset is mapped to opParams
                            // ic is mapped to stabilParams

    // Parameters forwarded to subsidiary cores
    OperatingPointParameters opParams;
    TranParameters stabilParams;
    TranParameters shootParams;

    PssParameters() {
        icmode = TranCore::icmodeOp;
        opParams.write      = 0;
        stabilParams.write  = 0;
        shootParams.write   = 0;
        // Shooting icmode is always uic
        shootParams.icmode = TranCore::icmodeUic;
    }
} PssParameters;


class PssCore : public AnalysisCore {
public:
    enum class PssError {
        OK,
        NoConvergence,
        SensitivityFailed,
        AdjointFailed,
        AdjointDisabled,
        LinearSolveFailed,
        OutputError,
        TperInvalid,
        SingularJacobian,
        StabstepInvalid, 
        StabilisationTranFailed,
        OpFailed, 
        ShootingTranFailed, 
        ForcesFailed, 
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

    virtual bool storeState(size_t ndx, bool storeDetails=true);
    virtual bool restoreState(size_t ndx);
    
    void dump(std::ostream& os) const;

    // Converged period in seconds. Valid after a successful run().
    double convergedPeriod() const { return T0_converged_; }

    // Converged PSS initial condition vector xs(t0). Valid after a successful run().
    const Vector<double>& convergedInitialCondition() const { return x0_converged_; }

    // Converged monodromy matrix Phi. Valid after a successful run().
    const DenseMatrix<double>& convergedMonodromy() const { return phiT_; }

    // Converged adjoint monodromy matrix Omega. Valid after a successful run()
    // with adjoint enabled. Returns false and sets the error (formattable via
    // formatError()) if adjoint monodromy was not computed (params.adjoint==0).
    // Return value: ok, adjont monodromy matrix reference
    std::tuple<bool, const DenseMatrix<double>&> convergedAdjointMonodromy();

protected:
    // Prepare stabilisation transient
    void prepareStabilisation(double period);

    // Clamp step/maxstep of tp to respect params.maxacfreq, given the
    // applicable period (stabilisation period or shoot period T0). Shared
    // by prepareStabilisation() and runShoot().
    void clampStepToMaxacfreq(TranParameters& tp, double period) const;

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

    Vector<double>      Fp;
    Vector<double>      alpha;
    DenseMatrix<double> Jp;

    // Shooting Newton state, sized in rebuild(); the main loop only assigns
    // into them (no local reallocation).
    Vector<double>      x0;
    Vector<double>      xT;

    // Run the DC operating point and the stabilisation transient.
    // On return, solution_.vector() holds the initial guess x0 for the
    // Newton loop.
    // Return value: ok, period
    std::tuple<bool, double> runStabilisation(bool continuePrevious);

    // Integrate one period T0 from the initial condition in solution_.vector().
    // On return, solution_.vector() holds xT, the endpoint of the shoot.
    // pssTran_.trajectory() is populated with G(t) and C(t) snapshots.
    bool runShoot(double T0);

    // Compute the phase constraint vector alpha.
    // alpha fixes the phase of the PSS solution so the (n+1) x (n+1)
    // augmented system is square and non-singular. alpha is estimated as the
    // forward finite difference (x1-x0)/h0 between the initial shoot state
    // x0 and the state x1 at the first accepted timepoint of the shoot,
    // which approximates xdot_s(t0) (pss.md, "Choosing alpha"). Normalised
    // to unit length for conditioning.
    void computePhaseConstraint(
        const Vector<double>& x0,
        const Vector<double>& x1,
        double h0,
        Vector<double>& alpha
    );
};

} // namespace NAMESPACE

#endif // __COREPSS_DEFINED
