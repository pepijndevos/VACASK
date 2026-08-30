#ifndef __ANCOREDCINC_DEFINED
#define __ANCOREDCINC_DEFINED

#include "circuit.h"
#include "core.h"
#include "coreop.h"
#include "klumatrix.h"
#include "output.h"
#include "outrawfile.h"
#include "flags.h"
#include "outrawfile.h"
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

// DC incremental small-signal analysis
// Assuming t=0 first solves for operating point (x0)
//   f(x0) = 0
// Then it linearizes the circuit by computing the resistive Jacobian Jr
// (Jacobian of f(x)) at x=x0 and solves
//   Jr dx = du
// where du comprises incremental excitations specified by the mag parameters 
// of independent sources: mag can also be negative. 
// 
// See coreop.h on how to specify nodesets. 

typedef struct DCIncrementalParameters {
    OperatingPointParameters opParams;
    
    Int write {1};    // Write the results to a file
                      // writeop is the write parameter of op core
                      // nodeset and store parameters of the op core are also exposed. 

    DCIncrementalParameters();
} DCIncrementalParameters;


SIMPLE_ERRORCLASS(DcIncEvalAndLoadFailed, "Jacobian evaluation failed.");

SIMPLE_ERRORCLASS(DcIncMatrixError, "DC incremental analysis matrix error.");

SIMPLE_ERRORCLASS(DcIncSolutionNotFinite, "Solution component is not finite.");

SIMPLE_ERRORCLASS(DcIncOperatingPointFailed, "Operating point analysis failed.");


class DCIncrementalCore : public AnalysisCore {
public:
    typedef DCIncrementalParameters Parameters;

    DCIncrementalCore(
        OutputDescriptorResolver& parentResolver, DCIncrementalParameters& params, OperatingPointCore& opCore, Circuit& circuit, 
        CommonData& commons, KluRealMatrix& jacobian, Vector<double>& incrementalSolution
    ); 
    ~DCIncrementalCore();
    
    DCIncrementalCore           (const DCIncrementalCore&)  = delete;
    DCIncrementalCore           (      DCIncrementalCore&&) = delete;
    DCIncrementalCore& operator=(const DCIncrementalCore&)  = delete;
    DCIncrementalCore& operator=(      DCIncrementalCore&&) = delete;

    bool addDefaultOutputDescriptors(ErrorConsumer& errors);
    bool resolveOutputDescriptors(bool strict, ErrorConsumer& errors);

    bool rebuild(ErrorConsumer& errors);
    bool initializeOutputs(const std::string& name, ErrorConsumer& errors);
    CoreCoroutine coroutine(bool continuePrevious, ErrorConsumer& errors);
    bool run(bool continuePrevious, ErrorConsumer& errors);
    bool finalizeOutputs(ErrorConsumer& errors);
    bool deleteOutputs(Id name, ErrorConsumer& errors);

    void dump(std::ostream& os) const;

    OperatingPointCore& opCore_;
    OutputRawfile* outfile;

protected:
    static constexpr size_t bucketSize = 1;
    

    KluRealMatrix& jacobian;
    Vector<double>& incrementalSolution;
    DCIncrementalParameters& params;
};

}

#endif
