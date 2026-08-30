#ifndef __ANCORE_DEFINED
#define __ANCORE_DEFINED

#include <unordered_map>
#include "circuit.h"
#include "output.h"
#include "hash.h"
#include "generator.h"
#include "progress.h"
#include "common.h"
#include "errorstack.h"

namespace NAMESPACE {

// Default value is Uninitialized
enum class CoreState { Uninitilized=0, Aborted, Stopped, Finished };


ERRORCLASS(CoreNodeNotFound)
    Id node;
    CoreNodeNotFound(Id node) : node(node) {}
    std::string format() const { return "Node '" + std::string(node) + "' not found."; }
END_ERRORCLASS(CoreNodeNotFound);

ERRORCLASS(CoreInstanceNotFound)
    Id instance;
    CoreInstanceNotFound(Id instance) : instance(instance) {}
    std::string format() const { return "Instance '" + std::string(instance) + "' not found."; }
END_ERRORCLASS(CoreInstanceNotFound);

SIMPLE_ERRORCLASS(CoreOutputSpec, "Output must be a single node or a node pair.");

SIMPLE_ERRORCLASS(CoreOutputType, "Output specification must be a string or a string vector.");

ERRORCLASS(CoreInstanceNotSource)
    Id instance;
    CoreInstanceNotSource(Id instance) : instance(instance) {}
    std::string format() const { return "Instance '" + std::string(instance) + "' is not an independent source."; }
END_ERRORCLASS(CoreInstanceNotSource);

ERRORCLASS(CoreSaveArguments)
    int count;
    CoreSaveArguments(int count) : count(count) {}
    std::string format() const {
        if (count == 0) return "Save directive does not accept arguments.";
        if (count == 1) return "Save directive requires one argument.";
        return "Save directive requires " + std::to_string(count) + " arguments.";
    }
END_ERRORCLASS(CoreSaveArguments);

ERRORCLASS(CoreOutvarNotFound)
    Id instance;
    Id outvar;
    CoreOutvarNotFound(Id instance, Id outvar) : instance(instance), outvar(outvar) {}
    std::string format() const {
        return "Output variable '" + std::string(outvar) + "' of instance '" + std::string(instance) + "' not found.";
    }
END_ERRORCLASS(CoreOutvarNotFound);

ERRORCLASS(CoreOutvarNotReadable)
    Id instance;
    Id outvar;
    CoreOutvarNotReadable(Id instance, Id outvar) : instance(instance), outvar(outvar) {}
    std::string format() const {
        return "Output variable '" + std::string(outvar) + "' of instance '" + std::string(instance) + "' not readable.";
    }
END_ERRORCLASS(CoreOutvarNotReadable);

ERRORCLASS(CoreAddOutputDescriptor)
    std::string what;
    CoreAddOutputDescriptor(std::string what) : what(std::move(what)) {}
    std::string format() const { return "Failed to add output descriptor for " + what + "."; }
END_ERRORCLASS(CoreAddOutputDescriptor);

// Core coroutine type
typedef Generator<CoreState> CoreCoroutine;


class OutputDescriptorResolver {
public:
    // Resolves output descriptor, adds output sorce to srcs
    // Returns true on success
    // Fail by default 
    virtual bool resolveOutputDescriptor(const OutputDescriptor& descr, Output::SourcesList& srcs, bool strict, ErrorConsumer& errors) { return false; };
};

class Analysis;

class CoreStateStorage {
public:
    CoreStateStorage() {};
    
    CoreStateStorage           (const CoreStateStorage&)  = delete;
    CoreStateStorage           (      CoreStateStorage&&) = default;
    CoreStateStorage& operator=(const CoreStateStorage&)  = delete;
    CoreStateStorage& operator=(      CoreStateStorage&&) = default;

    // Annotated solution
    AnnotatedSolution solution;
    
    // Is state coherent with current topology 
    // Becomes coherent when it is written, 
    // stops being coherent when makeStateIncoherent() is called.
    bool coherent;
    // Is state valid 
    // Becomes valid as soon as something is written into the slot. 
    bool valid;
};


// Analysis core, one analysis can have multiple analysis cores (i.e. op, tran, ...)
class AnalysisCore : public ProgressTracker {
public:
    AnalysisCore(OutputDescriptorResolver& parentResolver, Circuit& circuit, CommonData& commons);
    
    AnalysisCore           (const AnalysisCore&)  = delete;
    AnalysisCore           (      AnalysisCore&&) = delete;
    AnalysisCore& operator=(const AnalysisCore&)  = delete;
    AnalysisCore& operator=(      AnalysisCore&&) = delete;

    // Common data
    CommonData& commonData() { return commons; };

    // Clear output descriptors
    void clearOutputDescriptors();

    // Add an output descriptor to descriptors list of the core analysis
    // Silently ignore duplicates
    bool addOutputDescriptor(const OutputDescriptor& descr);
    bool addOutputDescriptor(OutputDescriptor&& descr);

    // Add output descriptors that are not based on saves but are specific
    // to analysis core (e.g. frequency, time). By default add nothing.
    bool addCoreOutputDescriptors(ErrorConsumer& errors) { return true; };

    // Add default output descriptors if no save has been provided
    bool addDefaultOutputDescriptors(ErrorConsumer& errors) { return true; };

    // Resolve all output descriptors into output sources
    // Delegate resolving of unknown decriptors to analysis
    bool resolveOutputDescriptors(bool strict, ErrorConsumer& errors) { return true; };

    // Core functionality

    // The following two are called before output descriptors are added or resolved

    // Check if we need to add sparsity map or states vector entries
    // Return value: ok, need mapping
    std::tuple<bool, bool> preMapping(ErrorConsumer& errors) { return std::make_tuple(true, false); };

    // Add sparsity map and states vector entries, set up forces on NR solver that require extradiagonal elements
    bool populateStructures(ErrorConsumer& errors) { return true; };

    // Called before core is run
    // - calls rebuild() for Jacobians
    // - binds instances to Jacobian entries
    // - calls NRSolver::rebuild() (if a NR solver is used)
    bool rebuild(ErrorConsumer& errors) { return true; };

    // Called before core is run (and once per sweep) to initalize output files
    bool initializeOutputs(const std::string& name, ErrorConsumer& errors) { return true; };

    // Runs the core
    bool run(bool continuePrevious) { return true; };

    // Called after core is run (and once per sweep) to finalieze and close output files
    bool finalizeOutputs(ErrorConsumer& errors) { return true; };

    // Called if analysis fails to remove output files
    bool deleteOutputs(Id name, ErrorConsumer& errors) { return true; };

    // Core state storage (used by continuation in sweeps and homotopy), do nothing by default
    // Return number of state slots
    virtual size_t stateStorageSize() const;
    // Allocate n slots, mark them as incoherent, return the number of first allocated slot
    virtual size_t allocateStateStorage(size_t n);
    // Deallocate n slots. If n>slots count or n=0, deallocate all
    virtual void deallocateStateStorage(size_t n=0);
    // Store state in slot ndx, mark as coherent
    // Override in derived classes if needed
    virtual bool storeState(size_t ndx, bool storeDetails=true) { return true; };
    // Restore state from slot ndx
    // Override in derived classes if needed
    virtual bool restoreState(size_t ndx) { return true; };
    // Make state in slot ndx incoherent
    virtual void makeStateIncoherent(size_t ndx);
    // Get core state
    CoreStateStorage& coreState(size_t ndx, bool storeDetails=true) { return coreStates.at(ndx); };

    // Homotopy interface
    // Return value: coverged, abort
    // Override in derived classes if needed
    virtual std::tuple<bool, bool> runSolver(bool continuePrevious, ErrorConsumer& errors) { return std::make_tuple(true, false); };
    // Return the number of solver iterations in last solver run
    // Override in derived classes if needed
    virtual Int iterations() const { return 0; };
    // Maximal number of allowed solver iterations
    // Override in derived classes if needed
    virtual Int iterationLimit(bool continuePrevious) const { return 0; };

    // Dump internals
    void dump(std::ostream& os) const;

    // Common handlers for save directive -> output descriptor(s)
    bool addAllUnknowns(const PTSave& save, ErrorConsumer& errors);
    bool addAllNodes(const PTSave& save, ErrorConsumer& errors);
    bool addNode(const PTSave& save, ErrorConsumer& errors);
    bool addFlow(const PTSave& save, ErrorConsumer& errors);
    bool addInstanceOutvar(const PTSave& save, ErrorConsumer& errors);
    bool addAllTfZin(const PTSave& save, std::unordered_map<Id,size_t>& nameMap, ErrorConsumer& errors);
    bool addTf(const PTSave& save, std::unordered_map<Id,size_t>& nameMap, ErrorConsumer& errors);
    bool addZin(const PTSave& save, std::unordered_map<Id,size_t>& nameMap, ErrorConsumer& errors);
    bool addYin(const PTSave& save, std::unordered_map<Id,size_t>& nameMap, ErrorConsumer& errors);
    bool addAllNoiseContribInst(const PTSave& save, bool details, ErrorConsumer& errors);
    bool addNoiseContribInst(const PTSave& save, bool details, ErrorConsumer& errors);

    // Common handlers for output descriptor -> output source
    // Always return true if strict=false, return false on error when struct=true
    bool addRealVarOutputSource(bool strict, Id name, const Vector<double>& solution, Id asName, ErrorConsumer& errors);
    bool addRealVarOutputSource(bool strict, Id name, const VectorRepository<double>& solution, Id asName, ErrorConsumer& errors);
    // Index of variable i -> index in solution i*stride+offset
    // In classical analyses, like AC, stride=1, offset=0
    bool addComplexVarOutputSource(bool strict, Id name, const Vector<Complex>& solution, size_t stride, size_t offset, Id asName, ErrorConsumer& errors);
    bool addComplexVarOutputSource(bool strict, Id name, const VectorRepository<Complex>& solution, size_t stride, size_t offset, Id asName, ErrorConsumer& errors);
    bool addOutvarOutputSource(bool strict, Id instance, Id outvar, Id asName, ErrorConsumer& errors);

protected:
    void expectedSaveArgumentsError(int expectedArgumentCount, ErrorConsumer& errors);

    CommonData& commons;

    std::tuple<bool, UnknownIndex, UnknownIndex> getDiffNodePair(Value& v, ErrorConsumer& errors);
    std::tuple<bool, Instance*> getExcitation(Id name, ErrorConsumer& errors);
    OutputDescriptorResolver& parentResolver;
    Circuit& circuit;
    Output::DescriptorList outputDescriptors;
    Output::SourcesList outputSources;
    std::unordered_map<Id,size_t> outputDescriptorIndices;
    // Number of save directives that produced at least one output descriptor
    size_t savesCount; 

    std::vector<CoreStateStorage> coreStates;
};

}

#endif
