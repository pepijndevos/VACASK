#ifndef __ANPSSSMSIG_DEFINED
#define __ANPSSSMSIG_DEFINED

#include <memory>
#include "an.h"
#include "corepss.h"
#include "parameterized.h"
#include "output.h"
#include "solver.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

// PSS analog of HBSmallSignal (anhbsmsig.h): owns the PSS cores (opCore_, stabilTran_,
// pssTran_, pssCore_, same as PSS) plus a CoreClass smsigCore (PACCore, ...).
// Same DataMixin/explicit-specialization mechanism.
template<typename CoreClass, typename DataMixin> class PssSmallSignal : public Analysis, public DataMixin {
public:
    typedef CoreClass::Parameters Parameters;

    // Will need to specialize the constructor
    PssSmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis) {};

    PssSmallSignal           (const PssSmallSignal&)  = delete;
    PssSmallSignal           (      PssSmallSignal&&) = delete;
    PssSmallSignal& operator=(const PssSmallSignal&)  = delete;
    PssSmallSignal& operator=(      PssSmallSignal&&) = delete;

    Parameterized& parameters() override { return params; };
    const Parameterized& parameters() const override { return params; };

    // Factory function
    static Analysis* create(PTAnalysis& ptAnalysis, Circuit& circuit, Status& s=Status::ignore) {
        auto* an = new PssSmallSignal<CoreClass, DataMixin>(ptAnalysis.name(), circuit, ptAnalysis);
        return an;
    };

    void dump(std::ostream& os) const override;

protected:
    // Add output descriptors common to all cores, no error message is returned
    bool addCommonOutputDescriptor(const OutputDescriptor& desc) override;

    // Add core-specific output descriptors, no error message is returned
    bool addCoreOutputDescriptors(ErrorConsumer& errors) override;

    // Add PSS output descriptor(s) based on save, generates error message if verification is required
    // Returns ok, resolved
    std::tuple<bool, bool> resolvePssSave(const PTSave& save, bool verify, ErrorConsumer& errors);

    // Add small-signal output descriptor(s) based on save, needs to be specialized
    bool resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) override { return true; };

    // Add default output descriptors if no save is specified
    // No error message is returned
    bool addDefaultOutputDescriptors(ErrorConsumer& errors) override;

    // Remove all output descriptors from all cores
    // No error message is returned
    void clearOutputDescriptors() override;

    // Resolve output descriptors into output sources across all cores
    bool resolveOutputDescriptors(bool strict, ErrorConsumer& errors) override;

    // Check if we need to add analysis-specific matrix entries or states
    std::tuple<bool, bool> preMapping(ErrorConsumer& errors) override;

    // Add analysis-specific matrix entries and states
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
    // Only opCore_ (one slot for the PSS solution) and pssCore_ have state storage
    size_t analysisStateStorageSize() const override;
    size_t allocateAnalysisStateStorage(size_t n) override;
    virtual void deallocateAnalysisStateStorage(size_t n=0);
    bool storeState(size_t ndx, bool storeDetails=true) override;
    bool restoreState(size_t ndx) override;
    void makeStateIncoherent(size_t ndx) override;

    IStruct<Parameters> params;

    // Declared before the cores so the references they bind in their init lists
    // refer to fully-constructed members. The cores are declared in the order
    // in which they bind references to each other. The DataMixin base members
    // (jacSpec, smsigMatrix, ...) are constructed before these and are safe.
    CSCRealMatrix jac_;
    VectorRepository<double> opSolution_;
    VectorRepository<double> solution_;
    VectorRepository<double> states_;

    // Dummies to make opCore happy
    DelayLines delayLines_;
    DelayMatrixBindings<double*> delayBindings_;

    std::unique_ptr<RealSparseSolver> linearSolver_;
    std::unique_ptr<ComplexSparseSolver> linearCxSolver_;

    OperatingPointCore opCore_;
    TranCore           stabilTran_;
    PssTranCore        pssTran_;
    PssCore            pssCore_;
    CoreClass smsigCore;
};

template<typename CoreClass, typename DataMixin>
bool PssSmallSignal<CoreClass, DataMixin>::addCommonOutputDescriptor(const OutputDescriptor& desc) {
    // opCore_ and pssCore_ output nothing
    // False is returned if the descriptor is already there
    bool s1 = stabilTran_.addOutputDescriptor(desc);
    bool s2 = pssTran_.addOutputDescriptor(desc);
    bool s3 = smsigCore.addOutputDescriptor(desc);
    return s1 && s2 && s3;
}

template<typename CoreClass, typename DataMixin>
bool PssSmallSignal<CoreClass, DataMixin>::addCoreOutputDescriptors(ErrorConsumer& errors) {
    // shootParams.write is manipulated during shooting, so set it here so that descriptors are populated
    params.core().pssParams.shootParams.write = params.core().pssParams.write;
    if (!stabilTran_.addCoreOutputDescriptors(errors)) {
        return false;
    }
    if (!pssTran_.addCoreOutputDescriptors(errors)) {
        return false;
    }
    if (!smsigCore.addCoreOutputDescriptors(errors)) {
        return false;
    }
    return true;
}

template<typename CoreClass, typename DataMixin>
bool PssSmallSignal<CoreClass, DataMixin>::addDefaultOutputDescriptors(ErrorConsumer& errors) {
    params.core().pssParams.shootParams.write = params.core().pssParams.write;
    // Must be invoked on all cores regardless of return value
    auto s1 = stabilTran_.addDefaultOutputDescriptors(errors);
    auto s2 = pssTran_.addDefaultOutputDescriptors(errors);
    auto s3 = smsigCore.addDefaultOutputDescriptors(errors);
    return s1 && s2 && s3;
}

template<typename CoreClass, typename DataMixin>
void PssSmallSignal<CoreClass, DataMixin>::clearOutputDescriptors() {
    // Suppress operating-point output
    params.core().pssParams.opParams.write = 0;
    // Must be invoked on all cores regardless of return value
    opCore_.clearOutputDescriptors();
    stabilTran_.clearOutputDescriptors();
    pssTran_.clearOutputDescriptors();
    pssCore_.clearOutputDescriptors();
    smsigCore.clearOutputDescriptors();
}

// Resolve output descriptors to output sources across all cores
template<typename CoreClass, typename DataMixin>
bool PssSmallSignal<CoreClass, DataMixin>::resolveOutputDescriptors(bool strict, ErrorConsumer& errors) {
    // Any error causes immediate exit if strict is true
    // Before exit an error message is formatted and status is set
    if (!stabilTran_.resolveOutputDescriptors(strict, errors)) {
        if (strict) {
            return false;
        }
    }
    if (!pssTran_.resolveOutputDescriptors(strict, errors)) {
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
std::tuple<bool, bool> PssSmallSignal<CoreClass, DataMixin>::resolvePssSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
    // PSS saves
    static const auto idPssDefault = Id("pssdefault");
    static const auto idPssFull = Id("pssfull");
    static const auto idV = Id("v");
    static const auto idI = Id("i");
    static const auto idP = Id("p");

    // When verification is not required, save-resolution errors are not fatal
    // and must not reach the caller's error consumer.
    ErrorConsumer sink;
    ErrorConsumer& e1 = verify ? errors : sink;

    bool st = true;
    if (save.typeName() == idPssDefault) {
        st = stabilTran_.addAllUnknowns(save, e1);
        st = st && pssTran_.addAllUnknowns(save, e1);
    } else if (save.typeName() == idPssFull) {
        st = stabilTran_.addAllNodes(save, e1);
        st = st && pssTran_.addAllNodes(save, e1);
    } else if (save.typeName() == idV) {
        st = stabilTran_.addNode(save, e1);
        st = st && pssTran_.addNode(save, e1);
    } else if (save.typeName() == idI) {
        st = stabilTran_.addFlow(save, e1);
        st = st && pssTran_.addFlow(save, e1);
    } else if (save.typeName() == idP) {
        st = stabilTran_.addInstanceOutvar(save, e1);
        st = st && pssTran_.addInstanceOutvar(save, e1);
    } else {
        // Do not know how to handle this save
        if (verify) {
            errors.push(AnUnsupportedSaveDirective{save.location()});
        }
        // Return false, false (error, not handled)
        return std::make_tuple(false, false);
    }
    // Error detected in PSS save, verification required
    if (verify && !st) {
        errors.push(AnSaveDirectiveLocation{save.location()});
    }
    // Status, handled
    return std::make_tuple(st, true);
}

template<typename CoreClass, typename DataMixin>
std::tuple<bool, bool> PssSmallSignal<CoreClass, DataMixin>::preMapping(ErrorConsumer& errors) {
    auto [ok1, needsMappingOp] = opCore_.preMapping(errors);
    auto [ok2, needsMappingStabil] = stabilTran_.preMapping(errors);
    auto [ok3, needsMappingPss] = pssTran_.preMapping(errors);
    auto [ok4, needsMappingSmsig] = smsigCore.preMapping(errors);
    return std::make_tuple(
        ok1 && ok2 && ok3 && ok4,
        needsMappingOp || needsMappingStabil || needsMappingPss || needsMappingSmsig
    );
}

template<typename CoreClass, typename DataMixin>
bool PssSmallSignal<CoreClass, DataMixin>::populateStructures(ErrorConsumer& errors) {
    if (!opCore_.populateStructures(errors)) {
        return false;
    }
    if (!stabilTran_.populateStructures(errors)) {
        return false;
    }
    if (!pssCore_.populateStructures(errors)) {
        return false;
    }
    return smsigCore.populateStructures(errors);
}

template<typename CoreClass, typename DataMixin>
bool PssSmallSignal<CoreClass, DataMixin>::rebuildCores(ErrorConsumer& errors) {
    // Create Jacobian - it is common to PSS, Op and tran core, so we need to rebuild it here
    if (!jac_.rebuild(circuit.sparsityMap(), circuit.unknownCount(), errors)) {
        return false;
    }

    // Create and rebuild the linear solver shared by the NR solvers
    auto& options = circuit.simulatorOptions().core();
    auto solverId = params.core().pssParams.opParams.solver;
    solverId = solverId?solverId:options.tdsolver;
    solverId = solverId?solverId:Simulator::defaultTdSolverId;
    linearSolver_ = std::unique_ptr<RealSparseSolver>(RealSparseSolver::createSolver(solverId, jac_, errors));
    if (!linearSolver_ || !linearSolver_->rebuild(errors)) {
        return false;
    }
    opCore_.setLinearSolver(linearSolver_.get());
    stabilTran_.setLinearSolver(linearSolver_.get());
    pssTran_.setLinearSolver(linearSolver_.get());

    // First rebuild the pssTran core because otherwise it will clear ic forces in
    // opCore_. pssTran has not ic parameter!
    if (!pssTran_.rebuild(errors)) {
        return false;
    }
    // This one sets up ic forces in opCore_
    if (!stabilTran_.rebuild(errors)) {
        return false;
    }
    // Rebuild opCore, now it has forces set up
    if (!opCore_.rebuild(errors)) {
        return false;
    }
    // Rebuild PSS core
    if (!pssCore_.rebuild(errors)) {
        return false;
    }

    if (!smsigCore.rebuild(errors)) {
        return false;
    }

    // Small-signal conversion-matrix solver (smsigMatrix pattern built by smsigCore.rebuild())
    auto cxSolverId = params.core().solver;
    cxSolverId = cxSolverId?cxSolverId:options.qpsmsigsolver;
    cxSolverId = cxSolverId?cxSolverId:Simulator::defaultQpsmsigSolverId;
    linearCxSolver_ = std::unique_ptr<ComplexSparseSolver>(
        ComplexSparseSolver::createSolver(cxSolverId, this->smsigMatrix, errors));
    if (!linearCxSolver_) {
        return false;
    }
    // smsigMatrix has nf x nf dense blocks; hint the solver before it builds
    linearCxSolver_->setBlockSize(this->smsigMatrix.nBlockElementCols());
    if (!linearCxSolver_->rebuild(errors)) {
        return false;
    }
    smsigCore.setLinearSolver(linearCxSolver_.get());

    return true;
}

template<typename CoreClass, typename DataMixin>
bool PssSmallSignal<CoreClass, DataMixin>::initializeOutputs(ErrorConsumer& errors) {
    // opCore_ and pssCore_ output nothing
    // Any error exits immediately
    if (!stabilTran_.initializeOutputs(prefixedName_+".pss.tran", errors)) {
        return false;
    }
    if (!pssTran_.initializeOutputs(prefixedName_+".pss", errors)) {
        return false;
    }
    if (!smsigCore.initializeOutputs(prefixedName_, errors)) {
        return false;
    }
    return true;
}

template<typename CoreClass, typename DataMixin>
bool PssSmallSignal<CoreClass, DataMixin>::finalizeOutputs(ErrorConsumer& errors) {
    // Finalization has to be performed on all cores, regardless of errors
    auto ok1 = stabilTran_.finalizeOutputs(errors);
    auto ok2 = pssTran_.finalizeOutputs(errors);
    auto ok3 = smsigCore.finalizeOutputs(errors);
    return ok1 && ok2 && ok3;
}

template<typename CoreClass, typename DataMixin>
bool PssSmallSignal<CoreClass, DataMixin>::deleteOutputs(ErrorConsumer& errors) {
    params.core().pssParams.shootParams.write = params.core().pssParams.write;
    // Output needs to be deleted for all cores
    auto ok1 = stabilTran_.deleteOutputs(prefixedName_+".pss.tran", errors);
    auto ok2 = pssTran_.deleteOutputs(prefixedName_+".pss", errors);
    auto ok3 = smsigCore.deleteOutputs(prefixedName_, errors);
    return ok1 && ok2 && ok3;
}

template<typename CoreClass, typename DataMixin>
size_t PssSmallSignal<CoreClass, DataMixin>::analysisStateStorageSize() const {
    // Only pssCore_ has storage
    return pssCore_.stateStorageSize();
}

template<typename CoreClass, typename DataMixin>
size_t PssSmallSignal<CoreClass, DataMixin>::allocateAnalysisStateStorage(size_t n) {
    // opCore_ needs 1 storage slot for the PSS solution
    opCore_.allocateStateStorage(1);
    return pssCore_.allocateStateStorage(n);
}

template<typename CoreClass, typename DataMixin>
void PssSmallSignal<CoreClass, DataMixin>::deallocateAnalysisStateStorage(size_t n) {
    opCore_.deallocateStateStorage(1);
    pssCore_.deallocateStateStorage(n);
}

template<typename CoreClass, typename DataMixin>
bool PssSmallSignal<CoreClass, DataMixin>::storeState(size_t ndx, bool storeDetails) {
    // Only pssCore_ has storage
    return pssCore_.storeState(ndx, storeDetails);
}

template<typename CoreClass, typename DataMixin>
bool PssSmallSignal<CoreClass, DataMixin>::restoreState(size_t ndx) {
    // Only pssCore_ has storage
    return pssCore_.restoreState(ndx);
}

template<typename CoreClass, typename DataMixin>
void PssSmallSignal<CoreClass, DataMixin>::makeStateIncoherent(size_t ndx) {
    pssCore_.makeStateIncoherent(ndx);
}

template<typename CoreClass, typename DataMixin>
void PssSmallSignal<CoreClass, DataMixin>::dump(std::ostream& os) const {
}

}

#endif
