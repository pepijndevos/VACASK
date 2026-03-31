#ifndef __ANCORESP_DEFINED
#define __ANCORESP_DEFINED

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
    Int writeop {0};    // 1 = dump operating point to <analysisname>.op.raw;
    // Nodeset and store parameters of the operating point core 
    // are also exposed. 

    Int write {1};    // Write the results to a file

    ACSPParameters();
} ACSPParameters;

class ACSPCore : public AnalysisCore {
public:
    typedef ACSPParameters Parameters;
    enum class SPError {
        OK, 
        Sweeper, 
        SweepCompute, 
        EvalAndLoad, 
        MatrixError, 
        SolutionError, 
        OperatingPointError, 
        SingularMatrix, 
        BadFrequency, 
        SingularS, 
        MatrixEntryNotFound
    };

    ACSPCore(
        OutputDescriptorResolver& parentResolver, ACSPParameters& params, OperatingPointCore& opCore, Circuit& circuit, 
        CommonData& commons, 
        KluRealMatrix& dcJacobian, VectorRepository<double>& dcSolution, VectorRepository<double>& dcStates, 
        KluComplexMatrix& acMatrix, Vector<Complex>& acSolution, 
        DenseMatrix<Complex>& yMatrix, DenseMatrix<Complex>& stMatrix
    ); 
    ~ACSPCore();
    
    ACSPCore           (const ACSPCore&)  = delete;
    ACSPCore           (      ACSPCore&&) = delete;
    ACSPCore& operator=(const ACSPCore&)  = delete;
    ACSPCore& operator=(      ACSPCore&&) = delete;

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

    OperatingPointCore& opCore_;
    OutputRawfile* outfile;

protected:
    // Clear error
    void clearError() { AnalysisCore::clearError(); lastAcError = SPError::OK; }; 

    void setError(SPError e) { lastAcError = e; lastError = Error::OK; };
    SPError lastAcError;
    double errorFreq;
    Status errorStatus;

    VectorRepository<double>& dcSolution;
    VectorRepository<double>& dcStates;
    KluRealMatrix& dcJacobian;
    KluComplexMatrix& acMatrix;
    Vector<Complex>& acSolution;
    ACSPParameters& params;
    DenseMatrix<Complex>& yMatrix;
    DenseMatrix<Complex>& stMatrix;
    DenseMatrix<Complex> atMatrix;
    
    Vector<Instance*> sourceVector;
    Vector<Instance*> resistorVector;
    Vector<std::tuple<UnknownIndex, UnknownIndex>> terminalsVector;
    Vector<double> z0;

    double frequency;
};

}

#endif
