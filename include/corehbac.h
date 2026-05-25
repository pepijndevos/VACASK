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
#include "devbase.h"
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

// (Quasi)periodic small-signal analysis based on harmonic balance
// 
// See corehb.h on how to specify nodesets. 

typedef struct HBACParameters {
    HBParameters hbParams;

    Real from {0};      // Start frequency for step and dec/oct/lin sweep
    Real to {0};        // Stop frequency for step and dec/oct/lin sweep
    Real step {0};      // Step size for step sweep
    Id mode {Id()};     // Mode for dec/oct/lin sweep
    Int points {0};     // Number of points for dec/oct/lin sweep
    Value values {0};   // Vector of values for values sweep
    Value outspur;      // specifies spurs where small signal response is observed
                        // - scalar real spur frequency
                        // - integer vector with tone weights defining a spur
                        // - list holding reals (frequency), integer vectors (tone weights)
                        // Default is empty list {} (all spurs). 
    Value maxharm {-1}; // Maximal absolute tone weight for spectrum truncation. 
                        // Integer or integer vector, scalar applies to all tones. 
                        // <0 keeps all tones computed by hb. 
    Real maxfreq {-1};  // Maximal absolute frequencyfor spectrum truncation
                        // <0 keeps all tones. 
    Int write {1};      // Write the results to a file
                        // writehb is the write parameter of hb core
                        // nodeset and store parameters of the hb core are also exposed. 
                        // solve parameter of hb core is exposed as hbsolve

    HBACParameters();
} HBACParameters;


class HBACCore : public AnalysisCore {
public:
    typedef HBACParameters Parameters;
    enum class HBACError {
        OK, 
        Sweeper, 
        SweepCompute, 
        MatrixError, 
        SolutionError, 
        HBError, 
        SingularMatrix, 
        BadFrequency, 
        MagLength, 
        PhaseLength, 
        SpurNotFound, 
    };

    typedef struct Excitation {
        Instance* source;
        Vector<size_t> spur;
        Vector<Complex> value;

    } Excitation;
       
    HBACCore(
        OutputDescriptorResolver& parentResolver, HBACParameters& params, HBCore& opCore, 
        Circuit& circuit, CommonData& commons, 
        KluBlockSparseComplexMatrix& jacSpec, 
        VectorRepository<Complex>& hbSolution, 
        KluBlockSparseComplexMatrix& acMatrix, Vector<Complex>& acSolution
    ); 
    ~HBACCore();
    
    HBACCore           (const HBACCore&)  = delete;
    HBACCore           (      HBACCore&&) = delete;
    HBACCore& operator=(const HBACCore&)  = delete;
    HBACCore& operator=(      HBACCore&&) = delete;

    // Format error, return false on error - this function is not cheap (works with strings)
    bool formatError(Status& s=Status::ignore) const; 

    bool addCoreOutputDescriptors(Status& s);
    bool addDefaultOutputDescriptors(Status& s);
    bool resolveOutputDescriptors(bool strict, Status& s=Status::ignore);

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
    // Bucket size is nf

    // Collect excitations
    bool collectExcitations();

    // Excitations
    Vector<Excitation> excitations;

    // Construct suffixes for small-signal frequency in HB spurs
    void constructSuffixes();

    // Construct omega vector with 2*pi*(f+f_n)
    void computeOmega(Real f);

    // Fill dense block
    void fillDenseBlock(const VectorView<Complex>& G, const VectorView<Complex>& C, const Vector<Real>& omega, DenseMatrixView<Complex>& block);

    // Build matrix
    void fillMatrix();

    // Evaluate (quasi)periodic operating point based on given nodeset
    bool evalOp();

    // Clear error
    void clearError() { AnalysisCore::clearError(); lastHBACError = HBACError::OK; }; 

    void setError(HBACError e) { lastHBACError = e; lastError = Error::OK; };
    HBACError lastHBACError;
    double errorFreq;
    Status errorStatus;
    Instance* errorInst;
    size_t errorSpur;

    VectorRepository<Complex>& hbSolution;
    KluBlockSparseComplexMatrix& jacSpec;
    KluBlockSparseComplexMatrix& acMatrix;
    Vector<Complex>& acSolution;
    HBACParameters& params;

    std::vector<std::string> suffixes;
    std::vector<int> spurIndices;
    std::vector<std::vector<Int>> spurSignatures;

    Vector<Real> omega;

    Spurs spurs_;

    double frequency;
};

}

#endif
