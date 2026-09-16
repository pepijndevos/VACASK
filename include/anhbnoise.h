#ifndef __ANHBNOISE_DEFINED
#define __ANHBNOISE_DEFINED

#include "anhbsmsig.h"
#include "corehb.h"
#include "corehbnoise.h"
#include "parameterized.h"
#include "common.h"


namespace NAMESPACE {

// HB noise analysis data
class HBNoiseData {
public:
    static inline const Id analysisId = Id::createStatic("hbnoise");

protected:
    CSCBlockSparseComplexMatrix jacSpec;
    VectorRepository<Complex> hbSolution;
    CSCBlockSparseComplexMatrix acMatrix;
    Vector<Complex> acSolution;

    std::unordered_map<std::pair<Id, Id>, size_t> contributionOffset;
    Vector<double> results;
    double powerGain;
    double outputNoise;
};

// Constructor specialization
template<> HBSmallSignal<HBNoiseCore, HBNoiseData>::HBSmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis);

// Resolve save specialization
template<> bool HBSmallSignal<HBNoiseCore, HBNoiseData>::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors);

// Dump specialization
template<> void HBSmallSignal<HBNoiseCore, HBNoiseData>::dump(std::ostream& os) const;

// Typedef HBNoise
typedef HBSmallSignal<HBNoiseCore, HBNoiseData> HBNoise;

}

#endif
