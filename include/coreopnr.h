#ifndef __COREOPNR_DEFINED
#define __COREOPNR_DEFINED

#include "nrsolver.h"
#include "common.h"


namespace NAMESPACE {

struct PreprocessedUserForces {
    std::vector<double> nodeValues;
    std::vector<Node*> nodes;
    std::vector<Id> nodeIds;
    std::vector<double> nodePairValues;
    std::vector<std::tuple<Node*, Node*>> nodePairs;
    std::vector<std::tuple<Id, Id>> nodeIdPairs;

    PreprocessedUserForces() {};

    PreprocessedUserForces           (const PreprocessedUserForces&)  = delete;
    PreprocessedUserForces           (      PreprocessedUserForces&&) = default;
    PreprocessedUserForces& operator=(const PreprocessedUserForces&)  = delete;
    PreprocessedUserForces& operator=(      PreprocessedUserForces&&) = default;

    // Preprocess user specified nodeset/ic parameter values, store node ptrs instead of string names.
    // If syntax is bad, return error. Otherwise simply store the forced value,
    // node and id (or node pair and ids).
    // If node is not found, the corresponding force is ignored.
    // Checks if all extradiagonal sparsity map entries are present.
    // This is phase 1 of user forces processing.
    // Return value: ok, needs to add entries to sparsity map
    std::tuple<bool, bool> set(Circuit& circuit, ValueVector& userForces, ErrorConsumer& errors);

    void clear() {
        nodeValues.clear();
        nodes.clear();
        nodeIds.clear();
        nodePairValues.clear();
        nodePairs.clear();
        nodeIdPairs.clear();
    };
};

// PreprocessedUserForces::set() syntax errors, N is the 0-based position in the list.
ERRORCLASS(UserForcesExpectString)
    size_t position;
    UserForcesExpectString(size_t position) : position(position) {}
    std::string format() const { return "Expecting a string at position " + std::to_string(position) + "."; }
END_ERRORCLASS(UserForcesExpectString);

ERRORCLASS(UserForcesExpectStringOrValue)
    size_t position;
    UserForcesExpectStringOrValue(size_t position) : position(position) {}
    std::string format() const { return "Expecting a string, an integer, or a real at position " + std::to_string(position) + "."; }
END_ERRORCLASS(UserForcesExpectStringOrValue);

ERRORCLASS(UserForcesExpectValue)
    size_t position;
    UserForcesExpectValue(size_t position) : position(position) {}
    std::string format() const { return "Expecting an integer or a real at position " + std::to_string(position) + "."; }
END_ERRORCLASS(UserForcesExpectValue);


//
// Operating-point NR solver errors
//
// Node names are resolved to an Id from the offending Node* when the error is
// created, so no name resolver is needed.
//

ERRORCLASS(OpNrConflictNode)
    Id node;
    OpNrConflictNode(Id node) : node(node) {}
    std::string format() const {
        return "Conflicting forces for node '" + std::string(node) + "'.";
    }
END_ERRORCLASS(OpNrConflictNode);

ERRORCLASS(OpNrConflictDelta)
    Id node1;
    Id node2;
    OpNrConflictDelta(Id node1, Id node2) : node1(node1), node2(node2) {}
    std::string format() const {
        return "Forcing delta on node pair ('" + std::string(node1) + "', '"
               + std::string(node2) + "') conflicts previous forces.";
    }
END_ERRORCLASS(OpNrConflictDelta);

SIMPLE_ERRORCLASS(OpNrLoadForcesError, "Failed to load forces.");

ERRORCLASS(OpNrBadSolutionReference)
    Id ref;
    OpNrBadSolutionReference(Id ref) : ref(ref) {};
    std::string format() const { return "Bad solution reference '"+std::string(ref)+"'."; }
END_ERRORCLASS(OpNrBadSolutionReference);

ERRORCLASS(OpNrBadResidualReference)
    Id ref;
    OpNrBadResidualReference(Id ref) : ref(ref) {};
    std::string format() const { return "Bad residual reference '"+std::string(ref)+"'."; }
END_ERRORCLASS(OpNrBadResidualReference);


// Build the operating-point convergence report. Every input it needs is passed
// in explicitly, so the text can be produced without an OpNRSolver instance.
std::string formatOpConvergence(
    bool preventedConvergence, bool iterationConverged,
    bool residualCheck, double maxResidual, bool residualWithinTol, Id maxResidualNode,
    Int iteration, double maxDelta, bool deltaWithinTol, Id maxDeltaNode
);

// Carries a full snapshot of the convergence state so the report can be
// rendered later, off the error stack, via formatOpConvergence().
ERRORCLASS(OpNrConvergenceReport)
    bool preventedConvergence;
    bool iterationConverged;
    bool residualCheck;
    double maxResidual;
    bool residualWithinTol;
    Id maxResidualNode;
    Int iteration;
    double maxDelta;
    bool deltaWithinTol;
    Id maxDeltaNode;

    OpNrConvergenceReport(
        bool preventedConvergence, bool iterationConverged,
        bool residualCheck, double maxResidual, bool residualWithinTol, Id maxResidualNode,
        Int iteration, double maxDelta, bool deltaWithinTol, Id maxDeltaNode
    ) : preventedConvergence(preventedConvergence), iterationConverged(iterationConverged),
        residualCheck(residualCheck), maxResidual(maxResidual), residualWithinTol(residualWithinTol),
        maxResidualNode(maxResidualNode), iteration(iteration), maxDelta(maxDelta),
        deltaWithinTol(deltaWithinTol), maxDeltaNode(maxDeltaNode) {}

    std::string format() const {
        return formatOpConvergence(
            preventedConvergence, iterationConverged,
            residualCheck, maxResidual, residualWithinTol, maxResidualNode,
            iteration, maxDelta, deltaWithinTol, maxDeltaNode
        );
    }
END_ERRORCLASS(OpNrConvergenceReport);


class OpNRSolver : public NRSolver {
public:
    // By default OpNRSolver has 2 force slots
    // 0 .. continuation nodesets for sweep and homotopy
    //      cannot contain branch forces
    // 1 .. forces explicitly specified via nodeset analysis parameter
    //      can contain branch forces
    // When created in transient analysis for computing initial point
    // there is an extra slot
    // 2 .. forces specified via ic analysis parameter
    //      can contain branch forces
    // Slots containing branch forces affect the circuit topology. 
    // They need to be set before rebuild() is called. 
    // By default we have 2 slots. Transient analysis requests 3 slots. 
    OpNRSolver(
        Circuit& circuit, CommonData& commons, KluRealMatrix& jac,
        VectorRepository<double>& states, VectorRepository<double>& solution,
        DelayLines* delayLines, DelayMatrixBindings<double*>* delayBindings,
        NRSettings& settings, Int forcesSize=2
    );

    // Format convergence
    virtual std::string formatConvergence() const override;

    // Push an OpNrConvergenceReport snapshot onto the error stack
    virtual void pushConvergenceReport(ErrorConsumer& errors) override;

    // Set forces based on an annotated solution
    bool setForces(Int ndx, const AnnotatedSolution& solution, bool abortOnError, ErrorConsumer& errors);

    // Set forces based on preprocessed user forces
    bool setForces(Int ndx, const PreprocessedUserForces& preprocessed, bool uicMode, bool abortOnError, ErrorConsumer& errors);
    
    virtual void rebuildCheckResidualFlags();
    
    virtual bool rebuild(size_t nSolComp) override;
    virtual bool initialize(bool continuePrevious, ErrorConsumer& errors) override;
    virtual bool preIteration(bool continuePrevious) override;
    virtual bool postSolve(bool continuePrevious) override;
    virtual bool postConvergenceCheck(bool continuePrevious) override;
    virtual bool postIteration(bool continuePrevious) override;
    
    virtual std::tuple<bool, bool> buildSystem(bool continuePrevious, ErrorConsumer& errors) override;
    virtual std::tuple<bool, bool> checkResidual() override;
    virtual std::tuple<bool, bool> checkDelta() override;

    // Reset solution maxima and residual maxima
    void resetMaxima();

    // Initialize maxima from another OP NR solver
    void initializeMaxima(OpNRSolver& other);

    // Update historic and global maxima (across history and unknowns)
    void updateMaxima();

    // Return max historic solution vector (needed for LTE convergence check)
    const Vector<double>& historicMaxSolution() const { return historicMaxSolution_; };
    const Vector<double>& historicMaxResidualContribution() const { return historicMaxResidualContribution_; };

    // Return max global historic solution
    const Vector<double>& globalMaxSolution() const { return globalMaxSolution_; };
    const Vector<double>& globalMaxResidualContribution() const { return globalMaxResidualContribution_; };

    // Return point max solution
    const Vector<double>& pointMaxSolution() const { return pointMaxSolution_; };
    const Vector<double>& pointMaxResidualContribution() const { return pointMaxResidualContribution_; };
        
    double* maxResidualContribution() { return maxResidualContribution_.data(); }; 
    
    EvalSetup& evalSetup() { return evalSetup_; };
    LoadSetup& loadSetup() { return loadSetup_; };
    
    virtual void dumpSolution(std::ostream& os, double* solution, const char* prefix="") override;

protected:
    bool setForceOnUnknown(Forces& f, Node* node, double value, ErrorConsumer& errors);

    // Load forces
    bool loadForces(bool loadJacobian=true); 

    void loadShunts(double gshunt, bool loadJacobian=true);
    bool evalAndLoadWrapper(EvalSetup& evalSetup, LoadSetup& loadSetup, ErrorConsumer& errors);
    
    void setNodesetAndIcFlags(bool continuePrevious);

    CommonData& commons;
    
    Vector<double*> diagPtrs;
    std::vector<std::vector<std::tuple<double*, double*>>> extraDiags;
    
    EvalSetup evalSetup_;
    LoadSetup loadSetup_;

    DelayLines* delayLines_;
    DelayMatrixBindings<double*>* delayBindings_;
    
    // Passed from outside
    Circuit& circuit;
    VectorRepository<double>& states;
    
    // Internal structures
    Vector<double> dummyStates;
    Vector<double> deviceStates;

    // Internal structure for max residual contribution
    Vector<double> maxResidualContribution_; // maximal residual contributionm at this evaluation for each equation
    
    // What kind of tolerance reference to use
    bool historicSolRef;
    bool globalSolRef;
    bool historicResRef;
    bool globalResRef;
    
    // Historic and global maxima
    Vector<double> historicMaxResidualContribution_; // across produced solutions, maximal value for each equation, updated on external command
    Vector<double> globalMaxResidualContribution_;   // accross produced solutions, maximal value for each nature, updated on external command
    Vector<double> pointMaxResidualContribution_;    // at current solution, maximal value for each nature
    
    Vector<double> historicMaxSolution_; // across produced solutions, maximal value for each unknown, updated on external command
    Vector<double> globalMaxSolution_;   // across produced solutions, maximal value for each nature, updated on external command
    Vector<double> pointMaxSolution_;    // previous solution, maximal value for each nature

    // Flags indicating shuntable nodes
    Vector<bool> shuntable;

    // Flags indicating nodes for which residual can be checked
    Vector<bool> residualCheckable;

    // Solution natures and residual natures are currently limited to 
    //   0 .. voltage
    //   1 .. current
    
    // Convergence check auxiliary results
    double maxResidual; 
    double maxNormResidual; 
    double l2normResidual2;
    Node* maxResidualNode;
    bool residualWithinTol;
    double maxDelta; 
    double maxNormDelta; 
    Node* maxDeltaNode;
    bool deltaWithinTol;
};

}

#endif
