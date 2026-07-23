#ifndef __ANCORETRAN_DEFINED
#define __ANCORETRAN_DEFINED

#include <random>
#include "status.h"
#include "circuit.h"
#include "core.h"
#include "klumatrix.h"
#include "output.h"
#include "outrawfile.h"
#include "flags.h"
#include "coreop.h"
#include "coretrannr.h"
#include "coretrancoef.h"
#include "tdnblock.h"
#include "common.h"


namespace NAMESPACE {

// Circuit equations
//              d
//   f(x(t)) + ---- q(x(t)) = 0 
//              dt
// 
//   x(t) .. unknowns
//   f(x) .. resistive residual
//   q(x) .. reactive residual

// Large signal time-domain analysis. 
// icmode="op"  computes the point at t=0 using operating point analysis
//              where initial conditions are forced
// icmode="uic" tries to compute the initial states of reactive components 
//              from the given initial condition and does not perform 
//              operating point analysis. It is equivalent to classic SPICE3 
//              uic transient analysis. 
// 
// See coreop.h on how to specify nodesets. 
// 
// Initial conditions are specified with the same format as nodesets. 

typedef struct TranParameters {
    OperatingPointParameters opParams;
    Real step {0.0};          // Initial timestep, if 0 a default is used
    Real stop {0.0};          // Time up to which the circuit is to be simulated
    Real start {0.0};         // Time at which the results start being recorded
    Real maxstep {0.0};       // Maximal timestep (optional)
    Id icmode {Id()};         // op=op with ic forces, uic=spice uic
    Value ic {Value("")};     // String specifying stored solution slot or
                              // list specifying initial conditions
                              // for transient analysis
    Int noiseseed {0};        // seed for noise generators
    Real noisescale {1.0};    // scaling factor for noise generators
    Real noisefmax {0};       // maximum frequency for transient noise 
                              // 0 = disable transient noise
    Real noisefmin {0};       // minimum frequency for transient noise
                              // If noisefmin=0 uses noisefmax/1e3. 
                              // If noisefmax!=0 should be <noisefmax.
    Id noisemode {};          // Noise mode (ZOH by default)
    Int oversample {6};       // oversampling factor, 6 by default
                              // The timestep is limited to 0.5/(oversample*noisefmax). 
    String store {""};        // name of stored solution slot to write transient solution to
    // Nodeset parameter of the operating point core is also exposed. 

    Int write {1};            // Write the results to a file
    
    TranParameters();
} TranParameters;

// Operating point core functionality, assumes all circuit parameters and simulator options have been set
// This core uses no other core
class TranCore : public AnalysisCore {
public:
    typedef TranParameters Parameters;
    enum class TranError {
        OK, 
        Tstop, 
        Tstart, 
        Maxstep, 
        Fmin, 
        Fmax, 
        Oversample, 
        NoiseMode, 
        Rows, 
        Method, 
        IcMode, 
        Predictor, 
        Corrector, 
        EvalAndLoad, 
        MatrixError, 
        OperatingPointError, 
        NRSolver, 
        TimestepTooSmall, 
        BadLteReference, 
        BreakPointPanic, 
    };
    
    TranCore(
        OutputDescriptorResolver& parentResolver, TranParameters& params, OperatingPointCore& opCore, 
        Circuit& circuit, CommonData& commons, 
        KluRealMatrix& jacobian, VectorRepository<double>& opSolution, VectorRepository<double>& solution, 
        VectorRepository<double>& states
    ); 
    ~TranCore();
    
    TranCore           (const TranCore&)  = delete;
    TranCore           (      TranCore&&) = delete;
    TranCore& operator=(const TranCore&)  = delete;
    TranCore& operator=(      TranCore&&) = delete;

    // Format error, return false on error - this function is not cheap (works with strings)
    bool formatError(Status& s=Status::ignore) const; 

    bool addCoreOutputDescriptors(Status& s);
    bool addDefaultOutputDescriptors(Status& s);
    bool resolveOutputDescriptors(bool strict, Status& s=Status::ignore);

    std::tuple<bool, bool> preMapping(Status& s=Status::ignore);
    bool populateStructures(Status& s=Status::ignore);

    bool rebuild(Status& s=Status::ignore); 
    bool initializeOutputs(const std::string& name, Status& s=Status::ignore);
    void install(ProgressReporter* p);
    CoreCoroutine coroutine(bool continuePrevious);
    bool run(bool continuePrevious);
    bool finalizeOutputs(Status& s=Status::ignore);
    bool deleteOutputs(Id name, Status& s=Status::ignore);

    void dump(std::ostream& os) const;

    TranNRSolver& solver() { return nrSolver; }
    // Slot number used for ic forces, by default 2 
    // Analysies like PSS, use slot 3 in continue mode
    // TranCore writes only slot 2
    void setIcForcesSlot(size_t n) { icForcesSlot = n; };

    static Id icmodeOp;
    static Id icmodeUic;
    static Id methodAM;
    static Id methodBDF;
    static Id methodGear;
    static Id methodEuler;
    static Id methodTrapezoidal;
    static Id methodBDF2;
    static Id methodGear2;
    static Id noiseZoh;
    static Id noiseSde;

protected:
    // Clear error
    void clearError() { AnalysisCore::clearError(); lastTranError = TranError::OK; }; 

    void setError(TranError e) { lastTranError = e; lastError = Error::OK; };
    TranError lastTranError;
    Id errorId;
    
    VectorRepository<double>& opSolution; // Solution history
    KluRealMatrix& jacobian; // Resistive Jacobian
    VectorRepository<double>& solution; // Solution history
    VectorRepository<double>& states; // Circuit states

    RealVector predictedSolution;
    RealVector scaledLte;

    OutputRawfile* outfile;

    bool finished; 

    PreprocessedUserForces preprocessedIc;
    // Forces uicForces;

    const IntegratorCoeffs& getIntegCoeffs() const { return integCoeffs; }
    const CircularBuffer<double>& getPastTimesteps() const { return pastTimesteps; }

    // Called at every accepted timestep before pastTimesteps and tk are
    // updated, and before solution/states history is advanced - so
    // getIntegCoeffs()/getPastTimesteps() still reflect exactly the state
    // used to solve this step, not a history already advanced past it.
    // Return false to abort the analysis.
    virtual bool onTimestepAccepted(double /*tSolve*/, double /*hk*/, Int /*order*/) { return true; }

protected:
    TranParameters& params;

private:
    std::tuple<size_t, size_t, size_t> countNoiseSources() const;
    bool evalAndLoadWrapper(EvalSetup& evalSetup, LoadSetup& loadSetup);

    // Update breakpoint, but only if it is after last
    void updateBreakPoint(double& bp, double candidate, double last) { if (candidate<bp && candidate>last) bp = candidate; };

    VectorRepository<double> filteredSolution;
    
    RealVector noiselessSolution;

    OperatingPointCore& opCore_;
    NRSettings nrSettings;
    // integCoeffs declared before nrSolver: nrSolver's init list binds a reference
    // to it, so it must be fully constructed first.
    IntegratorCoeffs integCoeffs;
    TranNRSolver nrSolver;
    CircularBuffer<double> pastTimesteps;
    IntegratorCoeffs predictorCoeffs;
    CircularBuffer<double> breakPoints;
    double acceptedBoundStep;
    double acceptedHmax;
    
    size_t nPoints;
    double tk;
    size_t icForcesSlot;
    
    // Transient noise
    std::mt19937_64 randomGenerator;
    std::unique_ptr<TimeDomainNoiseBlock<std::mt19937_64>> whiteBlock;
    std::unique_ptr<TimeDomainNoiseBlock<std::mt19937_64>> flickerBlock;
};

}

#endif
