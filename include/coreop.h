#ifndef __ANCOREOP_DEFINED
#define __ANCOREOP_DEFINED

#include "circuit.h"
#include "core.h"
#include "klumatrix.h"
#include "output.h"
#include "outrawfile.h"
#include "flags.h"
#include "coreopnr.h"
#include "ansolution.h"
#include "generator.h"
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

// Operating point analysis
// Assuming t=0 solves
//   f(x) = 0
//
// Nodesets are specified as a list of values where each nodeset is given
// with 2 or 3 values
// - single node nodesets (...; "<node>"; value; ...)
// - differential nodeset (...; "<node1>"; "<node2>"; value; ...)

typedef struct OperatingPointParameters {
    Value nodeset {Value("")}; // String specifying stored solution slot to read or
                               // list specifying nodesets
    String store {""};         // Name of stored solution slot to write
    Int write {1};             // Write the results to a file

    OperatingPointParameters();
} OperatingPointParameters;


//
// Operating-point core errors
//
// None need a name resolver: they carry no node data, only a plain step count.
// The offending node/convergence detail, when there is one, is reported by a
// separate error pushed earlier by the NR solver or the matrix code.
//

SIMPLE_ERRORCLASS(OpInitialFailed, "Initial OP analysis failed.");

ERRORCLASS(OpHomotopyFailed)
    Int steps;
    OpHomotopyFailed(Int steps) : steps(steps) {}
    std::string format() const {
        return "Homotopy failed, " + std::to_string(steps) + " step(s) tried.";
    }
END_ERRORCLASS(OpHomotopyFailed);

SIMPLE_ERRORCLASS(OpNoAlgorithm, "No operating point algorithm tried.");

SIMPLE_ERRORCLASS(OpNodesetType, "Nodeset must be a list or a string.");

SIMPLE_ERRORCLASS(OpNodesetPreprocessFailed, "Failed to preprocess nodesets.");

// Wraps the Status message from Circuit::createJacobianEntry() when adding the
// extra-diagonal sparsity entry for a nodeset delta force fails.
ERRORCLASS(OpNodesetEntryError)
    std::string message;
    OpNodesetEntryError(std::string message) : message(std::move(message)) {}
    std::string format() const { return "Failed to add matrix entry for a nodeset delta force.\n" + message; }
END_ERRORCLASS(OpNodesetEntryError);

SIMPLE_ERRORCLASS(OpNrRebuildFailed, "Failed to rebuild internal structures of nonlinear solver.");


// Operating point core functionality, assumes all circuit parameters and simulator options have been set
// This core uses no other core
class OperatingPointCore : public AnalysisCore {
public:
    typedef OperatingPointParameters Parameters;
    
    OperatingPointCore(
        OutputDescriptorResolver& parentResolver, OperatingPointParameters& params, Circuit& circuit, 
        CommonData& commons, 
        KluRealMatrix& jacobian, VectorRepository<double>& solution, VectorRepository<double>& states, 
        DelayLines& delayLines, DelayMatrixBindings<double*>& delayBindings
    ); 
    ~OperatingPointCore();
    
    OperatingPointCore           (const OperatingPointCore&)  = delete;
    OperatingPointCore           (      OperatingPointCore&&) = delete;
    OperatingPointCore& operator=(const OperatingPointCore&)  = delete;
    OperatingPointCore& operator=(      OperatingPointCore&&) = delete;

    bool addDefaultOutputDescriptors(ErrorConsumer& errors);
    bool resolveOutputDescriptors(bool strict, ErrorConsumer& errors);

    std::tuple<bool, bool> preMapping(ErrorConsumer& errors);
    bool populateStructures(ErrorConsumer& errors);

    bool rebuild(ErrorConsumer& errors);
    bool initializeOutputs(const std::string& name, ErrorConsumer& errors);
    bool run(bool continuePrevious, ErrorConsumer& errors);
    CoreCoroutine coroutine(bool continuePrevious, ErrorConsumer& errors);
    bool finalizeOutputs(ErrorConsumer& errors);
    bool deleteOutputs(Id name, ErrorConsumer& errors);

    virtual bool storeState(size_t ndx, bool storeDetails=true);
    virtual bool restoreState(size_t ndx);
    
    virtual std::tuple<bool, bool> runSolver(bool continuePrevious, ErrorConsumer& errors);
    virtual Int iterations() const;
    virtual Int iterationLimit(bool continuePrevious) const;

    // Get solver
    OpNRSolver& solver() { return nrSolver; }; 

    void enableNodesets(bool enable) { nodesetsMasterSwitch = enable; };
    
    void dump(std::ostream& os) const;

    static Id solutionTag;

protected:
    KluRealMatrix& jac; // Resistive Jacobian
    VectorRepository<double>& solution; // Solution history
    VectorRepository<double>& states; // Circuit states

    DelayLines& delayLines_;
    DelayMatrixBindings<double*>& delayBindings_;

    CoreStateStorage* continueState;

    Forces stateNodesets;
    
    bool continuePrevious;
    bool converged_;

    OutputRawfile* outfile;
    
    PreprocessedUserForces preprocessedNodeset;

private:
    // Resolves a Jacobian row/column index to a circuit node name for matrix
    // error messages. Owned here (the matrix only borrows it via
    // jac.setResolver()), so it must outlive jac's use of it.
    UnknownNameResolver resolver_;

    NRSettings nrSettings;
    OpNRSolver nrSolver;

    OperatingPointParameters& params;

    bool nodesetsMasterSwitch;
};

}

#endif
