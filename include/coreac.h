#ifndef __ANCOREAC_DEFINED
#define __ANCOREAC_DEFINED

#include "status.h"
#include "circuit.h"
#include "core.h"
#include "coreop.h"
#include "klumatrix.h"
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

// AC small-signal analysis
// Assuming t=0 first solves for operating point (x0)
//   f(x0) = 0
// where f(x) is the resistive residual. 
// Then it linearizes the circuit by computing the resistive Jacobian Jr
// (Jacobian of f(x)) and the reactive Jacobian Jc (Jacobian of q(x)) 
// at x=x0 and solves
//   (Jr + omega Jc) X = U
// for 
//   omega = 2 pi f 
// where the range of f is given as
// - given values (values=[...])
// - stepped linear sweep (from, to, step)
// - linear sweep with given number of points 
//   (from, to, points, mode="lin")
// - logarithmic sweep with given number of points per decade 
//   (from, to, points, mode="dec")
// - logarithmic sweep with given number of points per decade 
//   (from, to, points, mode="oct")
// U comprises the AC excitations specified by the mag and phase 
// parameters of independent sources. Phase is given in degrees. 
// Mag can be negative (equivalent to adding 180 degrees to the phase). 
// The resulting X comprises phasors corresponding to sinusoidal responses 
// in the circuit's unknowns. Sinusoidal signal
//   A cos(omega t + phi) 
// corresponds to phasor
//   A exp(j phi)
// where j is the imaginary unit. 
// 
// See coreop.h on how to specify nodesets. 

typedef struct ACParameters {
    OperatingPointParameters opParams;

    Real from {0};    // Start frequency for step and dec/oct/lin sweep
    Real to {0};      // Stop frequency for step and dec/oct/lin sweep
    Real step {0};    // Step size for step sweep
    Id mode {Id()};   // Mode for dec/oct/lin sweep
    Int points {0};   // Number of points for dec/oct/lin sweep
    Value values {0}; // Vector of values for values sweep
    Int write {1};    // Write the results to a file
                      // writeop is the write parameter of op core
                      // nodeset and store parameters of the op core are also exposed. 

    ACParameters();
} ACParameters;


class ACCore : public AnalysisCore {
public:
    typedef ACParameters Parameters;
    enum class AcError {
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
       
    ACCore(
        OutputDescriptorResolver& parentResolver, ACParameters& params, OperatingPointCore& opCore, Circuit& circuit, 
        CommonData& commons, 
        KluRealMatrix& dcJacobian, VectorRepository<double>& dcSolution, VectorRepository<double>& dcStates, 
        KluComplexMatrix& acMatrix, Vector<Complex>& acSolution
    ); 
    ~ACCore();
    
    ACCore           (const ACCore&)  = delete;
    ACCore           (      ACCore&&) = delete;
    ACCore& operator=(const ACCore&)  = delete;
    ACCore& operator=(      ACCore&&) = delete;

    // Format error, return false on error - this function is not cheap (works with strings)
    bool formatError(Status& s=Status::ignore) const; 

    bool addCoreOutputDescriptors(Status& s);
    bool addDefaultOutputDescriptors(Status& s);
    bool resolveOutputDescriptors(bool strict, Status& s=Status::ignore);

    bool rebuild(Status& s=Status::ignore); 
    bool initializeOutputs(const std::string& name, Status& s=Status::ignore);
    bool run(bool continuePrevious);
    CoreCoroutine coroutine(bool continuePrevious);
    bool finalizeOutputs(Status& s=Status::ignore);
    bool deleteOutputs(Id name, Status& s=Status::ignore);

    void dump(std::ostream& os) const;

    OperatingPointCore& opCore_;
    OutputRawfile* outfile;

protected:
    static constexpr size_t bucketSize = 1;

    // Clear error
    void clearError() { AnalysisCore::clearError(); lastAcError = AcError::OK; }; 

    void setError(AcError e) { lastAcError = e; lastError = Error::OK; };
    AcError lastAcError;
    double errorFreq;
    Status errorStatus;

    VectorRepository<double>& dcSolution;
    VectorRepository<double>& dcStates;
    KluRealMatrix& dcJacobian;
    KluComplexMatrix& acMatrix;
    Vector<Complex>& acSolution;
    ACParameters& params;

    double frequency;
};

}

#endif
