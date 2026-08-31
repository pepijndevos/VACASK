#ifndef __ANCORENOISE_DEFINED
#define __ANCORENOISE_DEFINED

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

// Small-signal noise analysis
// Assuming t=0 first solves for operating point (x0)
//   f(x0) = 0
// where f(x) is the resistive residual. 
// Then it linearizes the circuit by computing the resistive Jacobian Jr
// (Jacobian of f(x)) and the reactive Jacobian Jc (Jacobian of q(x)) 
// at x=x0 and solves
//   (Jr + omega Jc) X = U
// for each noise source in the system. U is set to reflect the 
// source's magnitude 1 and phase 0. Additionally this is also done  
// for the input source to compute the power gain from input to output. 
// The obtained X is used for computing 
// - the contribution of that noise source 
//   to the output power spectral density 
// - the contribution of all noise sources within individual instances 
//   to the output power spectral density 
// - output power spectral density
// - gain from the input source to the output
// The output is given as a node or a pair of nodes. 
// Frequency is swept across the given range. 
// See coreac.h for details on the frequency sweep. 
// 
// See coreop.h on how to specify nodesets. 

typedef struct NoiseParameters {
    OperatingPointParameters opParams;
    
    Value out {""};   // Output node or node pair (string vector)
    Id in {""};       // Input source
    Real from {0};    // Start frequency for step and dec/oct/lin sweep
    Real to {0};      // Stop frequency for step and dec/oct/lin sweep
    Real step {0};    // Step size for step sweep
    Id mode {Id()};   // Mode for dec/oct/lin sweep
    Int points {0};   // Number of points for dec/oct/lin sweep
    Value values {0}; // Vector of values for values sweep
    Int write {1};    // Write the results to a file
                      // writeop is the write parameter of op core
                      // nodeset and store parameters of the op core are also exposed. 

    NoiseParameters();
} NoiseParameters;


ERRORCLASS(NoiseInstanceNotFound)
    Id instance;
    NoiseInstanceNotFound(Id instance) : instance(instance) {}
    std::string format() const { return "Instance '" + std::string(instance) + "' not found."; }
END_ERRORCLASS(NoiseInstanceNotFound);

ERRORCLASS(NoiseContribNotFound)
    Id instance;
    Id contribution;
    NoiseContribNotFound(Id instance, Id contribution) : instance(instance), contribution(contribution) {}
    std::string format() const {
        return "Noise contribution '" + std::string(contribution) + "' of instance '" + std::string(instance) + "' not found.";
    }
END_ERRORCLASS(NoiseContribNotFound);

SIMPLE_ERRORCLASS(NoiseSweepSetupFailed, "Failed to set up the noise analysis frequency sweep.");

SIMPLE_ERRORCLASS(NoiseSweepComputeFailed, "Noise analysis sweep point computation failed.");

SIMPLE_ERRORCLASS(NoiseEvalAndLoadFailed, "Jacobian evaluation failed.");

SIMPLE_ERRORCLASS(NoisePsdFailed, "Power spectral density evaluation failed.");

SIMPLE_ERRORCLASS(NoiseMatrixError, "Noise analysis matrix error.");

SIMPLE_ERRORCLASS(NoiseSolutionNotFinite, "Solution component is not finite.");

SIMPLE_ERRORCLASS(NoiseOperatingPointFailed, "Operating point analysis failed.");

SIMPLE_ERRORCLASS(NoiseSingularMatrix, "Matrix is close to singular.");

SIMPLE_ERRORCLASS(NoiseBadFrequency, "Frequency value cannot be converted to real.");

SIMPLE_ERRORCLASS(NoiseDelayBindFailed, "Failed to bind delay lines to the noise analysis matrix.");

SIMPLE_ERRORCLASS(NoiseBindFailed, "Failed to bind the noise analysis matrix.");

ERRORCLASS(NoiseSweepAborted)
    double frequency;
    NoiseSweepAborted(double frequency) : frequency(frequency) {}
    std::string format() const {
        if (frequency >= 0) {
            return "Leaving frequency sweep at frequency=" + std::to_string(frequency) + ".";
        }
        return "Leaving frequency sweep.";
    }
END_ERRORCLASS(NoiseSweepAborted);


class NoiseCore : public AnalysisCore {
public:
    typedef NoiseParameters Parameters;

    NoiseCore(
        OutputDescriptorResolver& parentResolver, NoiseParameters& params, OperatingPointCore& opCore,
        std::unordered_map<std::pair<Id, Id>, size_t>& contributionOffset,
        Circuit& circuit, CommonData& commons,
        KluRealMatrix& dcJacobian, VectorRepository<double>& dcSolution, VectorRepository<double>& dcStates,
        KluComplexMatrix& acMatrix, Vector<Complex>& acSolution,

        Vector<double>& results, double& powerGain, double& outputNoise,
        DelayLines& delayLines, DelayMatrixBindings<Complex*>& delayBindings
    );
    ~NoiseCore();
    
    NoiseCore           (const NoiseCore&)  = delete;
    NoiseCore           (      NoiseCore&&) = delete;
    NoiseCore& operator=(const NoiseCore&)  = delete;
    NoiseCore& operator=(      NoiseCore&&) = delete;

    bool addCoreOutputDescriptors(ErrorConsumer& errors);
    bool addDefaultOutputDescriptors(ErrorConsumer& errors);
    bool resolveOutputDescriptors(bool strict, ErrorConsumer& errors);

    bool rebuild(ErrorConsumer& errors);
    bool initializeOutputs(const std::string& name, ErrorConsumer& errors);
    CoreCoroutine coroutine(bool continuePrevious, ErrorConsumer& errors);
    bool run(bool continuePrevious, ErrorConsumer& errors);
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

    // second Id is Id() -> total instance contribution
    std::unordered_map<std::pair<Id, Id>, size_t>& contributionOffset; 
    // noise contributions
    Vector<double>& results;  
    double& powerGain;
    double& outputNoise;
    
    NoiseParameters& params;

    DelayLines& delayLines_;
    DelayMatrixBindings<Complex*>& delayBindings_;

    double frequency;

private:
    UnknownNameResolver resolver_;
};

}

#endif
