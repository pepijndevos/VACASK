#ifndef __ANSMSIG_DEFINED
#define __ANSMSIG_DEFINED

#include "an.h"
#include "coreop.h"
#include "parameterized.h"
#include "output.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

template<typename CoreClass, typename DataMixin, bool complexSmsig = true> class SmallSignal : public Analysis, public DataMixin {
public:
    typedef CoreClass::Parameters Parameters;

    // Will need to specialize the constructor
    SmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis) {};
    
    SmallSignal           (const SmallSignal&)  = delete;
    SmallSignal           (      SmallSignal&&) = delete;
    SmallSignal& operator=(const SmallSignal&)  = delete;
    SmallSignal& operator=(      SmallSignal&&) = delete;

    Parameterized& parameters() override { return params; };
    const Parameterized& parameters() const override { return params; };

    // Factory function for operating point analysis
    static Analysis* create(PTAnalysis& ptAnalysis, Circuit& circuit, Status& s=Status::ignore) {
        auto* an = new SmallSignal<CoreClass, DataMixin, complexSmsig>(ptAnalysis.name(), circuit, ptAnalysis);
        return an;
    };

    void dump(std::ostream& os) const override;

protected:
    // Add output descriptors common to all cores, no error message is returned
    bool addCommonOutputDescriptor(const OutputDescriptor& desc) override;

    // Add core-specific output descriptors, no error message is returned
    bool addCoreOutputDescriptors(ErrorConsumer& errors) override;

    // Add operating point output descriptor(s) based on save, generates error message if verification is required
    // Returns ok, resolved
    std::tuple<bool, bool> resolveOpSave(const PTSave& save, bool verify, ErrorConsumer& errors);

    // Add operating point output descriptor(s) based on save, needs to be specialized
    bool resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) override { return true; };

    // Add default output descriptors if no save is specified
    // No error message is returned
    bool addDefaultOutputDescriptors(ErrorConsumer& errors) override;

    // Remove all output descriptors from all cores, transfer parameters from smsig to op
    // No error message is returned
    void clearOutputDescriptors() override;

    // Resolve oputput descriptors into output sources across all cores
    bool resolveOutputDescriptors(bool strict, ErrorConsumer& errors) override;

    // Check if we need to add analysis-specific matrix entries or states
    // By default only op core needs this
    std::tuple<bool, bool> preMapping(ErrorConsumer& errors) override;

    // Add analysis-specific matrix entries and states
    // By default only op core needs this
    bool populateStructures(ErrorConsumer& errors) override;

    // Rebuild cores
    bool rebuildCores(ErrorConsumer& errors) override;

    // Initialize outputs
    bool initializeOutputs(ErrorConsumer& errors) override;

    // Analysis core
    AnalysisCore& analysisCore() override { return smsigCore; };

    // Create core coroutine
    CoreCoroutine coreCoroutine(bool continuePrevious, ErrorConsumer& errors) override {
        return std::move(smsigCore.coroutine(continuePrevious, errors));
    };

    // Finalize outputs
    bool finalizeOutputs(ErrorConsumer& errors) override;

    // Delete outputs
    bool deleteOutputs(ErrorConsumer& errors) override;

    // Analysis state storage for continuation in sweeps
    // Only op core has state storage
    size_t analysisStateStorageSize() const override;
    size_t allocateAnalysisStateStorage(size_t n) override;
    virtual void deallocateAnalysisStateStorage(size_t n=0);
    bool storeState(size_t ndx, bool storeDetails=true) override;
    bool restoreState(size_t ndx) override;
    void makeStateIncoherent(size_t ndx) override;

    IStruct<Parameters> params;

    // Declared before the cores so the references they bind in their init lists
    // (jac, solution, states) refer to fully-constructed members. opCore precedes
    // smsigCore because smsigCore binds a reference to opCore. The DataMixin base
    // members (acMatrix, acSolution, ...) are constructed before these and are safe.
    CSCRealMatrix jac; // Resistive Jacobian
    std::unique_ptr<RealSparseSolver> linearSolver_;
    std::unique_ptr<ComplexSparseSolver> linearCxSolver_;
    VectorRepository<double> solution; // Solution history
    VectorRepository<double> states; // Circuit states
    DelayLines delayLines_;
    DelayMatrixBindings<double*> opDelayBindings_;
    DelayMatrixBindings<Complex*> smsigDelayBindings_;

    OperatingPointCore opCore;
    CoreClass smsigCore;
};

template<typename CoreClass, typename DataMixin, bool complexSmsig>
bool SmallSignal<CoreClass, DataMixin, complexSmsig>::addCommonOutputDescriptor(const OutputDescriptor& desc) {  
    // False is returned if the descriptor is already there
    bool s1 = opCore.addOutputDescriptor(desc);
    bool s2 = smsigCore.addOutputDescriptor(desc);
    return  s1 && s2;
}

template<typename CoreClass, typename DataMixin, bool complexSmsig>
bool SmallSignal<CoreClass, DataMixin, complexSmsig>::addCoreOutputDescriptors(ErrorConsumer& errors) {
    // False is returned if the descriptor is already there
    if (!opCore.addCoreOutputDescriptors(errors)) {
        return false;
    }
    if (!smsigCore.addCoreOutputDescriptors(errors)) {
        return false;
    }
    return true;
}

template<typename CoreClass, typename DataMixin, bool complexSmsig>
bool SmallSignal<CoreClass, DataMixin, complexSmsig>::addDefaultOutputDescriptors(ErrorConsumer& errors) {
    // Must be invoked on all cores regardless of return value
    auto s1 = opCore.addDefaultOutputDescriptors(errors);
    auto s2 = smsigCore.addDefaultOutputDescriptors(errors);
    return s1 && s2;
}

template<typename CoreClass, typename DataMixin, bool complexSmsig>
void SmallSignal<CoreClass, DataMixin, complexSmsig>::clearOutputDescriptors() {
    // Must be invoked on all cores regardless of return value
    opCore.clearOutputDescriptors();
    smsigCore.clearOutputDescriptors();
}


// Resolve output descriptors to output sources across all cores
template<typename CoreClass, typename DataMixin, bool complexSmsig>
bool SmallSignal<CoreClass, DataMixin, complexSmsig>::resolveOutputDescriptors(bool strict, ErrorConsumer& errors) {
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

template<typename CoreClass, typename DataMixin, bool complexSmsig>
std::tuple<bool, bool> SmallSignal<CoreClass, DataMixin, complexSmsig>::resolveOpSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
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

template<typename CoreClass, typename DataMixin, bool complexSmsig>
std::tuple<bool, bool> SmallSignal<CoreClass, DataMixin, complexSmsig>::preMapping(ErrorConsumer& errors) {
    auto [ok, needsMapping] = opCore.preMapping(errors);
    if (!ok) {
        return std::make_tuple(false, needsMapping);
    }
    auto [ok1, map1] = smsigCore.preMapping(errors);
    return std::make_tuple(ok&&ok1, needsMapping||map1);
}

template<typename CoreClass, typename DataMixin, bool complexSmsig>
bool SmallSignal<CoreClass, DataMixin, complexSmsig>::populateStructures(ErrorConsumer& errors) {
    auto ok = opCore.populateStructures(errors);
    if (!ok) {
        return false;
    }
    return smsigCore.populateStructures(errors);
}

template<typename CoreClass, typename DataMixin, bool complexSmsig>
bool SmallSignal<CoreClass, DataMixin, complexSmsig>::rebuildCores(ErrorConsumer& errors) {
    // Create Jacobian - it is common to both cores, so we need to rebuild it here
    if (!jac.rebuild(circuit.sparsityMap(), circuit.unknownCount(), errors)) {
        return false;
    }

    // Create and rebuild operating point linear solver
    auto& options = circuit.simulatorOptions().core();
    auto solverId = params.core().opParams.solver;
    solverId = solverId?solverId:options.tdsolver;
    solverId = solverId?solverId:Simulator::defaultTdSolverId;
    linearSolver_ = std::unique_ptr<RealSparseSolver>(RealSparseSolver::createSolver(solverId, jac, errors));
    if (!linearSolver_ || !linearSolver_->rebuild(errors)) {
        return false;
    }
    opCore.setLinearSolver(linearSolver_.get());

    // Any error aborts immediately
    if (!opCore.rebuild(errors)) {
        return false;
    }
    if (!smsigCore.rebuild(errors)) {
        return false;
    }

    // Create and rebuild the complex linear solver for the smsig matrix
    // (smsigCore.rebuild() has built the smsig matrix sparsity pattern)
    if constexpr (complexSmsig) {
        auto cxSolverId = params.core().solver;
        cxSolverId = cxSolverId?cxSolverId:options.smsigsolver;
        cxSolverId = cxSolverId?cxSolverId:Simulator::defaultSmsigSolverId;
        linearCxSolver_ = std::unique_ptr<ComplexSparseSolver>(
            ComplexSparseSolver::createSolver(cxSolverId, this->acMatrix, errors));
        if (!linearCxSolver_ || !linearCxSolver_->rebuild(errors)) {
            return false;
        }
        smsigCore.setLinearSolver(linearCxSolver_.get());
    }

    return true;
}

template<typename CoreClass, typename DataMixin, bool complexSmsig>
bool SmallSignal<CoreClass, DataMixin, complexSmsig>::initializeOutputs(ErrorConsumer& errors) {
    // Any error exits immediately
    if (!opCore.initializeOutputs(prefixedName_+".op", errors)) {
        return false;
    }
    if (!smsigCore.initializeOutputs(prefixedName_, errors)) {
        return false;
    }
    return true;
}

template<typename CoreClass, typename DataMixin, bool complexSmsig>
bool SmallSignal<CoreClass, DataMixin, complexSmsig>::finalizeOutputs(ErrorConsumer& errors) {
    // Finalization has to be performed on all cores, regardless of errors
    auto ok1 = opCore.finalizeOutputs(errors);
    auto ok2 = smsigCore.finalizeOutputs(errors);
    return ok1 && ok2;
}

template<typename CoreClass, typename DataMixin, bool complexSmsig>
bool SmallSignal<CoreClass, DataMixin, complexSmsig>::deleteOutputs(ErrorConsumer& errors) {
    // Output needs to be deleted for all cores
    auto ok1 = opCore.deleteOutputs(prefixedName_+".op", errors);
    auto ok2 = smsigCore.deleteOutputs(prefixedName_, errors);
    return ok1 && ok2;
}

template<typename CoreClass, typename DataMixin, bool complexSmsig>
size_t SmallSignal<CoreClass, DataMixin, complexSmsig>::analysisStateStorageSize() const { 
    // Only op core has storage
    return opCore.stateStorageSize();
}

template<typename CoreClass, typename DataMixin, bool complexSmsig>
size_t SmallSignal<CoreClass, DataMixin, complexSmsig>::allocateAnalysisStateStorage(size_t n) { 
    // Only op core has storage
    return opCore.allocateStateStorage(n);
}

template<typename CoreClass, typename DataMixin, bool complexSmsig>
void SmallSignal<CoreClass, DataMixin, complexSmsig>::deallocateAnalysisStateStorage(size_t n) { 
    // Only op core has storage
    opCore.deallocateStateStorage(n);
}

template<typename CoreClass, typename DataMixin, bool complexSmsig>
bool SmallSignal<CoreClass, DataMixin, complexSmsig>::storeState(size_t ndx, bool storeDetails) {
    // Only op core has storage
    return opCore.storeState(ndx, storeDetails);
}

template<typename CoreClass, typename DataMixin, bool complexSmsig>
bool SmallSignal<CoreClass, DataMixin, complexSmsig>::restoreState(size_t ndx) {
    // Only op core has storage
    return opCore.restoreState(ndx);
}

template<typename CoreClass, typename DataMixin, bool complexSmsig>
void SmallSignal<CoreClass, DataMixin, complexSmsig>::makeStateIncoherent(size_t ndx) {
    opCore.makeStateIncoherent(ndx);
}

template<typename CoreClass, typename DataMixin, bool complexSmsig>
void SmallSignal<CoreClass, DataMixin, complexSmsig>::dump(std::ostream& os) const {
}

}

#endif
