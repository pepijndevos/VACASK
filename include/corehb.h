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
    Real tstart {0};       // Starting time for colocation point selection
    Real nper {1};         // Number of lowest frequency periods across which colocation points are selected
    Id sample {Id()};      // Sampling mode (uniform, random, mixed), default is uniform. 
    Real shift {0.2};      // Sample shift in consecutive sample distance for uniform and mixed sampling
    String store {""};     // Name of stored solution slot to write
    String nodeset {""};   // String specifying stored solution slot to read
    Int solve {1};         // If true, solves the HB problem, if false evaluates at given stored solution
                           // Not exposed to user. 
    
    Int write {1};         // Write the results to a file
                             
    HBParameters();
} HBParameters;


class HbUnknownNameResolver : public NameResolver {
public:
    HbUnknownNameResolver(Circuit& circuit, size_t nb=0) : circuit(circuit), nb(nb) {};

    void setTimepointCount(size_t n) { nb = n; };

    virtual Id operator()(MatrixEntryIndex u) {
        if (nb>0) {
            return circuit.reprNode(u/nb+1)->name();
        } else {
            return Id();
        }
    };

private:
    Circuit& circuit;
    size_t nb;
};



SIMPLE_ERRORCLASS(HbNoAlgorithm, "No HB algorithm tried.");

SIMPLE_ERRORCLASS(HbSolverBuildFailed, "Failed to rebuild internal structures of nonlinear solver.");

SIMPLE_ERRORCLASS(HbSolverInitFailed, "Failed to initialize internal structures of nonlinear solver.");

SIMPLE_ERRORCLASS(HbInitialFailed, "Initial HB analysis failed.");

ERRORCLASS(HbHomotopyFailed)
    Int steps;
    HbHomotopyFailed(Int steps) : steps(steps) {}
    std::string format() const { return "Homotopy failed, " + std::to_string(steps) + " step(s) tried."; }
END_ERRORCLASS(HbHomotopyFailed);

SIMPLE_ERRORCLASS(HbEvaluationFailed, "Evaluation at given nodeset failed.");

SIMPLE_ERRORCLASS(HbFreqEmpty, "freq must have at least one component.");

SIMPLE_ERRORCLASS(HbFreqZeroExplicit, "Zero frequency should not be specified explicitly.");

SIMPLE_ERRORCLASS(HbNharmNonPositive, "nharm must be >0.");

SIMPLE_ERRORCLASS(HbNharmComponentNonPositive, "nharm components must be >0.");

SIMPLE_ERRORCLASS(HbNharmCountMismatch, "Number of nharm components must match number of freq components.");

SIMPLE_ERRORCLASS(HbNharmType, "nharm must be an integer or an integer vector.");

SIMPLE_ERRORCLASS(HbTruncateUnknown, "Unknown spectrum truncation method.");

SIMPLE_ERRORCLASS(HbSpursBuildFailed, "Failed to build the HB frequency spectrum.");

SIMPLE_ERRORCLASS(HbSpectrumTooSmall, "Too few frequencies in spectrum.");

SIMPLE_ERRORCLASS(HbSpectrumNoNonzero, "Spectrum must contain at least one nonzero frequency.");

SIMPLE_ERRORCLASS(HbSamplefacTooSmall, "samplefac must be >=1.");

SIMPLE_ERRORCLASS(HbSamplemodeUnknown, "Unknown samplmode.");

SIMPLE_ERRORCLASS(HbTransformMatrixFailed, "Failed to build transform matrix.");

SIMPLE_ERRORCLASS(HbColocationZeroNorm, "Zero norm encountered while computing colocation points.");

SIMPLE_ERRORCLASS(HbForwardTransformFailed, "Failed to compute forward transform matrix.");

SIMPLE_ERRORCLASS(HbNodesetNotFound, "Nodeset not found.");

SIMPLE_ERRORCLASS(HbVariableDelayUnsupported, "HB solver cannot handle circuits with variable delay.");

SIMPLE_ERRORCLASS(HbBindFailed, "Failed to bind the HB Jacobian.");

SIMPLE_ERRORCLASS(HbDelayBindFailed, "Failed to bind delay lines to the HB Jacobian.");


class HBCore : public AnalysisCore {
public:
    typedef HBParameters Parameters;

    HBCore(
        OutputDescriptorResolver& parentResolver, HBParameters& params, Circuit& circuit, CommonData& commons, 
        KluBlockSparseRealMatrix& jacColoc, KluBlockSparseRealMatrix& jacobian, VectorRepository<double>& solution, 
        DelayLines& delayLines, DelayMatrixBindings<DenseMatrixView<double>>& delayBindings
    );
    ~HBCore();
    
    HBCore           (const HBCore&)  = delete;
    HBCore           (      HBCore&&) = delete;
    HBCore& operator=(const HBCore&)  = delete;
    HBCore& operator=(      HBCore&&) = delete;

    bool addCoreOutputDescriptors(ErrorConsumer& errors);
    bool addDefaultOutputDescriptors(ErrorConsumer& errors);
    bool resolveOutputDescriptors(bool strict, ErrorConsumer& errors);

    bool rebuild(ErrorConsumer& errors);
    bool initializeOutputs(const std::string& name, ErrorConsumer& errors);
    bool run(bool continuePrevious, ErrorConsumer& errors);
    CoreCoroutine coroutine(bool continuePrevious, ErrorConsumer& errors);
    bool finalizeOutputs(ErrorConsumer& errors);
    bool deleteOutputs(Id name, ErrorConsumer& errors);

    bool buildGrid(ErrorConsumer& errors);
    bool buildColocation(ErrorConsumer& errors);
    bool buildAPFT(ErrorConsumer& errors);

    virtual bool storeState(size_t ndx, bool storeDetails=true);
    virtual bool restoreState(size_t ndx);
    
    virtual std::tuple<bool, bool> runSolver(bool continuePrevious, ErrorConsumer& errors);
    virtual Int iterations() const;
    virtual Int iterationLimit(bool continuePrevious) const;

    // Set stored solutiuon for evaluation, does not set up Jacobian to save memory
    // Bind circuit to jacColoc and evaluate at current solution
    bool evaluateAtNodeset(ErrorConsumer& errors);
    bool getFrequencyDomainJacobians(KluBlockSparseComplexMatrix& jacSpec, const Spurs& prunedSpurs);

    void dump(std::ostream& os) const;

    static Id truncateBox;
    static Id truncateDiamond;
    static Id truncateHybrid;
    static Id sampleUniform;
    static Id sampleRandom;
    static Id sampleMixed;

    static bool test();

    Spurs& spurs() { return spurs_; };

    static Id solutionTag;

protected:
    bool buildTransformMatrix(DenseMatrix<double>& XF, ErrorConsumer& errors);

    Int homotopySteps;

    // Block-sparse matrix with rectangular blocks for 
    // storing Jacobian values at colocation timepoints
    KluBlockSparseRealMatrix& jacColoc;

    // HB Jacobian
    KluBlockSparseRealMatrix& bsjac; 
    VectorRepository<Real>& solution; // Solution history

    DelayLines& delayLines_;
    DelayMatrixBindings<DenseMatrixView<double>>& delayBindings_;

    CoreStateStorage* continueState;

    OutputRawfile* outfile;
    
    bool converged_;
    
private:
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
    // Omega Gamma (APFT followed by differentiation in frequency domain), row major
    DenseMatrix<double> OmegaGamma;
    // IAPFT in co,umn major form
    DenseMatrix<double> GammaInvColumnMajor;
    // Pivot scratch for coeffs.factorAndInvert() in buildAPFT() (LAPACK ipiv storage type)
    std::vector<int> rowPerm_;

    // Resolves an HB block-matrix column index to a circuit node name for
    // matrix error messages. Owned here (bsjac only borrows it via
    // setResolver()); its timepoint count is filled in once known.
    HbUnknownNameResolver hbResolver_;

    // Declared last so spurs_, timepoints, and the transform matrices it
    // captures by reference are fully constructed before its init list runs.
    NRSettings nrSettings;
    HBNRSolver nrSolver;
};

}

#endif
