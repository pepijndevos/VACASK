#ifndef __ANSTB_DEFINED
#define __ANSTB_DEFINED

#include "ansmsig.h"
#include "coreop.h"
#include "coreacstb.h"
#include "parameterized.h"
#include "common.h"


namespace NAMESPACE {

// ACStb analysis data
class ACStbData {
protected:
    KluComplexMatrix acMatrix; 
    Vector<Complex> acSolution;

    Vector<Complex> resultsVector;
};

// Constructor specialization
template<> SmallSignal<ACStbCore, ACStbData>::SmallSignal(Id name, Circuit& circuit, PTAnalysis& ptAnalysis);

// Resolve save specialization
template<> bool SmallSignal<ACStbCore, ACStbData>::resolveSave(const PTSave& save, bool verify, Status& s);

// Dump specialization
template<> void SmallSignal<ACStbCore, ACStbData>::dump(std::ostream& os) const;

// Typedef ACStb
typedef SmallSignal<ACStbCore, ACStbData> ACStb;

}

#endif
