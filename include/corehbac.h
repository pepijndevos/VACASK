#ifndef __ANCOREHBAC_DEFINED
#define __ANCOREHBAC_DEFINED

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


SIMPLE_ERRORCLASS(HbAcMaxharmType, "Maxharm vector must be an integer vector.");

SIMPLE_ERRORCLASS(HbAcMaxharmSize, "Maxharm vector size must match the number of fundamental frequencies.");

SIMPLE_ERRORCLASS(HbAcMaxharmScalarType, "Maxharm scalar must be an integer.");

ERRORCLASS(HbAcOutspurNotFound)
    size_t index;
    HbAcOutspurNotFound(size_t index) : index(index) {}
    std::string format() const { return "Output spur #" + std::to_string(index) + " not found."; }
END_ERRORCLASS(HbAcOutspurNotFound);

SIMPLE_ERRORCLASS(HbAcOutspurSingleNotFound, "Output spur not found.");

SIMPLE_ERRORCLASS(HbAcNoOutspur, "No output spur given.");

SIMPLE_ERRORCLASS(HbAcOutspurChanged, "Output spurs are not allowed to change.");

SIMPLE_ERRORCLASS(HbAcSpurPruneFailed, "Failed to prune the HB spur set.");

SIMPLE_ERRORCLASS(HbAcMixingMapFailed, "Failed to build the HB mixing map.");

SIMPLE_ERRORCLASS(HbAcDelayBindFailed, "Failed to bind delay lines to the HBAC matrix.");

ERRORCLASS(HbAcMagLength)
    Id instance;
    HbAcMagLength(Id instance) : instance(instance) {}
    std::string format() const {
        return "smag length exceeds spur length for instance '" + std::string(instance) + "'.";
    }
END_ERRORCLASS(HbAcMagLength);

ERRORCLASS(HbAcPhaseLength)
    Id instance;
    HbAcPhaseLength(Id instance) : instance(instance) {}
    std::string format() const {
        return "sphase length exceeds spur length for instance '" + std::string(instance) + "'.";
    }
END_ERRORCLASS(HbAcPhaseLength);

ERRORCLASS(HbAcSpurNotFound)
    size_t spur;
    Id instance;
    HbAcSpurNotFound(size_t spur, Id instance) : spur(spur), instance(instance) {}
    std::string format() const {
        return "Spur #" + std::to_string(spur) + " specified for instance '" + std::string(instance) + "' not found.";
    }
END_ERRORCLASS(HbAcSpurNotFound);

SIMPLE_ERRORCLASS(HbAcHbFailed, "HB analysis failed.");

SIMPLE_ERRORCLASS(HbAcMatrixError, "HBAC matrix error.");

SIMPLE_ERRORCLASS(HbAcSingularMatrix, "Matrix is close to singular.");

SIMPLE_ERRORCLASS(HbAcSolutionNotFinite, "Solution component is not finite.");

SIMPLE_ERRORCLASS(HbAcBadFrequency, "Frequency value cannot be converted to real.");

SIMPLE_ERRORCLASS(HbAcSweepSetupFailed, "Failed to set up the HBAC frequency sweep.");

SIMPLE_ERRORCLASS(HbAcSweepComputeFailed, "HBAC sweep point computation failed.");

ERRORCLASS(HbAcSweepAborted)
    double frequency;
    HbAcSweepAborted(double frequency) : frequency(frequency) {}
    std::string format() const {
        if (frequency >= 0) {
            return "Leaving frequency sweep at frequency=" + std::to_string(frequency) + ".";
        }
        return "Leaving frequency sweep.";
    }
END_ERRORCLASS(HbAcSweepAborted);


class HBACCore : public AnalysisCore {
public:
    typedef HBACParameters Parameters;

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
        KluBlockSparseComplexMatrix& acMatrix, Vector<Complex>& acSolution,
        DelayLines& delayLines, DelayMatrixBindings<DenseMatrixView<Complex>>& hbacDelayBindings
    );
    ~HBACCore();
    
    HBACCore           (const HBACCore&)  = delete;
    HBACCore           (      HBACCore&&) = delete;
    HBACCore& operator=(const HBACCore&)  = delete;
    HBACCore& operator=(      HBACCore&&) = delete;

    bool addCoreOutputDescriptors(ErrorConsumer& errors);
    bool addDefaultOutputDescriptors(ErrorConsumer& errors);
    bool resolveOutputDescriptors(bool strict, ErrorConsumer& errors);

    bool rebuild(ErrorConsumer& errors);
    bool initializeOutputs(Id name, ErrorConsumer& errors);
    bool run(bool continuePrevious, ErrorConsumer& errors);
    CoreCoroutine coroutine(bool continuePrevious, ErrorConsumer& errors);
    bool finalizeOutputs(ErrorConsumer& errors);
    bool deleteOutputs(Id name, ErrorConsumer& errors);

    void dump(std::ostream& os) const;

    HBCore& hbCore_;
    OutputRawfile* outfile;

protected:
    // Bucket size is nf

    // Collect excitations
    bool collectExcitations(ErrorConsumer& errors);

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

    VectorRepository<Complex>& hbSolution;
    KluBlockSparseComplexMatrix& jacSpec;
    KluBlockSparseComplexMatrix& acMatrix;
    Vector<Complex>& acSolution;

    // Previous HB parameters to check if we need to rebuild()
    HBACParameters oldParams;
    // Flag indicating rebuild() has not been called yet
    bool firstBuild;

    HBACParameters& params;

    std::vector<std::string> suffixes;
    std::vector<int> spurIndices;
    std::vector<std::vector<Int>> spurSignatures;

    DelayLines& delayLines_;
    DelayMatrixBindings<DenseMatrixView<Complex>>& hbacDelayBindings_;

    Vector<Real> omega;

    Spurs spurs_;

    double frequency;
};

}

#endif
