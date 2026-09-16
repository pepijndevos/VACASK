#ifndef __ANHBAC_DEFINED
#define __ANHBAC_DEFINED

#include "anhbsmsig.h"
#include "corehb.h"
#include "corehbac.h"
#include "parameterized.h"
#include "common.h"


namespace NAMESPACE {

// HBAC analysis data
class HBAcData {
public:
    static inline const Id analysisId = Id::createStatic("hbac");

protected:
    CSCBlockSparseComplexMatrix jacSpec;
    VectorRepository<Complex> hbSolution;
    CSCBlockSparseComplexMatrix acMatrix;
    Vector<Complex> acSolution;
};

// Constructor specialization
template<> HBSmallSignal<HBACCore, HBAcData>::HBSmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis);

// Resolve save specialization
template<> bool HBSmallSignal<HBACCore, HBAcData>::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors);

// Dump specialization
template<> void HBSmallSignal<HBACCore, HBAcData>::dump(std::ostream& os) const;

// Typedef HBAC
typedef HBSmallSignal<HBACCore, HBAcData> HBAC;

}

#endif
