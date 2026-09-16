#ifndef __ANHBSMSIG_DEFINED
#define __ANHBSMSIG_DEFINED

#include "an.h"
#include "corehb.h"
#include "parameterized.h"
#include "output.h"
#include "solver.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

// HB analog of SmallSignal (ansmsig.h): owns an HBCore plus a CoreClass smsigCore
// (HBACCore, HBNoiseCore, ...). Same DataMixin/explicit-specialization mechanism.
template<typename CoreClass, typename DataMixin> class HBSmallSignal : public Analysis, public DataMixin {
public:
    typedef CoreClass::Parameters Parameters;

    // Will need to specialize the constructor
    HBSmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis) {};

    HBSmallSignal           (const HBSmallSignal&)  = delete;
    HBSmallSignal           (      HBSmallSignal&&) = delete;
    HBSmallSignal& operator=(const HBSmallSignal&)  = delete;
    HBSmallSignal& operator=(      HBSmallSignal&&) = delete;

    virtual Parameterized& parameters() { return params; };
    virtual const Parameterized& parameters() const { return params; };

    // Factory function
    static Analysis* create(PTAnalysis& ptAnalysis, Circuit& circuit, Status& s=Status::ignore) {
        auto* an = new HBSmallSignal<CoreClass, DataMixin>(ptAnalysis.name(), circuit, ptAnalysis);
        return an;
    };

    virtual void dump(std::ostream& os) const;

protected:
    // Add output descriptors common to all cores, no error message is returned
    virtual bool addCommonOutputDescriptor(const OutputDescriptor& desc);

    // Add core-specific output descriptors, no error message is returned
    virtual bool addCoreOutputDescriptors(ErrorConsumer& errors);

    // Add HB operating point output descriptor(s) based on save, generates error message if verification is required
    // Returns ok, resolved
    std::tuple<bool, bool> resolveHbSave(const PTSave& save, bool verify, ErrorConsumer& errors);

    // Add HB operating point output descriptor(s) based on save, needs to be specialized
    virtual bool resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) { return true; };

    // Add default output descriptors if no save is specified
    // No error message is returned
    virtual bool addDefaultOutputDescriptors(ErrorConsumer& errors);

    // Remove all output descriptors from all cores
    // No error message is returned
    virtual void clearOutputDescriptors();

    // Resolve output descriptors into output sources across all cores
    virtual bool resolveOutputDescriptors(bool strict, ErrorConsumer& errors);

    // Check if we need to add analysis-specific matrix entries or states
    // By default only hbCore needs this
    virtual std::tuple<bool, bool> preMapping(ErrorConsumer& errors);

    // Add analysis-specific matrix entries and states
    // By default only hbCore needs this
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
    // Only hbCore has state storage
    virtual size_t analysisStateStorageSize() const;
    virtual size_t allocateAnalysisStateStorage(size_t n);
    virtual void deallocateAnalysisStateStorage(size_t n=0);
    virtual bool storeState(size_t ndx, bool storeDetails=true);
    virtual bool restoreState(size_t ndx);
    virtual void makeStateIncoherent(size_t ndx);

    IStruct<Parameters> params;

    // Declared before the cores so the references they bind in their init lists
    // (jacColoc/jac/solution; smsigCore's own DataMixin fields) refer to
    // fully-constructed members. hbCore precedes smsigCore because smsigCore
    // binds a reference to hbCore. The DataMixin base members (jacSpec,
    // hbSolution, acMatrix, ...) are constructed before these and are safe.
    CSCBlockSparseRealMatrix jacColoc;
    CSCBlockSparseRealMatrix jac;
    VectorRepository<double> solution;
    DelayLines delayLines_;
    DelayMatrixBindings<DenseMatrixView<double>> hbDelayBindings_;
    DelayMatrixBindings<DenseMatrixView<Complex>> smsigDelayBindings_;

    std::unique_ptr<RealSparseSolver> linearSolver_;
    std::unique_ptr<ComplexSparseSolver> linearCxSolver_;

    HBCore hbCore;
    CoreClass smsigCore;
};

template<typename CoreClass, typename DataMixin>
bool HBSmallSignal<CoreClass, DataMixin>::addCommonOutputDescriptor(const OutputDescriptor& desc) {
    // False is returned if the descriptor is already there
    bool s1 = hbCore.addOutputDescriptor(desc);
    bool s2 = smsigCore.addOutputDescriptor(desc);
    return s1 && s2;
}

template<typename CoreClass, typename DataMixin>
bool HBSmallSignal<CoreClass, DataMixin>::addCoreOutputDescriptors(ErrorConsumer& errors) {
    // False is returned if the descriptor is already there
    if (!hbCore.addCoreOutputDescriptors(errors)) {
        return false;
    }
    if (!smsigCore.addCoreOutputDescriptors(errors)) {
        return false;
    }
    return true;
}

template<typename CoreClass, typename DataMixin>
bool HBSmallSignal<CoreClass, DataMixin>::addDefaultOutputDescriptors(ErrorConsumer& errors) {
    // Must be invoked on all cores regardless of return value
    auto s1 = hbCore.addDefaultOutputDescriptors(errors);
    auto s2 = smsigCore.addDefaultOutputDescriptors(errors);
    return s1 && s2;
}

template<typename CoreClass, typename DataMixin>
void HBSmallSignal<CoreClass, DataMixin>::clearOutputDescriptors() {
    // Must be invoked on all cores regardless of return value
    hbCore.clearOutputDescriptors();
    smsigCore.clearOutputDescriptors();
}

// Resolve output descriptors to output sources across all cores
template<typename CoreClass, typename DataMixin>
bool HBSmallSignal<CoreClass, DataMixin>::resolveOutputDescriptors(bool strict, ErrorConsumer& errors) {
    // Any error causes immediate exit if strict is true
    // Before exit an error message is formatted and status is set
    if (!hbCore.resolveOutputDescriptors(strict, errors)) {
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
std::tuple<bool, bool> HBSmallSignal<CoreClass, DataMixin>::resolveHbSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
    // HB saves
    static const auto idHbDefault = Id("hbdefault");
    static const auto idHbFull = Id("hbfull");
    static const auto idV = Id("v");
    static const auto idI = Id("i");
    static const auto idP = Id("p");

    // When verification is not required, save-resolution errors are not fatal
    // and must not reach the caller's error consumer.
    ErrorConsumer sink;
    ErrorConsumer& e1 = verify ? errors : sink;

    bool st = true;
    if (save.typeName() == idHbDefault) {
        st = hbCore.addAllUnknowns(save, e1);
    } else if (save.typeName() == idHbFull) {
        st = hbCore.addAllNodes(save, e1);
    } else if (save.typeName() == idV) {
        st = hbCore.addNode(save, e1);
    } else if (save.typeName() == idI) {
        st = hbCore.addFlow(save, e1);
    } else {
        // Do not know how to handle this save
        if (verify) {
            errors.push(AnUnsupportedSaveDirective{save.location()});
        }
        // Return false, false (error, not handled)
        return std::make_tuple(false, false);
    }
    // Error detected in hbCore save, verification required
    if (verify && !st) {
        errors.push(AnSaveDirectiveLocation{save.location()});
    }
    // Status, handled
    return std::make_tuple(st, true);
}

template<typename CoreClass, typename DataMixin>
std::tuple<bool, bool> HBSmallSignal<CoreClass, DataMixin>::preMapping(ErrorConsumer& errors) {
    auto [ok, needsMapping] = hbCore.preMapping(errors);
    if (!ok) {
        return std::make_tuple(false, needsMapping);
    }
    auto [ok1, map1] = smsigCore.preMapping(errors);
    return std::make_tuple(ok&&ok1, needsMapping||map1);
}

template<typename CoreClass, typename DataMixin>
bool HBSmallSignal<CoreClass, DataMixin>::populateStructures(ErrorConsumer& errors) {
    auto ok = hbCore.populateStructures(errors);
    if (!ok) {
        return false;
    }
    return smsigCore.populateStructures(errors);
}

template<typename CoreClass, typename DataMixin>
bool HBSmallSignal<CoreClass, DataMixin>::rebuildCores(ErrorConsumer& errors) {
    // Any error aborts immediately
    if (!hbCore.rebuild(errors)) {
        return false;
    }

    auto& options = circuit.simulatorOptions().core();

    // Large-signal HB linear solver, built on jac by hbCore.rebuild().
    // Only needed (and only built) if the HB operating point is actually solved here
    // rather than reused/imported from elsewhere.
    if (params.core().hbParams.solve) {
        auto solverId = params.core().hbParams.solver;
        solverId = solverId?solverId:options.hbsolver;
        solverId = solverId?solverId:Simulator::defaultHbSolverId;
        linearSolver_ = std::unique_ptr<RealSparseSolver>(RealSparseSolver::createSolver(solverId, jac, errors));
        if (!linearSolver_) {
            return false;
        }
        // jac has nt x nt dense blocks; hint the solver before it builds
        linearSolver_->setBlockSize(jac.nBlockElementCols());
        if (!linearSolver_->rebuild(errors)) {
            return false;
        }
        hbCore.setLinearSolver(linearSolver_.get());
    }

    if (!smsigCore.rebuild(errors)) {
        return false;
    }

    // Small-signal conversion-matrix solver (acMatrix pattern built by smsigCore.rebuild())
    auto cxSolverId = params.core().solver;
    cxSolverId = cxSolverId?cxSolverId:options.qpsmsigsolver;
    cxSolverId = cxSolverId?cxSolverId:Simulator::defaultQpsmsigSolverId;
    linearCxSolver_ = std::unique_ptr<ComplexSparseSolver>(
        ComplexSparseSolver::createSolver(cxSolverId, this->acMatrix, errors));
    if (!linearCxSolver_) {
        return false;
    }
    // acMatrix has nf x nf dense blocks; hint the solver before it builds
    linearCxSolver_->setBlockSize(this->acMatrix.nBlockElementCols());
    if (!linearCxSolver_->rebuild(errors)) {
        return false;
    }
    smsigCore.setLinearSolver(linearCxSolver_.get());

    return true;
}

template<typename CoreClass, typename DataMixin>
bool HBSmallSignal<CoreClass, DataMixin>::initializeOutputs(ErrorConsumer& errors) {
    // Any error exits immediately
    if (!hbCore.initializeOutputs(prefixedName_+".hb", errors)) {
        return false;
    }
    if (!smsigCore.initializeOutputs(prefixedName_, errors)) {
        return false;
    }
    return true;
}

template<typename CoreClass, typename DataMixin>
bool HBSmallSignal<CoreClass, DataMixin>::finalizeOutputs(ErrorConsumer& errors) {
    // Finalization has to be performed on all cores, regardless of errors
    auto ok1 = hbCore.finalizeOutputs(errors);
    auto ok2 = smsigCore.finalizeOutputs(errors);
    return ok1 && ok2;
}

template<typename CoreClass, typename DataMixin>
bool HBSmallSignal<CoreClass, DataMixin>::deleteOutputs(ErrorConsumer& errors) {
    // Output needs to be deleted for all cores
    auto ok1 = hbCore.deleteOutputs(prefixedName_+".hb", errors);
    auto ok2 = smsigCore.deleteOutputs(prefixedName_, errors);
    return ok1 && ok2;
}

template<typename CoreClass, typename DataMixin>
size_t HBSmallSignal<CoreClass, DataMixin>::analysisStateStorageSize() const {
    // Only hbCore has storage
    return hbCore.stateStorageSize();
}

template<typename CoreClass, typename DataMixin>
size_t HBSmallSignal<CoreClass, DataMixin>::allocateAnalysisStateStorage(size_t n) {
    // Only hbCore has storage
    return hbCore.allocateStateStorage(n);
}

template<typename CoreClass, typename DataMixin>
void HBSmallSignal<CoreClass, DataMixin>::deallocateAnalysisStateStorage(size_t n) {
    // Only hbCore has storage
    hbCore.deallocateStateStorage(n);
}

template<typename CoreClass, typename DataMixin>
bool HBSmallSignal<CoreClass, DataMixin>::storeState(size_t ndx, bool storeDetails) {
    // Only hbCore has storage
    return hbCore.storeState(ndx, storeDetails);
}

template<typename CoreClass, typename DataMixin>
bool HBSmallSignal<CoreClass, DataMixin>::restoreState(size_t ndx) {
    // Only hbCore has storage
    return hbCore.restoreState(ndx);
}

template<typename CoreClass, typename DataMixin>
void HBSmallSignal<CoreClass, DataMixin>::makeStateIncoherent(size_t ndx) {
    hbCore.makeStateIncoherent(ndx);
}

template<typename CoreClass, typename DataMixin>
void HBSmallSignal<CoreClass, DataMixin>::dump(std::ostream& os) const {
}

}

#endif
