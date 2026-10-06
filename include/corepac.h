#ifndef __ANCOREPAC_DEFINED
#define __ANCOREPAC_DEFINED

#include <memory>
#include "circuit.h"
#include "core.h"
#include "corepss.h"
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

// Small-signal analysis linearized around a periodic steady-state (PSS)
// operating point with period T0. The circuit is linear periodically
// time-varying (LPTV), so a small-signal probe at frequency f produces
// response sidebands at f + h*f0 (f0=1/T0) for signed harmonic h in
// -maxharm..maxharm.
//
// Unlike HBAC (harmonic balance small signal), there is a single
// fundamental and no intermodulation, so the conversion matrix is Toeplitz:
// block (row,col) depends only on row-col=k, using the LPTV Jacobian's own
// Fourier coefficients G[k]/C[k] for k=0..maxharm (negative k is the
// conjugate of the |k|-th coefficient, since the time-domain Jacobians are
// real signals). No Spurs/mixing-map machinery is used.
//
// See corepss.h on how to specify nodesets/ic.

typedef struct PACParameters {
    PssParameters pssParams;

    Real from {0};       // Start frequency for step and dec/oct/lin sweep
    Real to {0};         // Stop frequency for step and dec/oct/lin sweep
    Real step {0};       // Step size for step sweep
    Id mode {Id()};      // Mode for dec/oct/lin sweep
    Int points {0};      // Number of points for dec/oct/lin sweep
    Value values {0};    // Vector of values for values sweep
    Value outharm;       // Signed harmonic sideband(s) to observe, relative to
                         // the swept probe frequency: scalar integer or integer
                         // vector. Default is empty list {} (all sidebands 
                         // up to truncharm).
    Int truncharm {10};  // Number of harmonics to keep beyond DC. The LPTV
                         // Jacobian is truncated beyond this order. 
                         // Must always be given. 
    Int write {1};       // Write the results to a file
                         // writepss is the write parameter of the pss core
                         // nodeset, ic and store parameters of the pss core
                         // are also exposed. solve parameter of the pss core
                         // is exposed as psssolve. The pss core's own linear
                         // solver (op-core solver) is exposed as pssopsolver,
                         // since "solver" here names PAC's own solver.
    Id solver {};        // Linear solver to use, overrides qpsmsigsolver option

    PACParameters();
} PACParameters;


SIMPLE_ERRORCLASS(PacMaxharmInvalid, "Maxharm must be >=0.");

ERRORCLASS(PacOutharmNotFound)
    size_t index;
    PacOutharmNotFound(size_t index) : index(index) {}
    std::string format() const { return "Output harmonic #" + std::to_string(index) + " not found."; }
END_ERRORCLASS(PacOutharmNotFound);

SIMPLE_ERRORCLASS(PacOutharmSingleNotFound, "Output harmonic not found.");

SIMPLE_ERRORCLASS(PacNoOutharm, "No output harmonic given.");

SIMPLE_ERRORCLASS(PacOutharmChanged, "Output harmonics are not allowed to change.");

SIMPLE_ERRORCLASS(PacFrequencySidebandNotAllowed, "Frequency-valued sideband/harmonic is not allowed before the PSS period is known.");

SIMPLE_ERRORCLASS(PacBadSidebandSpec, "Sideband/harmonic must be a signed integer harmonic or a matching real frequency.");

SIMPLE_ERRORCLASS(PacPssFailed, "PSS analysis failed.");

SIMPLE_ERRORCLASS(PacMatrixError, "PAC matrix error.");

SIMPLE_ERRORCLASS(PacSingularMatrix, "Matrix is close to singular.");

SIMPLE_ERRORCLASS(PacSolutionNotFinite, "Solution component is not finite.");

SIMPLE_ERRORCLASS(PacBadFrequency, "Frequency value cannot be converted to real.");

SIMPLE_ERRORCLASS(PacSweepSetupFailed, "Failed to set up the PAC frequency sweep.");

SIMPLE_ERRORCLASS(PacSweepComputeFailed, "PAC sweep point computation failed.");

ERRORCLASS(PacSweepAborted)
    double frequency;
    PacSweepAborted(double frequency) : frequency(frequency) {}
    std::string format() const {
        if (frequency >= 0) {
            return "Leaving frequency sweep at frequency=" + std::to_string(frequency) + ".";
        }
        return "Leaving frequency sweep.";
    }
END_ERRORCLASS(PacSweepAborted);

ERRORCLASS(PacMagLength)
    Id instance;
    PacMagLength(Id instance) : instance(instance) {}
    std::string format() const {
        return "smag length exceeds harmonic count for instance '" + std::string(instance) + "'.";
    }
END_ERRORCLASS(PacMagLength);

ERRORCLASS(PacPhaseLength)
    Id instance;
    PacPhaseLength(Id instance) : instance(instance) {}
    std::string format() const {
        return "sphase length exceeds harmonic count for instance '" + std::string(instance) + "'.";
    }
END_ERRORCLASS(PacPhaseLength);

ERRORCLASS(PacExcitationHarmNotFound)
    size_t spur;
    Id instance;
    PacExcitationHarmNotFound(size_t spur, Id instance) : spur(spur), instance(instance) {}
    std::string format() const {
        return "Excitation harmonic #" + std::to_string(spur) + " specified for instance '" + std::string(instance) + "' not found.";
    }
END_ERRORCLASS(PacExcitationHarmNotFound);


class PACUnknownNameResolver : public NameResolver {
public:
    PACUnknownNameResolver(Circuit& circuit, size_t nf=0) : circuit(circuit), nf(nf) {};

    void setFreqCount(size_t n) { nf = n; };

    Id operator()(MatrixEntryIndex u) override {
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


class PACCore : public AnalysisCore {
public:
    typedef PACParameters Parameters;

    typedef struct Excitation {
        Instance* source;
        Vector<size_t> harm;   // Row index into the 2*maxharm+1 sideband axis
        Vector<Complex> value;
    } Excitation;

    PACCore(
        OutputDescriptorResolver& parentResolver, PACParameters& params, PssCore& pssCore,
        Circuit& circuit, CommonData& commons,
        CSCBlockSparseComplexMatrix& jacSpec,
        CSCBlockSparseComplexMatrix& pacMatrix, Vector<Complex>& pacSolution
    );
    ~PACCore();

    PACCore           (const PACCore&)  = delete;
    PACCore           (      PACCore&&) = delete;
    PACCore& operator=(const PACCore&)  = delete;
    PACCore& operator=(      PACCore&&) = delete;

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

    PssCore& pssCore_;
    std::unique_ptr<OutputRawfile> outfile;

    // Shared implementations, public so unrelated cores (e.g. a future PAC
    // noise core) can call them without deriving from PACCore. Mirrors the
    // rationale for HBACCore::fillDenseBlock/fillMatrix.
    static void fillDenseBlock(
        Int maxharm,
        const VectorView<Complex>& G, const VectorView<Complex>& C, const Vector<Real>& omega,
        DenseMatrixView<Complex>& block
    );

    static void fillMatrix(
        Circuit& circuit, Int maxharm,
        CSCBlockSparseComplexMatrix& jacSpec, CSCBlockSparseComplexMatrix& pacMatrix,
        const Vector<Real>& omega
    );

protected:
    // Collect excitations
    bool collectExcitations(ErrorConsumer& errors);

    // Excitations
    Vector<Excitation> excitations;

    // Construct suffixes for signed harmonic numbers of the stored sidebands
    void constructSuffixes();

    // Construct omega vector with 2*pi*(f+h*f0), h=-maxharm_..maxharm_
    void computeOmega(Real f);

    // Decode a Value (signed integer harmonic, or real frequency matched
    // against h*f0 within tolerance) into a row index 0..2*maxharm_. Second
    // return value is meaningless if the first is false.
    std::tuple<bool, size_t> smsigFreqIndex(const Value& v, bool allowFrequency, ErrorConsumer& errors) const;

    CSCBlockSparseComplexMatrix& jacSpec;
    CSCBlockSparseComplexMatrix& pacMatrix;
    Vector<Complex>& pacSolution;

    PACParameters& params;

    // Row indices (into the 2*maxharm_+1 sideband axis) of the stored output
    // sidebands, and their signed-harmonic-number suffixes for output names.
    std::vector<int> outHarmRows;
    std::vector<std::string> suffixes;

    Vector<Real> omega;

    // Cached from params.maxharm after rebuild()
    Int maxharm_;

    double frequency;

private:
    PACUnknownNameResolver pacResolver_;

    ComplexSparseSolver* cxSolver_;
};

}

#endif
