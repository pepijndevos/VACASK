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
#include "cscblkmatrix.h"
#include "densematrix.h"
#include "output.h"
#include "outrawfile.h"
#include "common.h"

namespace NAMESPACE {

typedef struct PssParameters {
    Int  oscillator {0};    // Autonomous circuit, assume driven by default
    Real tper       {0.0};  // Initial period guess
    Real tstab      {0.0};  // Stabilization transient time
    Real stabstep   {0.0};  // Stabilization transient timestep
    Id   icmode     {Id()}; // IC mode for stabilisation transient (op by default)
    Int maxharm     {0};    // Maximal number of harmonics; limits timestep to period/(2*(maxharm+1))
    Real maxacfreq  {0.0};  // Max AC frequency to resolve; limits timestep to 1/(2*(maxacfreq+1/period)). Clipped to 40/tper if below that.
    String store    {""};   // Name of stored solution slot to write
    Int adjoint     {0};    // Enable adjoint monodromy computation
    Int  write      {1};    // Write output datasets
    Int  solve      {1};    // If true, shoots for the PSS solution, if false evaluates at the
                            // stored solution given by nodeset. Not exposed to user.
                            // nodeset is mapped to opParams
                            // ic is mapped to stabilParams
                            // solver is mapped to opParams
    
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


SIMPLE_ERRORCLASS(PssCircuitUsesUnsupportedFeatures, "Circuit uses features not supported by PSS analysis.");

SIMPLE_ERRORCLASS(PssTperInvalid, "Period should be >0.");

SIMPLE_ERRORCLASS(PssSolveIcType, "Ic must be a string when the PSS is not solved.");

SIMPLE_ERRORCLASS(PssIcNotFound, "Stored PSS solution given by ic not found.");

ERRORCLASS(PssIcIncomplete)
    Id node;
    PssIcIncomplete(Id node) : node(node) {}
    std::string format() const {
        return "Stored PSS solution given by ic has no value for node '" + std::string(node) + "'.";
    }
END_ERRORCLASS(PssIcIncomplete);

SIMPLE_ERRORCLASS(PssForcesFailed, "Failed to set forces.");

SIMPLE_ERRORCLASS(PssStabstepInvalid, "Stabilization timestep must be smaller than the period.");

SIMPLE_ERRORCLASS(PssStabilisationTranFailed, "PSS stabilisation transient failed.");

SIMPLE_ERRORCLASS(PssShootingTranFailed, "PSS transient failed.");

SIMPLE_ERRORCLASS(PssSensitivityFailed, "PSS sensitivity integration failed.");

SIMPLE_ERRORCLASS(PssSingularJacobian, "Singular Jacobian.");

ERRORCLASS(PssNoConvergence)
    Int iterations;
    PssNoConvergence(Int iterations) : iterations(iterations) {}
    std::string format() const {
        return "PSS failed to converge in " + std::to_string(iterations) + " iterations.";
    }
END_ERRORCLASS(PssNoConvergence);

SIMPLE_ERRORCLASS(PssAdjointFailed, "Adjoint monodromy computation failed.");

SIMPLE_ERRORCLASS(PssAdjointDisabled, "Adjoint monodromy was not computed. Set adjoint=1 before running PSS.");

SIMPLE_ERRORCLASS(PssTooManyShootingPoints, "The required number of PSS evaluation points overflows.");

SIMPLE_ERRORCLASS(PssFftPlanFailed, "Failed to create FFTW plan.");

SIMPLE_ERRORCLASS(PssMaxFreqIndexTooLarge, "Insufficient harmonics computed. Increase maxacfreq and maxharm.");


class PssCore : public AnalysisCore {
public:
    PssCore(
        OutputDescriptorResolver& parentResolver,
        PssParameters& params,
        Circuit& circuit,
        CommonData& commons,
        CSCRealMatrix& jacobian,
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

    bool addDefaultOutputDescriptors(ErrorConsumer& errors);

    bool rebuild(ErrorConsumer& errors);
    bool initializeOutputs(Id name, ErrorConsumer& errors);
    bool run(bool continuePrevious, ErrorConsumer& errors);
    CoreCoroutine coroutine(bool continuePrevious, ErrorConsumer& errors);

    // Evaluate one period without stabilising or shooting the Newton loop
    // nPts<=0 computes the number of points based on pss_minpts option, maxacfreq, and maxharm
    bool evaluate(bool atIc, bool noiseModulation, int nPts, ErrorConsumer& errors);
    // noiseModulationSpec: one block of maxFreqIndex+1 components per modulation slot
    bool getFrequencyDomainJacobians(CSCBlockSparseComplexMatrix& jacSpec, int maxFreqIndex, Vector<Complex>* noiseModulationSpec, ErrorConsumer& errors);
    // Flicker exponents per modulation slot, filled by evaluate(..., true, ...)
    const Vector<double>& noiseExponents() const;

    bool finalizeOutputs(ErrorConsumer& errors);
    bool deleteOutputs(Id name, ErrorConsumer& errors);

    bool storeState(size_t ndx, bool storeDetails=true) override;
    bool restoreState(size_t ndx) override;
    
    void dump(std::ostream& os) const;

    // Converged period in seconds. Valid after a successful run().
    double convergedPeriod() const { return T0_converged_; }

    // Maximum transient step actually used during the last shoot (after maxacfreq clamping). Valid after runShoot().
    double maxShootingStep() const { return maxShootingStep_; }

    // Converged PSS initial condition vector xs(t0). Valid after a successful run().
    const Vector<double>& convergedInitialCondition() const { return x0_converged_; }

    // Converged monodromy matrix Phi. Valid after a successful run().
    const DenseMatrix<double>& convergedMonodromy() const { return phiT_; }

    // Converged adjoint monodromy matrix Omega. Valid after a successful run()
    // with adjoint enabled. Returns false and pushes an error onto errors if
    // adjoint monodromy was not computed (params.adjoint==0).
    // Return value: ok, adjont monodromy matrix reference
    std::tuple<bool, const DenseMatrix<double>&> convergedAdjointMonodromy(ErrorConsumer& errors);

protected:
    // Prepare stabilisation transient
    void prepareStabilisation(double period);

    // Clamp step/maxstep of tp to respect params.maxacfreq and params.maxharm. 
    // Based on current period (stabilisation period or shoot period T0). Shared
    // by prepareStabilisation() and runShoot().
    void clampStep(TranParameters& tp, double period) const;

    CSCRealMatrix& jacobian;            // Resistive Jacobian
    VectorRepository<double>& solution; // Solution history
    VectorRepository<double>& states;   // Circuit states

    // Converged results. Populated on successful run().
    double         T0_converged_;
    Vector<double> x0_converged_;

    // Maximum transient step actually used during the last shoot. Set in runShoot(), read by maxShootingStep().
    double maxShootingStep_;

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
    std::vector<int>    rowPerm_; // Pivot scratch for Jp.factorAndLuSolve() (LAPACK ipiv storage type)
    std::vector<double> ones_;    // Length n, all 1.0, sized/filled in rebuild(); used to add
                                   // the identity to Jp's n x n block via diagonal().addScaled()

    // Shooting Newton state, sized in rebuild(); the main loop only assigns
    // into them (no local reallocation).
    Vector<double>      x0;
    Vector<double>      xT;

    // Run the DC operating point and the stabilisation transient.
    // On return, solution_.vector() holds the initial guess x0 for the
    // Newton loop.
    // Return value: ok, period
    std::tuple<bool, double> runStabilisation(bool continuePrevious, ErrorConsumer& errors);

    // Prepare timestep for shoot
    void prepareShoot(double T0);

    // Integrate one period T0 from the initial condition in solution_.vector().
    // On return, solution_.vector() holds xT, the endpoint of the shoot.
    // pssTran_.trajectory() is populated with G(t) and C(t) snapshots.
    bool runShoot(ErrorConsumer& errors);

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
