// anpss.cpp
//
// Pss analysis — wires PssCore into the simulator's analysis dispatch layer.
//
// Follows the same structure as antran.cpp. Pss owns the shared KluRealMatrix,
// solution, and states repositories and passes them by reference to PssCore.
// All output descriptor management routes through PssCore's proxy methods,
// which forward to the internal PssTranCore that actually writes the output.

#include "anpss.h"
#include "common.h"


namespace NAMESPACE {

Pss::Pss(Id name, Circuit& circuit, PTAnalysis& ptAnalysis)
    : Analysis(name, circuit, ptAnalysis),
      opCore_(*this, params.core().opParams, circuit, commons, jac_, opSolution_, states_, delayLines_, delayBindings_),
      stabilTran_(*this, params.core().stabilParams, opCore_, circuit, commons, jac_, opSolution_, solution_, states_, delayLines_, delayBindings_),
      pssTran_(*this, params.core().shootParams, opCore_, circuit, commons, jac_, opSolution_, solution_, states_, delayLines_, delayBindings_),
      pssCore_(*this, params.core(), circuit, commons, jac_, solution_, states_, opCore_, stabilTran_, pssTran_) {
}

Analysis* Pss::create(PTAnalysis& ptAnalysis, Circuit& circuit, Status& s) {
    return new Pss(ptAnalysis.name(), circuit, ptAnalysis);
}

void Pss::clearOutputDescriptors() {
    // Suppress operating-point output; the PSS trajectory is what is saved.
    params.core().opParams.write = 0;
    opCore_.clearOutputDescriptors();
    stabilTran_.clearOutputDescriptors();
    pssTran_.clearOutputDescriptors();
    pssCore_.clearOutputDescriptors();
}

bool Pss::addCommonOutputDescriptor(const OutputDescriptor& desc) {
    // Common output descriptor (i.e. sweep variables)
    // opCore_ and pssCore_ output nothing
    bool b2 = stabilTran_.addOutputDescriptor(desc);
    bool b3 = pssTran_.addOutputDescriptor(desc);
    return b2 && b3;
}

// shootParams.write is not exposed directly because it is manipulated during shooting. 
// Therefore it must be set to write so that descriptors are populated. 

bool Pss::addCoreOutputDescriptors(ErrorConsumer& errors) {
    // Output descriptors that are specific to a core (i.e. time, frequency)
    // opCore_ and pssCore_ output nothing
    params.core().shootParams.write = params.core().write;
    if (!stabilTran_.addCoreOutputDescriptors(errors)) {
        return false;
    }
    if (!pssTran_.addCoreOutputDescriptors(errors)) {
        return false;
    }
    return true;
}

bool Pss::resolveOutputDescriptors(bool strict, ErrorConsumer& errors) {
    // Resolve output descriptors into output sources
    // opCore_ and pssCore_ output nothing
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
    return true;
}

bool Pss::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
    // Dispatch saves to cores - create output descriptors
    // opCore_ and pssCore_ output nothing
    static const auto idDefault = Id("default");
    static const auto idFull    = Id("full");
    static const auto idV       = Id("v");
    static const auto idI       = Id("i");
    static const auto idP       = Id("p");

    // When verification is not required, save-resolution errors are not fatal.
    ErrorConsumer sink;
    ErrorConsumer& e1 = verify ? errors : sink;

    bool st = true;
    if (save.typeName() == idDefault) {
        st = stabilTran_.addAllUnknowns(save, e1);
        st = st && pssTran_.addAllUnknowns(save, e1);
    } else if (save.typeName() == idFull) {
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
        if (verify) {
            errors.push(AnUnsupportedSaveDirective{save.location()});
            return false;
        }
        return true;
    }

    if (verify && !st) {
        errors.push(AnSaveDirectiveLocation{save.location()});
        return false;
    }
    return true;
}

bool Pss::addDefaultOutputDescriptors(ErrorConsumer& errors) {
    // opCore_ and pssCore_ output nothing
    params.core().shootParams.write = params.core().write;
    auto s1 = stabilTran_.addDefaultOutputDescriptors(errors);
    auto s2 = pssTran_.addDefaultOutputDescriptors(errors);
    return s1 && s2;
}

bool Pss::initializeOutputs(ErrorConsumer& errors) {
    // opCore_ and pssCore_ output nothing
    if (!stabilTran_.initializeOutputs(std::string(name_)+".tran", errors)) {
        return false;
    }
    if (!pssTran_.initializeOutputs(name_, errors)) {
        return false;
    }
    return true;
}

bool Pss::finalizeOutputs(ErrorConsumer& errors) {
    // opCore_ and pssCore_ output nothing
    auto ok1 = stabilTran_.finalizeOutputs(errors);
    auto ok2 = pssTran_.finalizeOutputs(errors);
    return ok1 && ok2;
}

bool Pss::deleteOutputs(ErrorConsumer& errors) {
    params.core().shootParams.write = params.core().write;
    auto ok1 = stabilTran_.deleteOutputs(std::string(name_)+".tran", errors);
    auto ok2 = pssTran_.deleteOutputs(name_, errors);
    return ok1 && ok2;
}

bool Pss::rebuildCores(ErrorConsumer& errors) {
    // Create Jacobian - it is common to PSS, Op and tran core, so we need to rebuild it here
    if (!jac_.rebuild(circuit.sparsityMap(), circuit.unknownCount(), errors)) {
        return false;
    }

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
    // In the end rebuild top level core
    if (!pssCore_.rebuild(errors)) {
        return false;
    }
    return true;
}

size_t Pss::analysisStateStorageSize() const { 
    // Only op core has storage
    return pssCore_.stateStorageSize();
}

size_t Pss::allocateAnalysisStateStorage(size_t n) { 
    // Op core need 1 storage slot for pss solution
    auto ok1 = opCore_.allocateStateStorage(1);
    // Only pss core has storage
    auto ok2 = pssCore_.allocateStateStorage(n);
    return ok1 && ok2;
}

void Pss::deallocateAnalysisStateStorage(size_t n) { 
    opCore_.deallocateStateStorage(1);
    // Only op core has storage
    pssCore_.deallocateStateStorage(n);
}

bool Pss::storeState(size_t ndx, bool storeDetails) {
    // Only op core has storage
    return pssCore_.storeState(ndx, storeDetails);
}

bool Pss::restoreState(size_t ndx) {
    // Only op core has storage
    return pssCore_.restoreState(ndx);
}

void Pss::makeStateIncoherent(size_t ndx) {
    pssCore_.makeStateIncoherent(ndx);
}


std::tuple<bool, bool> Pss::preMapping(ErrorConsumer& errors) {
    // Forward IC and icmode to stabilParams so stabilTran_.preMapping()
    // sees them and populates preprocessedIc before rebuild() is called.
    // auto& p = params.core();
    // bool hasIc = (p.ic.type() == Value::Type::ValueVec);
    // if (hasIc) {
    //     p.stabilParams.ic     = p.ic;
    //     p.stabilParams.icmode = TranCore::icmodeUic;
    // }
    auto [ok1, needsMappingOp] = opCore_.preMapping(errors);
    auto [ok2, needsMappingStabil] = stabilTran_.preMapping(errors);
    auto [ok3, needsMappingPss] = pssTran_.preMapping(errors);
    return std::make_tuple(ok1 && ok2 && ok3, needsMappingOp || needsMappingStabil || needsMappingPss);
}

bool Pss::populateStructures(ErrorConsumer& errors) {
    auto ok1 = opCore_.populateStructures(errors);
    if (!ok1) {
        return false;
    }
    auto ok2 = stabilTran_.populateStructures(errors);
    if (!ok2) {
        return false;
    }
    return pssCore_.populateStructures(errors);
}

void Pss::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: periodic steady-state (PSS)" << std::endl;
    os << "PSS core:" << std::endl;
    pssCore_.dump(os);
}

} // namespace NAMESPACE
