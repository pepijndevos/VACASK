#ifndef __ANCOREHBAC_DEFINED
#define __ANCOREHBAC_DEFINED

#include "circuit.h"
#include "core.h"
#include "corehb.h"
#include "cscblkmatrix.h"
#include "solver.h"
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
                        // solver parameter of hb core is exposed as hbsolver
    Id solver {};       // Linear solver to use, overrides qpsmsigsolver option

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


class HBACUnknownNameResolver : public NameResolver {
public:
    HBACUnknownNameResolver(Circuit& circuit, size_t nf=0) : circuit(circuit), nf(nf) {};

    void setFreqCount(size_t n) { nf = n; };

    virtual Id operator()(MatrixEntryIndex u) {
        if (nf>0) {
            return circuit.reprNode(u/nf+1)->name();
        } else {
            return Id();
        }
    };

private:
    Circuit& circuit;
    size_t nf;
};


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
        CSCBlockSparseComplexMatrix& jacSpec, 
        VectorRepository<Complex>& hbSolution, 
        CSCBlockSparseComplexMatrix& acMatrix, Vector<Complex>& acSolution,
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

    void setLinearSolver(ComplexSparseSolver* solver) { cxSolver_ = solver; };

    bool rebuild(ErrorConsumer& errors);
    bool initializeOutputs(Id name, ErrorConsumer& errors);
    bool run(bool continuePrevious, ErrorConsumer& errors);
    CoreCoroutine coroutine(bool continuePrevious, ErrorConsumer& errors);
    bool finalizeOutputs(ErrorConsumer& errors);
    bool deleteOutputs(Id name, ErrorConsumer& errors);

    void dump(std::ostream& os) const;

    HBCore& hbCore_;
    OutputRawfile* outfile;

    // Shared implementations of fillDenseBlock/fillMatrix, usable by other
    // HB small-signal cores (e.g. HBNoiseCore) that need the same matrix
    // construction but do not derive from HBACCore. All state that the
    // instance methods above access implicitly through members is passed
    // in explicitly instead. Public so unrelated cores can call them.
    static void fillDenseBlock(
        const Spurs& spurs,
        const VectorView<Complex>& G, const VectorView<Complex>& C, const Vector<Real>& omega,
        DenseMatrixView<Complex>& block
    );

    static void fillMatrix(
        Circuit& circuit, const Spurs& spurs,
        CSCBlockSparseComplexMatrix& jacSpec, CSCBlockSparseComplexMatrix& acMatrix,
        const Vector<Real>& omega,
        DelayLines& delayLines, DelayMatrixBindings<DenseMatrixView<Complex>>& delayBindings
    );

    // Shared rebuild() plumbing, templated on ParametersStruct for reuse by
    // other cores; defined below (out-of-class, still in this header - see
    // there for why). Param name must differ from the Parameters typedef
    // above (GCC rejects the definition otherwise).
    template<typename ParametersStruct>
    static bool rebuildCore(
        ParametersStruct& params, HBCore& hbCore, Circuit& circuit,
        Spurs& spurs,
        CSCBlockSparseComplexMatrix& jacSpec, CSCBlockSparseComplexMatrix& acMatrix,
        DelayLines& delayLines, DelayMatrixBindings<DenseMatrixView<Complex>>& delayBindings,
        ErrorConsumer& errors
    );

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

    VectorRepository<Complex>& hbSolution;
    CSCBlockSparseComplexMatrix& jacSpec;
    CSCBlockSparseComplexMatrix& acMatrix;
    Vector<Complex>& acSolution;

    // Previous HB parameters to check if we need to rebuild()
    HBACParameters oldParams;

    HBACParameters& params;

    std::vector<std::string> suffixes;
    std::vector<int> spurIndices;
    std::vector<std::vector<Int>> spurSignatures;

    DelayLines& delayLines_;
    DelayMatrixBindings<DenseMatrixView<Complex>>& hbacDelayBindings_;

    Vector<Real> omega;

    Spurs spurs_;

    double frequency;

private:
    HBACUnknownNameResolver hbacResolver_;

    ComplexSparseSolver* cxSolver_;
};

// Out-of-class definition of the rebuildCore template (declared above).
// Lives in this header, not corehbac.cpp, so any .cpp that needs a new
// Parameters instantiation can add its own explicit instantiation without
// corehbac.cpp knowing about it. Output-spur selection (which is
// HBAC-specific) stays in HBACCore::rebuild(), not here.
template<typename ParametersStruct>
bool HBACCore::rebuildCore(
    ParametersStruct& params, HBCore& hbCore, Circuit& circuit,
    Spurs& spurs,
    CSCBlockSparseComplexMatrix& jacSpec, CSCBlockSparseComplexMatrix& acMatrix,
    DelayLines& delayLines, DelayMatrixBindings<DenseMatrixView<Complex>>& delayBindings,
    ErrorConsumer& errors
) {
    // Make a local copy of spurs structure
    spurs = Spurs(hbCore.spurs());

    // Get maxharm and maxfreq
    Vector<Int> maxharm(spurs.fundamentals().size());
    if (params.maxharm.isVector()) {
        // Vector maxharm
        if (params.maxharm.type()!=Value::Type::IntVec) {
            errors.push(HbAcMaxharmType{});
            return false;
        }
        if (params.maxharm.size()!=spurs.fundamentals().size()) {
            errors.push(HbAcMaxharmSize{});
            return false;
        }
        maxharm = params.maxharm.template val<IntVector>();
    } else {
        // Scalar maxharm
        if (params.maxharm.type()!=Value::Type::Int) {
            errors.push(HbAcMaxharmScalarType{});
            return false;
        }
        maxharm.assign(spurs.fundamentals().size(), params.maxharm.template val<Int>());
    }
    auto maxfreq = params.maxfreq;

    // Prune spurs
    if (!spurs.prune(maxharm, maxfreq)) {
        errors.push(HbAcSpurPruneFailed{});
        return false;
    }

    // Build mixing map
    if (!spurs.buildMixingMap(circuit.simulatorOptions().core().smsig_debug>0)) {
        errors.push(HbAcMixingMapFailed{});
        return false;
    }
    auto& stencil = spurs.mixingStencil();
    auto nf = stencil.nRows();

    // Jacobian spectral components
    if (!jacSpec.rebuild(circuit.sparsityMap(), circuit.unknownCount(), nf, 2, errors, true)) {
        return false;
    }

    // AC analysis matrix
    if (!acMatrix.rebuild(circuit.sparsityMap(), circuit.unknownCount(), nf, nf, errors)) {
        return false;
    }

    // Bind delay lines to acMatrix blocks. delayLines is shared with the
    // driving HBCore and already sized by its own rebuild(), run before
    // this by the owning analysis; delay values are filled during the HB
    // solve / evaluateAtNodeset() in the caller's coroutine(). The
    // (out,in) and (out,out) blocks must exist in the sparsity map
    // (absdelay declares the Jacobian entry), so this only fails on a
    // genuine topology error.
    if (!delayLines.bindToBlockMatrix(acMatrix, delayBindings, errors)) {
        errors.push(HbAcDelayBindFailed{});
        return false;
    }

    return true;
}

}

#endif
