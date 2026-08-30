#ifndef __ANSMSIG_DEFINED
#define __ANSMSIG_DEFINED

#include "an.h"
#include "coreop.h"
#include "parameterized.h"
#include "output.h"
#include "common.h"

namespace NAMESPACE {

template<typename CoreClass, typename DataMixin> class SmallSignal : public Analysis, public DataMixin {
public:
    typedef CoreClass::Parameters Parameters;

    // Will need to specialize the constructor
    SmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis) {};
    
    SmallSignal           (const SmallSignal&)  = delete;
    SmallSignal           (      SmallSignal&&) = delete;
    SmallSignal& operator=(const SmallSignal&)  = delete;
    SmallSignal& operator=(      SmallSignal&&) = delete;

    virtual Parameterized& parameters() { return params; }; 
    virtual const Parameterized& parameters() const { return params; }; 

    // Factory function for operating point analysis
    static Analysis* create(PTAnalysis& ptAnalysis, Circuit& circuit, Status& s=Status::ignore) {
        auto* an = new SmallSignal<CoreClass, DataMixin>(ptAnalysis.name(), circuit, ptAnalysis);
        return an;
    }; 

    virtual void dump(std::ostream& os) const;

protected:
    // Add output descriptors common to all cores, no error message is returned 
    virtual bool addCommonOutputDescriptor(const OutputDescriptor& desc);

    // Add core-specific output descriptors, no error message is returned
    virtual bool addCoreOutputDescriptors(ErrorConsumer& errors);

    // Add operating point output descriptor(s) based on save, generates error message if verification is required
    // Returns ok, resolved
    std::tuple<bool, bool> resolveOpSave(const PTSave& save, bool verify, ErrorConsumer& errors);

    // Add operating point output descriptor(s) based on save, needs to be specialized
    virtual bool resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) { return true; };

    // Add default output descriptors if no save is specified
    // No error message is returned
    virtual bool addDefaultOutputDescriptors(ErrorConsumer& errors);

    // Remove all output descriptors from all cores, transfer parameters from smsig to op
    // No error message is returned
    virtual void clearOutputDescriptors();

    // Resolve oputput descriptors into output sources across all cores
    virtual bool resolveOutputDescriptors(bool strict, ErrorConsumer& errors);

    // Check if we need to add analysis-specific matrix entries or states
    // By default only op core needs this
    virtual std::tuple<bool, bool> preMapping(ErrorConsumer& errors);

    // Add analysis-specific matrix entries and states
    // By default only op core needs this
    virtual bool populateStructures(ErrorConsumer& errors);

    // Rebuild cores
    virtual bool rebuildCores(ErrorConsumer& errors);

    // Initialize outputs
    virtual bool initializeOutputs(ErrorConsumer& errors);

    // Analysis core
    virtual AnalysisCore& analysisCore() { return smsigCore; };

    // Create core coroutine
    virtual CoreCoroutine coreCoroutine(bool continuePrevious, ErrorConsumer& errors) {
        return std::move(smsigCore.coroutine(continuePrevious, errors));
    };
    
    // Finalize outputs
    virtual bool finalizeOutputs(ErrorConsumer& errors);

    // Delete outputs
    virtual bool deleteOutputs(ErrorConsumer& errors);
    
    // Analysis state storage for continuation in sweeps
    // Only op core has state storage
    virtual size_t analysisStateStorageSize() const;
    virtual size_t allocateAnalysisStateStorage(size_t n);
    virtual void deallocateAnalysisStateStorage(size_t n=0);
    virtual bool storeState(size_t ndx, bool storeDetails=true);
    virtual bool restoreState(size_t ndx);
    virtual void makeStateIncoherent(size_t ndx);

    IStruct<Parameters> params;

    // Declared before the cores so the references they bind in their init lists
    // (jac, solution, states) refer to fully-constructed members. opCore precedes
    // smsigCore because smsigCore binds a reference to opCore. The DataMixin base
    // members (acMatrix, acSolution, ...) are constructed before these and are safe.
    KluRealMatrix jac; // Resistive Jacobian
    VectorRepository<double> solution; // Solution history
    VectorRepository<double> states; // Circuit states
    DelayLines delayLines_;
    DelayMatrixBindings<double*> opDelayBindings_;
    DelayMatrixBindings<Complex*> smsigDelayBindings_;

    OperatingPointCore opCore;
    CoreClass smsigCore;
};

template<typename CoreClass, typename DataMixin> 
bool SmallSignal<CoreClass, DataMixin>::addCommonOutputDescriptor(const OutputDescriptor& desc) {  
    // False is returned if the descriptor is already there
    bool s1 = opCore.addOutputDescriptor(desc);
    bool s2 = smsigCore.addOutputDescriptor(desc);
    return  s1 && s2;
}

template<typename CoreClass, typename DataMixin> 
bool SmallSignal<CoreClass, DataMixin>::addCoreOutputDescriptors(ErrorConsumer& errors) {
    // False is returned if the descriptor is already there
    if (!opCore.addCoreOutputDescriptors(errors)) {
        return false;
    }
    if (!smsigCore.addCoreOutputDescriptors(errors)) {
        return false;
    }
    return true;
}

template<typename CoreClass, typename DataMixin>
bool SmallSignal<CoreClass, DataMixin>::addDefaultOutputDescriptors(ErrorConsumer& errors) {
    // Must be invoked on all cores regardless of return value
    auto s1 = opCore.addDefaultOutputDescriptors(errors);
    auto s2 = smsigCore.addDefaultOutputDescriptors(errors);
    return s1 && s2;
}

template<typename CoreClass, typename DataMixin> 
void SmallSignal<CoreClass, DataMixin>::clearOutputDescriptors() {
    // Must be invoked on all cores regardless of return value
    opCore.clearOutputDescriptors();
    smsigCore.clearOutputDescriptors();
}


// Resolve output descriptors to output sources across all cores
template<typename CoreClass, typename DataMixin> 
bool SmallSignal<CoreClass, DataMixin>::resolveOutputDescriptors(bool strict, ErrorConsumer& errors) {
    // Any error causes immediate exit if strict is true
    // Before exit an error message is formatted and status is set
    if (!opCore.resolveOutputDescriptors(strict, errors)) {
        if (strict) {
            return false;
        }
    }
    if (!smsigCore.resolveOutputDescriptors(strict, errors)) {
        if (strict) {
            return false;
        }
    }
    return true;
}

template<typename CoreClass, typename DataMixin>
std::tuple<bool, bool> SmallSignal<CoreClass, DataMixin>::resolveOpSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
    // OP saves
    static const auto idOpDefault = Id("opdefault");
    static const auto idOpFull = Id("opfull");
    static const auto idV = Id("v");
    static const auto idI = Id("i");
    static const auto idP = Id("p");

    // When verification is not required, save-resolution errors are not fatal
    // and must not reach the caller's error consumer.
    ErrorConsumer sink;
    ErrorConsumer& e1 = verify ? errors : sink;

    bool st = true;
    if (save.typeName() == idOpDefault) {
        st = opCore.addAllUnknowns(save, e1);
    } else if (save.typeName() == idOpFull) {
        st = opCore.addAllNodes(save, e1);
    } else if (save.typeName() == idV) {
        st = opCore.addNode(save, e1);
    } else if (save.typeName() == idI) {
        st = opCore.addFlow(save, e1);
    } else if (save.typeName() == idP) {
        st = opCore.addInstanceOutvar(save, e1);
    } else {
        // Do not know how to handle this save
        if (verify) {
            errors.push(AnUnsupportedSaveDirective{save.location()});
        }
        // Return false, false (error, not handled)
        return std::make_tuple(false, false);
    }
    // Error detected in opCore save, verification required
    if (verify && !st) {
        errors.push(AnSaveDirectiveLocation{save.location()});
    }
    // Status, handled
    return std::make_tuple(st, true);
}

template<typename CoreClass, typename DataMixin>
std::tuple<bool, bool> SmallSignal<CoreClass, DataMixin>::preMapping(ErrorConsumer& errors) {
    auto [ok, needsMapping] = opCore.preMapping(errors);
    if (!ok) {
        return std::make_tuple(false, needsMapping);
    }
    auto [ok1, map1] = smsigCore.preMapping(errors);
    return std::make_tuple(ok&&ok1, needsMapping||map1);
}

template<typename CoreClass, typename DataMixin>
bool SmallSignal<CoreClass, DataMixin>::populateStructures(ErrorConsumer& errors) {
    auto ok = opCore.populateStructures(errors);
    if (!ok) {
        return false;
    }
    return smsigCore.populateStructures(errors);
}

template<typename CoreClass, typename DataMixin>
bool SmallSignal<CoreClass, DataMixin>::rebuildCores(ErrorConsumer& errors) {
    // Create Jacobian - it is common to both cores, so we need to rebuild it here
    if (!jac.rebuild(circuit.sparsityMap(), circuit.unknownCount(), errors)) {
        return false;
    }

    // Any error aborts immediately
    if (!opCore.rebuild(errors)) {
        return false;
    }
    if (!smsigCore.rebuild(errors)) {
        return false;
    }

    return true;
}

template<typename CoreClass, typename DataMixin>
bool SmallSignal<CoreClass, DataMixin>::initializeOutputs(ErrorConsumer& errors) {
    // Any error exits immediately
    if (!opCore.initializeOutputs(prefixedName_+".op", errors)) {
        return false;
    }
    if (!smsigCore.initializeOutputs(prefixedName_, errors)) {
        return false;
    }
    return true;
}

template<typename CoreClass, typename DataMixin>
bool SmallSignal<CoreClass, DataMixin>::finalizeOutputs(ErrorConsumer& errors) {
    // Finalization has to be performed on all cores, regardless of errors
    auto ok1 = opCore.finalizeOutputs(errors);
    auto ok2 = smsigCore.finalizeOutputs(errors);
    return ok1 && ok2;
}

template<typename CoreClass, typename DataMixin>
bool SmallSignal<CoreClass, DataMixin>::deleteOutputs(ErrorConsumer& errors) {
    // Output needs to be deleted for all cores
    auto ok1 = opCore.deleteOutputs(prefixedName_+".op", errors);
    auto ok2 = smsigCore.deleteOutputs(prefixedName_, errors);
    return ok1 && ok2;
}

template<typename CoreClass, typename DataMixin> 
size_t SmallSignal<CoreClass, DataMixin>::analysisStateStorageSize() const { 
    // Only op core has storage
    return opCore.stateStorageSize();
}

template<typename CoreClass, typename DataMixin> 
size_t SmallSignal<CoreClass, DataMixin>::allocateAnalysisStateStorage(size_t n) { 
    // Only op core has storage
    return opCore.allocateStateStorage(n);
}

template<typename CoreClass, typename DataMixin> 
void SmallSignal<CoreClass, DataMixin>::deallocateAnalysisStateStorage(size_t n) { 
    // Only op core has storage
    opCore.deallocateStateStorage(n);
}

template<typename CoreClass, typename DataMixin> 
bool SmallSignal<CoreClass, DataMixin>::storeState(size_t ndx, bool storeDetails) {
    // Only op core has storage
    return opCore.storeState(ndx, storeDetails);
}

template<typename CoreClass, typename DataMixin> 
bool SmallSignal<CoreClass, DataMixin>::restoreState(size_t ndx) {
    // Only op core has storage
    return opCore.restoreState(ndx);
}

template<typename CoreClass, typename DataMixin> 
void SmallSignal<CoreClass, DataMixin>::makeStateIncoherent(size_t ndx) {
    opCore.makeStateIncoherent(ndx);
}

template<typename CoreClass, typename DataMixin> 
void SmallSignal<CoreClass, DataMixin>::dump(std::ostream& os) const {
}

}

#endif
