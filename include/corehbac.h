#ifndef __ANCOREHBAC_DEFINED
#define __ANCOREHBAC_DEFINED

#include "status.h"
#include "circuit.h"
#include "core.h"
#include "corehb.h"
#include "klubsmatrix.h"
#include "output.h"
#include "flags.h"
#include "outrawfile.h"
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

// (Quasi)cyclostationary small-signal analysis based on harmonic balance
// 
// See corehb.h on how to specify nodesets. 

typedef struct HBACParameters {
    HBParameters hbParams;

    Real from {0};    // Start frequency for step and dec/oct/lin sweep
    Real to {0};      // Stop frequency for step and dec/oct/lin sweep
    Real step {0};    // Step size for step sweep
    Id mode {Id()};   // Mode for dec/oct/lin sweep
    Int points {0};   // Number of points for dec/oct/lin sweep
    Value values {0}; // Vector of values for values sweep
    Int writehb {0};  // 1 = dump cyclostationary operating point to <analysisname>.hb.raw;
    // Nodeset and store parameters of the HB core 
    // are also exposed. 

    Value outspur {IntVector()};   // specifies spurs where small signal response is observed
                        // - scalar real spur frequency
                        // - integer vector with tone weights defining a spur
                        // - list holding reals (frequency), integers (only for 1-tone), integer vectors (tone weights)
                        // empty vector (default) computes response at all spurs

    Int write {1};    // Write the results to a file

    HBACParameters();
} HBACParameters;


class HBACCore : public AnalysisCore {
public:
    typedef HBACParameters Parameters;
    enum class HBACError {
        OK, 
        Sweeper, 
        SweepCompute, 
        EvalAndLoad, 
        MatrixError, 
        SolutionError, 
        OperatingPointError, 
        SingularMatrix, 
        BadFrequency, 
    };
       
    HBACCore(
        OutputDescriptorResolver& parentResolver, HBACParameters& params, HBCore& opCore, Circuit& circuit, 
        CommonData& commons, 
        KluRealMatrix& dcJacobian, VectorRepository<double>& dcSolution, VectorRepository<double>& dcStates, 
        KluComplexMatrix& acMatrix, Vector<Complex>& acSolution
    ); 
    ~HBACCore();
    
    HBACCore           (const HBACCore&)  = delete;
    HBACCore           (      HBACCore&&) = delete;
    HBACCore& operator=(const HBACCore&)  = delete;
    HBACCore& operator=(      HBACCore&&) = delete;

    // Format error, return false on error - this function is not cheap (works with strings)
    bool formatError(Status& s=Status::ignore) const; 

    bool addCoreOutputDescriptors();
    bool addDefaultOutputDescriptors();
    bool resolveOutputDescriptors(bool strict);

    bool rebuild(Status& s=Status::ignore); 
    bool initializeOutputs(Id name, Status& s=Status::ignore);
    bool run(bool continuePrevious);
    CoreCoroutine coroutine(bool continuePrevious);
    bool finalizeOutputs(Status& s=Status::ignore);
    bool deleteOutputs(Id name, Status& s=Status::ignore);

    void dump(std::ostream& os) const;

    HBCore& hbCore_;
    OutputRawfile* outfile;

protected:
    // Clear error
    void clearError() { AnalysisCore::clearError(); lastHBACError = HBACError::OK; }; 

    void setError(HBACError e) { lastHBACError = e; lastError = Error::OK; };
    HBACError lastHBACError;
    double errorFreq;
    Status errorStatus;

    Vector<Complex>& hbSolution;
    KluBlockSparseRealMatrix jacFd;
    KluBlockSparseComplexMatrix jacG;
    KluBlockSparseComplexMatrix jacC;
    KluBlockSparseComplexMatrix acMatrix;
    Vector<Complex>& acSolution;
    HBACParameters& params;

    double frequency;
};

}

#endif
