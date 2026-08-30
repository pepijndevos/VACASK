#ifndef __ANCOREDCXF_DEFINED
#define __ANCOREDCXF_DEFINED

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

// DC small-signal transfer function analysis
// Assuming t=0 first solves for operating point (x0)
//   f(x0) = 0
// Then it linearizes the circuit by computing the resistive Jacobian Jr
// (Jacobian of f(x)) at x=x0 and solves
//   Jr dx = du
// for each independent source in the system. Here du is set to the 
// incremental excitation of 1 originating from a particular independent 
// source for which the equation is being solved. 
// The obtained dx is used for computing 
// - the small-signal transfer function from the independent source 's mag 
//   parameter to the output defined by a node or a pair of nodes, 
// - the small-signal input impedance and admittance felt by a source 
//   at its terminals
//
// See coreop.h on how to specify nodesets. 

typedef struct DCXFParameters {
    OperatingPointParameters opParams;
    
    Value out {""};   // Output node or node pair (string vector)
    Int write {1};    // Write the results to a file
                      // writeop is the write parameter of op core
                      // nodeset and store parameters of the op core are also exposed. 

    DCXFParameters();
} DCXFParameters;


ERRORCLASS(DcxfSourceNotFound)
    Id instance;
    DcxfSourceNotFound(Id instance) : instance(instance) {}
    std::string format() const { return "Source '" + std::string(instance) + "' not found."; }
END_ERRORCLASS(DcxfSourceNotFound);

ERRORCLASS(DcxfNotSource)
    Id instance;
    DcxfNotSource(Id instance) : instance(instance) {}
    std::string format() const { return "Instance '" + std::string(instance) + "' is not a source."; }
END_ERRORCLASS(DcxfNotSource);

SIMPLE_ERRORCLASS(DcxfMatrixError, "DC transfer function analysis matrix error.");

SIMPLE_ERRORCLASS(DcxfSolutionNotFinite, "Solution component is not finite.");

SIMPLE_ERRORCLASS(DcxfOperatingPointFailed, "Operating point analysis failed.");


class DCXFCore : public AnalysisCore {
public:
    typedef DCXFParameters Parameters;
    DCXFCore(
        OutputDescriptorResolver& parentResolver, DCXFParameters& params, OperatingPointCore& opCore, std::unordered_map<Id,size_t>& sourceIndex, 
        Circuit& circuit, CommonData& commons, KluRealMatrix& jacobian, Vector<double>& incrementalSolution, 
        std::vector<Instance*>& sources, Vector<double>& tf, Vector<double>& yin, 
        Vector<double>& zin
    ); 
    ~DCXFCore();
    
    DCXFCore           (const DCXFCore&)  = delete;
    DCXFCore           (      DCXFCore&&) = delete;
    DCXFCore& operator=(const DCXFCore&)  = delete;
    DCXFCore& operator=(      DCXFCore&&) = delete;

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

    std::unordered_map<Id,size_t>& sourceIndex;
    std::vector<Instance*>& sources;
    Vector<double>& tf;
    Vector<double>& yin;
    Vector<double>& zin;

    DCXFParameters& params;
};

}

#endif
