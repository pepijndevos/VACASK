#ifndef __ANCOREPNOISE_DEFINED
#define __ANCOREPNOISE_DEFINED

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

// Periodic (cyclostationary) small-signal noise analysis linearized around a
// PSS operating point, PAC's shooting/Toeplitz counterpart to HBNOISE.
//
// Like PAC (see corepac.h), there is a single fundamental and no
// intermodulation: the LPTV conversion matrix H(omega) and the noise
// modulation matrix M are both Toeplitz, built from the LPTV Jacobian's own
// Fourier coefficients G[k]/C[k] and the noise modulation functions'
// coefficients ma[k], k=0..maxharm (negative k is the conjugate, since the
// time-domain signals are real). No Spurs/mixing-map machinery is used.
//
// See corepss.h on how to specify nodesets/ic.

typedef struct PNoiseParameters {
    PssParameters pssParams;

    Value out {""};      // Output node or node pair (string vector)
    Id in {""};          // Input source
    Real from {0};       // Start frequency for step and dec/oct/lin sweep
    Real to {0};         // Stop frequency for step and dec/oct/lin sweep
    Real step {0};       // Step size for step sweep
    Id mode {Id()};      // Mode for dec/oct/lin sweep
    Int points {0};      // Number of points for dec/oct/lin sweep
    Value values {0};    // Vector of values for values sweep
    Value outharm {0};   // Output sideband where we observe noise (DC by default), signed harmonic
    Value inharm {0};    // Input sideband where we inject equivalent input noise (DC by default), signed harmonic
    Int truncharm {10};  // Number of harmonics to keep beyond DC
    Int write {1};       // Write the results to a file
                         // writepss is the write parameter of the pss core
                         // nodeset, ic and store parameters of the pss core
                         // are also exposed. solve parameter of the pss core
                         // is exposed as psssolve.
    Id solver {};        // Linear solver to use, overrides qpsmsigsolver option

    PNoiseParameters();
} PNoiseParameters;


ERRORCLASS(PNoiseInstanceNotFound)
    Id instance;
    PNoiseInstanceNotFound(Id instance) : instance(instance) {}
    std::string format() const { return "Instance '" + std::string(instance) + "' not found."; }
END_ERRORCLASS(PNoiseInstanceNotFound);

ERRORCLASS(PNoiseContribNotFound)
    Id instance;
    Id contribution;
    PNoiseContribNotFound(Id instance, Id contribution) : instance(instance), contribution(contribution) {}
    std::string format() const {
        return "Noise contribution '" + std::string(contribution) + "' of instance '" + std::string(instance) + "' not found.";
    }
END_ERRORCLASS(PNoiseContribNotFound);

SIMPLE_ERRORCLASS(PNoisePssFailed, "PSS analysis failed.");

SIMPLE_ERRORCLASS(PNoiseMaxharmInvalid, "Truncharm must be >=0.");

SIMPLE_ERRORCLASS(PNoiseOutharmNotFound, "Output harmonic not found.");

SIMPLE_ERRORCLASS(PNoiseInharmNotFound, "Input harmonic not found.");

SIMPLE_ERRORCLASS(PNoiseFrequencySidebandNotAllowed, "Frequency-valued sideband/harmonic is not allowed before the PSS period is known.");

SIMPLE_ERRORCLASS(PNoiseBadSidebandSpec, "Sideband/harmonic must be a signed integer harmonic or a matching real frequency.");

SIMPLE_ERRORCLASS(PNoiseSweepSetupFailed, "Failed to set up the PNOISE frequency sweep.");

SIMPLE_ERRORCLASS(PNoiseSweepComputeFailed, "PNOISE sweep point computation failed.");

SIMPLE_ERRORCLASS(PNoiseBadFrequency, "Frequency value cannot be converted to real.");

SIMPLE_ERRORCLASS(PNoiseMatrixError, "PNOISE matrix error.");

SIMPLE_ERRORCLASS(PNoiseSingularMatrix, "Matrix is close to singular.");

SIMPLE_ERRORCLASS(PNoiseSolutionNotFinite, "Solution component is not finite.");

SIMPLE_ERRORCLASS(PNoisePsdFailed, "Power spectral density evaluation failed.");

ERRORCLASS(PNoiseSweepAborted)
    double frequency;
    PNoiseSweepAborted(double frequency) : frequency(frequency) {}
    std::string format() const {
        if (frequency >= 0) {
            return "Leaving frequency sweep at frequency=" + std::to_string(frequency) + ".";
        }
        return "Leaving frequency sweep.";
    }
END_ERRORCLASS(PNoiseSweepAborted);


class PNoiseUnknownNameResolver : public NameResolver {
public:
    PNoiseUnknownNameResolver(Circuit& circuit, size_t nf=0) : circuit(circuit), nf(nf) {};

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


class PNoiseCore : public AnalysisCore {
public:
    typedef PNoiseParameters Parameters;

    PNoiseCore(
        OutputDescriptorResolver& parentResolver, PNoiseParameters& params, PssCore& pssCore,
        std::unordered_map<std::pair<Id, Id>, size_t>& contributionOffset,
        Circuit& circuit, CommonData& commons,
        CSCBlockSparseComplexMatrix& jacSpec,
        CSCBlockSparseComplexMatrix& acMatrix, Vector<Complex>& acSolution,
        Vector<double>& results, double& powerGain, double& outputNoise
    );
    ~PNoiseCore();

    PNoiseCore           (const PNoiseCore&)  = delete;
    PNoiseCore           (      PNoiseCore&&) = delete;
    PNoiseCore& operator=(const PNoiseCore&)  = delete;
    PNoiseCore& operator=(      PNoiseCore&&) = delete;

    bool addCoreOutputDescriptors(ErrorConsumer& errors);
    bool addDefaultOutputDescriptors(ErrorConsumer& errors);
    bool resolveOutputDescriptors(bool strict, ErrorConsumer& errors);

    void setLinearSolver(ComplexSparseSolver* solver) { cxSolver_ = solver; };

    bool rebuild(ErrorConsumer& errors);
    bool initializeOutputs(const std::string& name, ErrorConsumer& errors);
    CoreCoroutine coroutine(bool continuePrevious, ErrorConsumer& errors);
    bool run(bool continuePrevious, ErrorConsumer& errors);
    bool finalizeOutputs(ErrorConsumer& errors);
    bool deleteOutputs(Id name, ErrorConsumer& errors);

    void dump(std::ostream& os) const;

    PssCore& pssCore_;
    std::unique_ptr<OutputRawfile> outfile;

protected:
    // Construct omega vector with 2*pi*(f+h*f0), h=row-maxharm_, f0=1/T0
    void computeOmega(Real f);

    // Decode a Value (signed harmonic, or matching real frequency once the
    // PSS period is known) into a row index 0..2*maxharm_. Mirrors
    // PACCore::smsigFreqIndex (own copy: reads this core's maxharm_/pssCore_).
    std::tuple<bool, size_t> smsigFreqIndex(const Value& v, bool allowFrequency, ErrorConsumer& errors) const;

    // wr = M^H * zr^conj, Toeplitz M[i,m]=ma[i-m], without assembling M
    static void applyModulationAdjoint(
        Int maxharm, const VectorView<Complex>& ma, const Vector<Complex>& zr, Vector<Complex>& wr
    );

    CSCBlockSparseComplexMatrix& jacSpec;
    CSCBlockSparseComplexMatrix& acMatrix;
    Vector<Complex>& acSolution;

    // second Id is Id() -> total instance contribution
    std::unordered_map<std::pair<Id, Id>, size_t>& contributionOffset;
    // noise contributions
    Vector<double>& results;
    double& powerGain;
    double& outputNoise;

    // Frequency-domain noise modulation function spectra, one block of
    // maxharm_+1 components per modulation slot (Toeplitz basis, DC..maxharm_)
    Vector<Complex> noiseModulationSpec;

    PNoiseParameters& params;

    Vector<Real> omega;

    // Cached from params.truncharm after rebuild()
    Int maxharm_;

    int outHarmIndex;
    int inHarmIndex;

    double frequency;

private:
    PNoiseUnknownNameResolver resolver_;
    NoiseTable noiseTable_;

    ComplexSparseSolver* cxSolver_;
};

}

#endif
