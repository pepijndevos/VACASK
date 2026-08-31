#ifndef __ANCORESP_DEFINED
#define __ANCORESP_DEFINED

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

// AC small-signal S parameter analysis
// Assuming t=0 first solves for operating point (x0)
//   f(x0) = 0
// where f(x) is the resistive residual. 
// Then it linearizes the circuit by computing the resistive Jacobian Jr
// (Jacobian of f(x)) and the reactive Jacobian Jc (Jacobian of q(x)) 
// at x=x0 and solves
//   (Jr + omega Jc) X = U
//
// A port is defined with a voltage source and a series resistor connected
// to the positive terminal of the voltage source. Characteristic impedance 
// of a port is defined by resistor's resistance (taking into account $mfactor). 
// Positive node at the interface plane is at the remaining terminal of the resistor. 
// Negative node at the interface plane is the negative terminal of the voltage source. 
// 
// Analysis solves one circuit per port. For i-th port the excitation at that port 
// is nonzero, while others are zero. The obtained interface plane voltages 
// and voltage source currents at each port are used for computing incident (a)
// and reflected (b) waves. 
// Waves at j-th port for i-th excitation are stored in matrices A and B, 
// row j, column i. S-parameters link matrices A and B as follows. 
//   B = S A 
// 
// The S matrix is computed by solving this system. 
//
// Frequency is swept across the given range. 
// See coreac.h for details on the frequency sweep. 
// 
// See coreop.h on how to specify nodesets. 

typedef struct ACSPParameters {
    OperatingPointParameters opParams;

    Value ports {""};   // Array of identifiers, each port is given by voltage source name and a series resistor name
    Real from {0};      // Start frequency for step and dec/oct/lin sweep
    Real to {0};        // Stop frequency for step and dec/oct/lin sweep
    Real step {0};      // Step size for step sweep
    Id mode {Id()};     // Mode for dec/oct/lin sweep
    Int points {0};     // Number of points for dec/oct/lin sweep
    Value values {0};   // Vector of values for values sweep
    Int write {1};      // Write the results to a file
                        // writeop is the write parameter of op core
                        // nodeset and store parameters of the op core are also exposed. 

    ACSPParameters();
} ACSPParameters;

SIMPLE_ERRORCLASS(SpSweepSetupFailed, "Failed to set up the S-parameter frequency sweep.");

SIMPLE_ERRORCLASS(SpSweepComputeFailed, "S-parameter sweep point computation failed.");

SIMPLE_ERRORCLASS(SpEvalAndLoadFailed, "Jacobian evaluation failed.");

SIMPLE_ERRORCLASS(SpMatrixError, "S-parameter analysis matrix error.");

SIMPLE_ERRORCLASS(SpSolutionNotFinite, "Solution component is not finite.");

SIMPLE_ERRORCLASS(SpOperatingPointFailed, "Operating point analysis failed.");

SIMPLE_ERRORCLASS(SpSingularMatrix, "Matrix is close to singular.");

SIMPLE_ERRORCLASS(SpBadFrequency, "Frequency value cannot be converted to real.");

SIMPLE_ERRORCLASS(SpSingularSMatrix, "S-parameter matrix is singular.");

SIMPLE_ERRORCLASS(SpMatrixEntryNotFound, "Matrix entry not found.");

SIMPLE_ERRORCLASS(SpDelayBindFailed, "Failed to bind delay lines to the S-parameter matrix.");

SIMPLE_ERRORCLASS(SpBindFailed, "Failed to bind the S-parameter matrix.");

ERRORCLASS(SpSweepAborted)
    double frequency;
    SpSweepAborted(double frequency) : frequency(frequency) {}
    std::string format() const {
        if (frequency >= 0) {
            return "Leaving frequency sweep at frequency=" + std::to_string(frequency) + ".";
        }
        return "Leaving frequency sweep.";
    }
END_ERRORCLASS(SpSweepAborted);

ERRORCLASS(SpPortSourceNotFound)
    Id source;
    SpPortSourceNotFound(Id source) : source(source) {}
    std::string format() const { return "Port source '" + std::string(source) + "' not found."; }
END_ERRORCLASS(SpPortSourceNotFound);

ERRORCLASS(SpPortResistorNotFound)
    Id resistor;
    SpPortResistorNotFound(Id resistor) : resistor(resistor) {}
    std::string format() const { return "Port resistor '" + std::string(resistor) + "' not found."; }
END_ERRORCLASS(SpPortResistorNotFound);

ERRORCLASS(SpPortSourceNotVoltage)
    Id source;
    SpPortSourceNotVoltage(Id source) : source(source) {}
    std::string format() const { return "Port source '" + std::string(source) + "' must be a voltage source."; }
END_ERRORCLASS(SpPortSourceNotVoltage);

ERRORCLASS(SpPortResistorParams)
    Id resistor;
    SpPortResistorParams(Id resistor) : resistor(resistor) {}
    std::string format() const {
        return "Port resistor '" + std::string(resistor) + "' must have 'r', '$mfactor', and 'noisy' parameters.";
    }
END_ERRORCLASS(SpPortResistorParams);

ERRORCLASS(SpPortResistorType)
    Id resistor;
    SpPortResistorType(Id resistor) : resistor(resistor) {}
    std::string format() const {
        return "Port resistor '" + std::string(resistor) + "' must not be a source and must have 2 terminals.";
    }
END_ERRORCLASS(SpPortResistorType);

ERRORCLASS(SpPortResistorParamRead)
    Id resistor;
    std::string parameter;
    SpPortResistorParamRead(Id resistor, std::string parameter) : resistor(resistor), parameter(std::move(parameter)) {}
    std::string format() const {
        return "Failed to read resistor parameter '" + parameter + "' for '" + std::string(resistor) + "'.";
    }
END_ERRORCLASS(SpPortResistorParamRead);

ERRORCLASS(SpPortResistorParamType)
    Id resistor;
    std::string parameter;
    SpPortResistorParamType(Id resistor, std::string parameter) : resistor(resistor), parameter(std::move(parameter)) {}
    std::string format() const {
        return "Resistor parameter '" + parameter + "' of '" + std::string(resistor) + "' is of wrong type.";
    }
END_ERRORCLASS(SpPortResistorParamType);

ERRORCLASS(SpPortTopology)
    Id source;
    Id resistor;
    SpPortTopology(Id source, Id resistor) : source(source), resistor(resistor) {}
    std::string format() const {
        return "Port defined by '" + std::string(source) + "' and '" + std::string(resistor) +
               "' has incorrect topology. Positive source node must be connected to the resistor.";
    }
END_ERRORCLASS(SpPortTopology);


class ACSPCore : public AnalysisCore {
public:
    typedef ACSPParameters Parameters;

    ACSPCore(
        OutputDescriptorResolver& parentResolver, ACSPParameters& params, OperatingPointCore& opCore, Circuit& circuit,
        CommonData& commons,
        KluRealMatrix& dcJacobian, VectorRepository<double>& dcSolution, VectorRepository<double>& dcStates,
        KluComplexMatrix& acMatrix, Vector<Complex>& acSolution,
        DenseMatrix<Complex>& stMatrix,
        DelayLines& delayLines, DelayMatrixBindings<Complex*>& delayBindings
    );
    ~ACSPCore();
    
    ACSPCore           (const ACSPCore&)  = delete;
    ACSPCore           (      ACSPCore&&) = delete;
    ACSPCore& operator=(const ACSPCore&)  = delete;
    ACSPCore& operator=(      ACSPCore&&) = delete;

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
    ACSPParameters& params;
    DenseMatrix<Complex>& stMatrix;
    DenseMatrix<Complex> atMatrix;
    std::vector<int> rowPerm_; // Pivot scratch for atMatrix.factorAndLuSolve() (LAPACK ipiv storage type)
    
    Vector<Instance*> sourceVector;
    Vector<Instance*> resistorVector;
    Vector<std::tuple<UnknownIndex, UnknownIndex>> terminalsVector;
    Vector<double> z0;

    DelayLines& delayLines_;
    DelayMatrixBindings<Complex*>& delayBindings_;

    double frequency;

private:
    UnknownNameResolver resolver_;
};

}

#endif
