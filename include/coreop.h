#ifndef __ANCOREOP_DEFINED
#define __ANCOREOP_DEFINED

#include <memory>
#include "circuit.h"
#include "core.h"
#include "cscmatrix.h"
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
//
// If solve is 0 the circuit is not solved by the analyses that use this core. 
// They call evaluate() instead, which evaluates the circuit at the stored solution 
// named by nodeset (must be a string) so they can linearize at that point. 

typedef struct OperatingPointParameters {
    Value nodeset {Value("")}; // String specifying stored solution slot to read or
                               // list specifying nodesets
    String store {""};         // Name of stored solution slot to write
    Int write {1};             // Write the results to a file
    Id solver {};              // Linear solver to use, overrides tdsolver option
    Int solve {1};             // If true, solves the circuit, if false evaluates at the stored solution 
                               // given by nodeset. Not exposed to user. 

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

SIMPLE_ERRORCLASS(OpSolveNodesetType, "Nodeset must be a string when the operating point is not solved.");

SIMPLE_ERRORCLASS(OpNodesetNotFound, "Stored operating point solution given by nodeset not found.");

ERRORCLASS(OpNodesetIncomplete)
    Id node;
    OpNodesetIncomplete(Id node) : node(node) {}
    std::string format() const {
        return "Stored operating point solution given by nodeset has no value for node '" + std::string(node) + "'.";
    }
END_ERRORCLASS(OpNodesetIncomplete);

SIMPLE_ERRORCLASS(OpEvaluationFailed, "Evaluation at given nodeset failed.");

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
        CSCRealMatrix& jacobian, VectorRepository<double>& solution, VectorRepository<double>& states, 
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

    bool storeState(size_t ndx, bool storeDetails=true) override;
    bool restoreState(size_t ndx) override;

    std::tuple<bool, bool> runSolver(bool continuePrevious, ErrorConsumer& errors) override;

    // Evaluate at current solution, or at the stored solution given by nodeset if atNodeset is true
    // Does not factor the Jacobian, called after rebuild()
    bool evaluate(bool atNodeset, ErrorConsumer& errors);

    Int iterations() const override;
    Int iterationLimit(bool continuePrevious) const override;

    // Get solver
    OpNRSolver& solver() { return nrSolver; }; 

    // Set linear solver
    void setLinearSolver(RealSparseSolver* solver) { nrSolver.setLinearSolver(solver); };

    // Get linear solver
    RealSparseSolver* linearSolver() { return nrSolver.linearSolver(); };

    void enableNodesets(bool enable) { nodesetsMasterSwitch = enable; };
    
    void dump(std::ostream& os) const;

    static Id solutionTag;

protected:
    CSCRealMatrix& jac; // Resistive Jacobian
    VectorRepository<double>& solution; // Solution history
    VectorRepository<double>& states; // Circuit states

    DelayLines& delayLines_;
    DelayMatrixBindings<double*>& delayBindings_;

    CoreStateStorage* continueState;

    Forces stateNodesets;
    
    bool continuePrevious;
    bool converged_;

    std::unique_ptr<OutputRawfile> outfile;
    
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
