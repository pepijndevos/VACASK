#ifndef __ANSP_DEFINED
#define __ANSP_DEFINED

#include "ansmsig.h"
#include "coreop.h"
#include "coreacsp.h"
#include "parameterized.h"
#include "common.h"


namespace NAMESPACE {

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
template<> bool SmallSignal<ACSPCore, ACSPData>::resolveSave(const PTSave& save, bool verify, Status& s);

// Dump specialization
template<> void SmallSignal<ACSPCore, ACSPData>::dump(std::ostream& os) const;

// Typedef ACSP
typedef SmallSignal<ACSPCore, ACSPData> ACSP;

}

#endif
