#ifndef __ANCOREAC_DEFINED
#define __ANCOREAC_DEFINED

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


SIMPLE_ERRORCLASS(AcSweepSetupFailed, "Failed to set up the AC frequency sweep.");

SIMPLE_ERRORCLASS(AcSweepComputeFailed, "AC sweep point computation failed.");

SIMPLE_ERRORCLASS(AcEvalAndLoadFailed, "Jacobian evaluation failed.");

SIMPLE_ERRORCLASS(AcMatrixError, "AC matrix error.");

SIMPLE_ERRORCLASS(AcSolutionNotFinite, "Solution component is not finite.");

SIMPLE_ERRORCLASS(AcOperatingPointFailed, "Operating point analysis failed.");

SIMPLE_ERRORCLASS(AcSingularMatrix, "Matrix is close to singular.");

SIMPLE_ERRORCLASS(AcBadFrequency, "Frequency value cannot be converted to real.");

SIMPLE_ERRORCLASS(AcDelayBindFailed, "Failed to bind delay lines to the AC matrix.");

SIMPLE_ERRORCLASS(AcBindFailed, "Failed to bind the AC matrix.");

ERRORCLASS(AcSweepAborted)
    double frequency;
    AcSweepAborted(double frequency) : frequency(frequency) {}
    std::string format() const {
        if (frequency >= 0) {
            return "Leaving frequency sweep at frequency=" + std::to_string(frequency) + ".";
        }
        return "Leaving frequency sweep.";
    }
END_ERRORCLASS(AcSweepAborted);


class ACCore : public AnalysisCore {
public:
    typedef ACParameters Parameters;

    ACCore(
        OutputDescriptorResolver& parentResolver, ACParameters& params, OperatingPointCore& opCore, Circuit& circuit, 
        CommonData& commons, 
        KluRealMatrix& dcJacobian, VectorRepository<double>& dcSolution, VectorRepository<double>& dcStates, 
        KluComplexMatrix& acMatrix, Vector<Complex>& acSolution, 
        DelayLines& delayLines, DelayMatrixBindings<Complex*>& delayBindings
    ); 
    ~ACCore();
    
    ACCore           (const ACCore&)  = delete;
    ACCore           (      ACCore&&) = delete;
    ACCore& operator=(const ACCore&)  = delete;
    ACCore& operator=(      ACCore&&) = delete;

    bool addCoreOutputDescriptors(ErrorConsumer& errors);
    bool addDefaultOutputDescriptors(ErrorConsumer& errors);
    bool resolveOutputDescriptors(bool strict, ErrorConsumer& errors);

    bool rebuild(ErrorConsumer& errors);
    bool initializeOutputs(const std::string& name, ErrorConsumer& errors);
    bool run(bool continuePrevious, ErrorConsumer& errors);
    CoreCoroutine coroutine(bool continuePrevious, ErrorConsumer& errors);
    bool finalizeOutputs(ErrorConsumer& errors);
    bool deleteOutputs(Id name, ErrorConsumer& errors);

    void dump(std::ostream& os) const;

    OperatingPointCore& opCore_;
    OutputRawfile* outfile;

protected:
    static constexpr size_t bucketSize = 1;

    VectorRepository<double>& dcSolution;
    VectorRepository<double>& dcStates;
    KluRealMatrix& dcJacobian;
    KluComplexMatrix& acMatrix;
    Vector<Complex>& acSolution;
    ACParameters& params;

    DelayLines& delayLines_;
    DelayMatrixBindings<Complex*>& delayBindings_;

    double frequency;

private:
    UnknownNameResolver resolver_;
};

}

#endif
