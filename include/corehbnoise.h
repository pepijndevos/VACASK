#ifndef __ANCOREHBNOISE_DEFINED
#define __ANCOREHBNOISE_DEFINED

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

// Periodic (cyclostationary) small-signal noise analysis based on harmonic balance
//
// See corehb.h on how to specify nodesets.

typedef struct HBNoiseParameters {
    HBParameters hbParams;

    Value out {""};      // Output node or node pair (string vector)
    Id in {""};          // Input source
    Real from {0};       // Start frequency for step and dec/oct/lin sweep
    Real to {0};         // Stop frequency for step and dec/oct/lin sweep
    Real step {0};       // Step size for step sweep
    Id mode {Id()};      // Mode for dec/oct/lin sweep
    Int points {0};      // Number of points for dec/oct/lin sweep
    Value values {0};    // Vector of values for values sweep
    Value outspur {0.0}; // Output spur where we observe noise (DC by default)
                         // - scalar real spur frequency
                         // - integer vector with tone weights defining a spur
    Value inspur {0.0};  // Input spur where we inject equivalent input noise (DC by default)
                         // - scalar real spur frequency
                         // - integer vector with tone weights defining a spur
    Value maxharm {-1};  // Maximal absolute tone weight for spectrum truncation.
                         // Integer or integer vector, scalar applies to all tones.
                         // <0 keeps all tones computed by hb.
    Real maxfreq {-1};   // Maximal absolute frequency for spectrum truncation
                         // <0 keeps all tones.
    Int write {1};       // Write the results to a file
                         // writehb is the write parameter of hb core
                         // nodeset and store parameters of the hb core are also exposed.
                         // solve parameter of hb core is exposed as hbsolve
                         // solver parameter of hb core is exposed as hbsolver
    Id solver {};        // Linear solver to use, overrides qpsmsigsolver option

    HBNoiseParameters();
} HBNoiseParameters;


ERRORCLASS(HbNoiseInstanceNotFound)
    Id instance;
    HbNoiseInstanceNotFound(Id instance) : instance(instance) {}
    std::string format() const { return "Instance '" + std::string(instance) + "' not found."; }
END_ERRORCLASS(HbNoiseInstanceNotFound);

ERRORCLASS(HbNoiseContribNotFound)
    Id instance;
    Id contribution;
    HbNoiseContribNotFound(Id instance, Id contribution) : instance(instance), contribution(contribution) {}
    std::string format() const {
        return "Noise contribution '" + std::string(contribution) + "' of instance '" + std::string(instance) + "' not found.";
    }
END_ERRORCLASS(HbNoiseContribNotFound);

SIMPLE_ERRORCLASS(HbNoiseHbFailed, "HB analysis failed.");

SIMPLE_ERRORCLASS(HbNoiseSpurPruneFailed, "Failed to prune the HB spur set.");

SIMPLE_ERRORCLASS(HbNoiseMixingMapFailed, "Failed to build the HB mixing map.");

SIMPLE_ERRORCLASS(HbNoiseOutspurNotFound, "Output spur not found.");

SIMPLE_ERRORCLASS(HbNoiseInspurNotFound, "Input spur not found.");

SIMPLE_ERRORCLASS(HbNoiseDelayBindFailed, "Failed to bind delay lines to the HBNOISE matrix.");

SIMPLE_ERRORCLASS(HbNoiseSweepSetupFailed, "Failed to set up the HBNOISE frequency sweep.");

SIMPLE_ERRORCLASS(HbNoiseSweepComputeFailed, "HBNOISE sweep point computation failed.");

SIMPLE_ERRORCLASS(HbNoiseBadFrequency, "Frequency value cannot be converted to real.");

SIMPLE_ERRORCLASS(HbNoiseMatrixError, "HBNOISE matrix error.");

SIMPLE_ERRORCLASS(HbNoiseSingularMatrix, "Matrix is close to singular.");

SIMPLE_ERRORCLASS(HbNoiseSolutionNotFinite, "Solution component is not finite.");

SIMPLE_ERRORCLASS(HbNoisePsdFailed, "Power spectral density evaluation failed.");

ERRORCLASS(HbNoiseSweepAborted)
    double frequency;
    HbNoiseSweepAborted(double frequency) : frequency(frequency) {}
    std::string format() const {
        if (frequency >= 0) {
            return "Leaving frequency sweep at frequency=" + std::to_string(frequency) + ".";
        }
        return "Leaving frequency sweep.";
    }
END_ERRORCLASS(HbNoiseSweepAborted);


class HBNoiseUnknownNameResolver : public NameResolver {
public:
    HBNoiseUnknownNameResolver(Circuit& circuit, size_t nf=0) : circuit(circuit), nf(nf) {};

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


class HBNoiseCore : public AnalysisCore {
public:
    typedef HBNoiseParameters Parameters;

    HBNoiseCore(
        OutputDescriptorResolver& parentResolver, HBNoiseParameters& params, HBCore& hbCore,
        std::unordered_map<std::pair<Id, Id>, size_t>& contributionOffset,
        Circuit& circuit, CommonData& commons,
        CSCBlockSparseComplexMatrix& jacSpec,
        VectorRepository<Complex>& hbSolution,
        CSCBlockSparseComplexMatrix& acMatrix, Vector<Complex>& acSolution,
        Vector<double>& results, double& powerGain, double& outputNoise,
        DelayLines& delayLines, DelayMatrixBindings<DenseMatrixView<Complex>>& delayBindings
    );
    ~HBNoiseCore();

    HBNoiseCore           (const HBNoiseCore&)  = delete;
    HBNoiseCore           (      HBNoiseCore&&) = delete;
    HBNoiseCore& operator=(const HBNoiseCore&)  = delete;
    HBNoiseCore& operator=(      HBNoiseCore&&) = delete;

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

    HBCore& hbCore_;
    OutputRawfile* outfile;

protected:
    // Construct omega vector with 2*pi*(f+f_n)
    void computeOmega(Real f);

    VectorRepository<Complex>& hbSolution;
    CSCBlockSparseComplexMatrix& jacSpec;
    CSCBlockSparseComplexMatrix& acMatrix;
    Vector<Complex>& acSolution;

    // second Id is Id() -> total instance contribution
    std::unordered_map<std::pair<Id, Id>, size_t>& contributionOffset;
    // noise contributions
    Vector<double>& results;
    double& powerGain;
    double& outputNoise;

    // Frequency-domain noise modulation function spectra, one slot per
    // modulated noise source (circuit.modulatedNoiseCount() of them)
    Vector<Complex> noiseModulationSpec;

    HBNoiseParameters& params;

    DelayLines& delayLines_;
    DelayMatrixBindings<DenseMatrixView<Complex>>& delayBindings_;

    Vector<Real> omega;

    Spurs spurs_;

    int outSpurIndex;
    int inSpurIndex;

    double frequency;

private:
    HBNoiseUnknownNameResolver resolver_;

    ComplexSparseSolver* cxSolver_;
};

}

#endif
