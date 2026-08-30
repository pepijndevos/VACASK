#ifndef __ANCORESTB_DEFINED
#define __ANCORESTB_DEFINED

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

// AC small-signal stability analysis
// Assuming t=0 first solves for operating point (x0)
//   f(x0) = 0
// where f(x) is the resistive residual. 
// Then it linearizes the circuit by computing the resistive Jacobian Jr
// (Jacobian of f(x)) and the reactive Jacobian Jc (Jacobian of q(x)) 
// at x=x0 and solves
//   (Jr + omega Jc) X = U
// Probe is assumed to be connected with positive node to the DUT input side
// and the negative node to the DUT output side. This way the probe in inserted 
// serially into the feedback loop. 
// If the probe has a mfactor!=1 its current is considered to be the current 
// flowing across mfactor parallel instances of the probe. 
//
// The linearized equations are solved for two excitations
// - unity current injected at the probe's positive node (from the local ground)
//   with probe voltage set to 0
//   responses:
//   - probe current I2I (i.e. current flowing into the DUT output)
//   - DUT input voltage U1I measured wrt. local ground
// - unity voltage at the probe
//   responses: 
//   - probe current I2U (i.e. current flowing into the DUT output) 
//   - DUT input voltage U1U measured wrt. local ground
//
// This results in 4 parameters:
//   A = I2I    B = I2U
//   C = U1I    D = U1U
// 
// Admittance parameters of the DUT (assuming side 1 is the input side) 
// can be expressed as
//
//   y11 = (1+AD-BC-A-D)/C    y12 = (BC-AD+D)/C
//   y21 = (BC-AD+A)/C        y22 = (AD-BC)/C
//
// Forward/reverse/total open-loop gain can then be expressed as
//
//   Wf = y21/(y11+y22)       Wr = y12/(y11+y22)
//   W  = (y21+y12)/(y11+y22)
// 
// or
// 
//   Wf = (BC-AD+A)/(1+2(AD-BC)-A-D)    Wr = (BC-AD+D)/(1+2(AD-BC)-A-D)
//   W  = (2(BC-AD)+A+D)/(1+2(AD-BC)-A-D)
//
// Nyquist criterion can then be applied to W to determine 
// the DUT stability under feedback. 
//
// Frequency is swept across the given range. 
// See coreac.h for details on the frequency sweep. 
// 
// See coreop.h on how to specify nodesets. 

typedef struct ACStbParameters {
    OperatingPointParameters opParams;

    Id probe {""};      // Voltage source instance that breaks the loop
    Id localgnd {""};   // Local ground node (global ground by default)
    Real from {0};      // Start frequency for step and dec/oct/lin sweep
    Real to {0};        // Stop frequency for step and dec/oct/lin sweep
    Real step {0};      // Step size for step sweep
    Id mode {Id()};     // Mode for dec/oct/lin sweep
    Int points {0};     // Number of points for dec/oct/lin sweep
    Value values {0};   // Vector of values for values sweep
    Int write {1};      // Write the results to a file
                        // writeop is the write parameter of op core
                        // nodeset and store parameters of the op core are also exposed.
    
    ACStbParameters();
} ACStbParameters;

SIMPLE_ERRORCLASS(StbSweepSetupFailed, "Failed to set up the stability analysis frequency sweep.");

SIMPLE_ERRORCLASS(StbSweepComputeFailed, "Stability analysis sweep point computation failed.");

SIMPLE_ERRORCLASS(StbEvalAndLoadFailed, "Jacobian evaluation failed.");

SIMPLE_ERRORCLASS(StbMatrixError, "Stability analysis matrix error.");

SIMPLE_ERRORCLASS(StbSolutionNotFinite, "Solution component is not finite.");

SIMPLE_ERRORCLASS(StbOperatingPointFailed, "Operating point analysis failed.");

SIMPLE_ERRORCLASS(StbSingularMatrix, "Matrix is close to singular.");

SIMPLE_ERRORCLASS(StbBadFrequency, "Frequency value cannot be converted to real.");

SIMPLE_ERRORCLASS(StbBadProbe, "Probe must be a voltage source.");

SIMPLE_ERRORCLASS(StbBadLocalGnd, "Local ground node not found.");

SIMPLE_ERRORCLASS(StbDelayBindFailed, "Failed to bind delay lines to the stability analysis matrix.");

SIMPLE_ERRORCLASS(StbBindFailed, "Failed to bind the stability analysis matrix.");

ERRORCLASS(StbSweepAborted)
    double frequency;
    StbSweepAborted(double frequency) : frequency(frequency) {}
    std::string format() const {
        if (frequency >= 0) {
            return "Leaving frequency sweep at frequency=" + std::to_string(frequency) + ".";
        }
        return "Leaving frequency sweep.";
    }
END_ERRORCLASS(StbSweepAborted);


class ACStbCore : public AnalysisCore {
public:
    typedef ACStbParameters Parameters;

    enum class StbResult {
        Wf=0, Wr, W, y11, y12, y21, y22, COUNT
    };
    static constexpr int to_int(StbResult res) { return static_cast<int>(res); };
       
    ACStbCore(
        OutputDescriptorResolver& parentResolver, ACStbParameters& params, OperatingPointCore& opCore, Circuit& circuit,
        CommonData& commons,
        KluRealMatrix& dcJacobian, VectorRepository<double>& dcSolution, VectorRepository<double>& dcStates,
        KluComplexMatrix& acMatrix, Vector<Complex>& acSolution, Vector<Complex>& resultsVector,
        DelayLines& delayLines, DelayMatrixBindings<Complex*>& delayBindings
    );
    ~ACStbCore();
    
    ACStbCore           (const ACStbCore&)  = delete;
    ACStbCore           (      ACStbCore&&) = delete;
    ACStbCore& operator=(const ACStbCore&)  = delete;
    ACStbCore& operator=(      ACStbCore&&) = delete;

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
    ACStbParameters& params;
    Vector<Complex>& resultsVector;
    DelayLines& delayLines_;
    DelayMatrixBindings<Complex*>& delayBindings_;

    double frequency;
};

}

#endif
