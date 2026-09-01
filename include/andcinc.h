#ifndef __ANDCINC_DEFINED
#define __ANDCINC_DEFINED

#include "ansmsig.h"
#include "coreop.h"
#include "coredcinc.h"
#include "parameterized.h"
#include "common.h"


namespace NAMESPACE {

// DcIncr analysis data
class DCIncrementalData {
public:
    static inline const Id analysisId = Id::createStatic("dcinc");

protected:
    Vector<double> incrementalSolution; // Incremental solution
};

// Constructor specialization
template<> SmallSignal<DCIncrementalCore, DCIncrementalData, false>::SmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis);

// Resolve save specialization
template<> bool SmallSignal<DCIncrementalCore, DCIncrementalData, false>::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors);

// Dump specialization
template<> void SmallSignal<DCIncrementalCore, DCIncrementalData, false>::dump(std::ostream& os) const;

// Typedef DCIncremental
typedef SmallSignal<DCIncrementalCore, DCIncrementalData, false> DCIncremental;

}

#endif
