#ifndef __COREHB_DEFINED
#define __COREHB_DEFINED

#include "densematrix.h"
#include "klubsmatrix.h"
#include "spurs.h"
#include "core.h"
#include "corehbnr.h"
#include "outrawfile.h"
#include "corehbnr.h"
#include "value.h"
#include "common.h"

namespace NAMESPACE {

typedef struct HBParameters {
    RealVector freq {};    // Fundamental frequencies (f1, f2, ..., fd) 
    Value nharm {4};       // Number of harmonics for each fundamental frequency 
                           // (H1, H2, ..., Hd) in box truncation
                           // If scalar, applies to all frequencies, 
                           // if vector, components apply to corresponding frequencies. 
    Int immax {0};         // Maximal order of intermodulation products (IM) in diamond truncation. 
                           // If <=0, defaults to largest component of nharm. 
                           // If harmonics is a vector, defaults to its largest component. 
    Id truncate {Id()};    // Truncation scheme: 
                           // raw     .. values in freq are the frequencies in the spectrum
                           // box     .. box truncation
                           //   kj = 0..Hj, first nonzero kj must be >0
                           // diamond .. diamond truncation (default)
                           //   sum abs(kj) <= immax, first nonzero kj must be >0
    Real samplefac {5};    // Sampling factor in time domain (>=1). 
    Real nper {1};         // Number of lowest frequency periods across which colocation points are selected
    Id sample {Id()};      // Sampling mode (uniform, random, mixed), default is uniform. 
    Real shift {0.2};      // Sample shift in consecutive sample distance for uniform and mixed sampling
    String store {""};     // Name of stored solution slot to write
    String nodeset {""};   // String specifying stored solution slot to read
    
    Int write {1};         // Write the results to a file
                             
    HBParameters();
} HBParameters;


class HBCore : public AnalysisCore {
public:
    typedef HBParameters Parameters;
    enum class HBError {
        OK, 
        NoAlgorithm, 
        MatrixError, 
        SolverError,
        InitialHB, 
        Homotopy,
    };

    HBCore(
        OutputDescriptorResolver& parentResolver, HBParameters& params, Circuit& circuit, CommonData& commons, 
        KluBlockSparseRealMatrix& jacColoc, KluBlockSparseRealMatrix& jacobian, VectorRepository<double>& solution
    );
    ~HBCore();
    
    HBCore           (const HBCore&)  = delete;
    HBCore           (      HBCore&&) = delete;
    HBCore& operator=(const HBCore&)  = delete;
    HBCore& operator=(      HBCore&&) = delete;

    // Format error, return false on error - this function is not cheap (works with strings)
    bool formatError(Status& s=Status::ignore) const; 

    bool addCoreOutputDescriptors(Status& s);
    bool addDefaultOutputDescriptors(Status& s);
    bool resolveOutputDescriptors(bool strict, Status& s=Status::ignore);

    std::tuple<bool, bool> requestsRebuild(Status& s = Status::ignore);
    
    bool rebuild(Status& s=Status::ignore); 
    bool initializeOutputs(const std::string& name, Status& s=Status::ignore);
    bool run(bool continuePrevious);
    CoreCoroutine coroutine(bool continuePrevious);
    bool finalizeOutputs(Status& s=Status::ignore);
    bool deleteOutputs(Id name, Status& s=Status::ignore);

    bool buildGrid(Status& s=Status::ignore);
    bool buildColocation(Status& s=Status::ignore);
    bool buildAPFT(Status& s=Status::ignore);

    virtual bool storeState(size_t ndx, bool storeDetails=true);
    virtual bool restoreState(size_t ndx);
    
    virtual std::tuple<bool, bool> runSolver(bool continuePrevious);
    virtual Int iterations() const;
    virtual Int iterationLimit(bool continuePrevious) const;
        
    void dump(std::ostream& os) const;

    static Id truncateBox;
    static Id truncateDiamond;
    static Id truncateHybrid;
    static Id sampleUniform;
    static Id sampleRandom;
    static Id sampleMixed;

    static bool test();

    const Spurs& spurs() const { return spurs_; };

protected:
    // Clear error
    void clearError() { AnalysisCore::clearError(); lastHbError = HBError::OK; }; 

    void setError(HBError e) { lastHbError = e; lastError = Error::OK; };
    
    bool buildTransformMatrix(DenseMatrix<double>& XF, Status& s=Status::ignore);

    HBError lastHbError;
    Int homotopySteps; 

    // Block-sparse matrix with rectangular blocks for 
    // storing Jacobian values at colocation timepoints
    KluBlockSparseRealMatrix& jacColoc;

    // HB Jacobian
    KluBlockSparseRealMatrix& bsjac; 
    VectorRepository<Real>& solution; // Solution history

    CoreStateStorage* continueState;

    OutputRawfile* outfile;
    
    bool converged_;
    
private:
    NRSettings nrSettings;
    HBNRSolver nrSolver;

    // Temporary structures for collecting the phasors at a single frequency
    // before they are dumped. This vector has a bucket so that the output 
    // source code is the same as with other analyses. 
    VectorRepository<Complex> outputPhasors;
    Complex outputFreq;

    // Solution in frequency domain, nf phasors for each on of the n unknowns
    // NR solver resizes this vector. This vector has no bucket. 
    Vector<Complex> solutionFD;

    // Previous HB parameters to check if we need to rebuild()
    HBParameters oldParams;
    // Flag indicating rebuild() has not been called yet
    bool firstBuild;

    // HB parameters
    HBParameters& params;

    // Spurs
    Spurs spurs_;

    // Vectors and matrices without a bucket
    // Colocation timepoints (sorted)
    Vector<double> timepoints; 
    // Almost periodic Fourier transform
    DenseMatrix<double> APFT;
    // Inverse almost periodic Fourier transform
    DenseMatrix<double> IAPFT;
    // Derivative wrt time operator for time-domain vectors
    // Results in a time domain vector
    // Computed as IAPFT Omega APFT
    DenseMatrix<double> DDT;
    // DDT operator in column-major order (for cache locality)
    DenseMatrix<double> DDTcolMajor;
};

}

#endif
