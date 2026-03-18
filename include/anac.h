#ifndef __ANAC_DEFINED
#define __ANAC_DEFINED

#include "ansmsig.h"
#include "coreop.h"
#include "coreac.h"
#include "parameterized.h"
#include "ansmsig.h"
#include "common.h"


namespace NAMESPACE {

// AC analysis data
class AcData {
protected:
    KluComplexMatrix acMatrix; 
    Vector<Complex> acSolution;
};

// Constructor specialization
template<> SmallSignal<ACCore, AcData>::SmallSignal(Id name, Circuit& circuit, PTAnalysis& ptAnalysis);

// Resolve save specialization
template<> bool SmallSignal<ACCore, AcData>::resolveSave(const PTSave& save, bool verify, Status& s);

// Dump specialization
template<> void SmallSignal<ACCore, AcData>::dump(std::ostream& os) const;

// Typedef AC
typedef SmallSignal<ACCore, AcData> AC;

}

#endif
