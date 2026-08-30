#ifndef __ANSP_DEFINED
#define __ANSP_DEFINED

#include "ansmsig.h"
#include "coreop.h"
#include "coreacsp.h"
#include "parameterized.h"
#include "common.h"


namespace NAMESPACE {

SIMPLE_ERRORCLASS(SpPortsVectorEmpty, "Ports vector must define at least one port.");

SIMPLE_ERRORCLASS(SpPortsVectorOdd, "Ports vector must define an even number of components.");

// ACSP analysis data
class ACSPData {
protected:
    KluComplexMatrix acMatrix;
    Vector<Complex> acSolution;

    DenseMatrix<Complex> stMatrix;
};

// Constructor specialization
template<> SmallSignal<ACSPCore, ACSPData>::SmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis);

// Resolve save specialization
template<> bool SmallSignal<ACSPCore, ACSPData>::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors);

// Dump specialization
template<> void SmallSignal<ACSPCore, ACSPData>::dump(std::ostream& os) const;

// Typedef ACSP
typedef SmallSignal<ACSPCore, ACSPData> ACSP;

}

#endif
