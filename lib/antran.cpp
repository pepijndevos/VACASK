#include "antran.h"
#include "common.h"


namespace NAMESPACE {

Tran::Tran(Id name, Circuit& circuit, PTAnalysis& ptAnalysis) 
    : Analysis(name, circuit, ptAnalysis),
      opCore(*this, params.core().opParams, circuit, commons, jac, opSolution, states, delayLines_, delayBindings_),
      tranCore(*this, params.core(), opCore, circuit, commons, jac, opSolution, solution, states, delayLines_, delayBindings_) { 
}

Analysis* Tran::create(PTAnalysis& ptAnalysis, Circuit& circuit, Status& s) {
    auto* an = new Tran(ptAnalysis.name(), circuit, ptAnalysis);
    return an;
}

void Tran::clearOutputDescriptors() {
    // This function is called once before analysis/sweep is started
    // and before any output descriptors are added. 
    // Disable op analysis output. 
    params.core().opParams.write = 0;
    
    opCore.clearOutputDescriptors();
    tranCore.clearOutputDescriptors();
}

bool Tran::addCommonOutputDescriptor(const OutputDescriptor& desc) {
    // Any error causes immediate exit
    bool s1 = opCore.addOutputDescriptor(desc);
    bool s2 = tranCore.addOutputDescriptor(desc);
    return s1 && s2;
}

bool Tran::addCoreOutputDescriptors(ErrorConsumer& errors) {
    // False is returned if something goes wrong
    if (!opCore.addCoreOutputDescriptors(errors)) {
        return false;
    }
    if (!tranCore.addCoreOutputDescriptors(errors)) {
        return false;
    }
    return true;
}

bool Tran::resolveOutputDescriptors(bool strict, ErrorConsumer& errors) {
    // Any error causes immediate exit if strict is true
    // Before exit an error message is formatted and status is set
    if (!opCore.resolveOutputDescriptors(strict, errors)) {
        if (strict) {
            return false;
        }
    }
    if (!tranCore.resolveOutputDescriptors(strict, errors)) {
        if (strict) {
            return false;
        }
    }
    return true;
}

bool Tran::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
    // Tran saves
    static const auto idOpDefault = Id("default");
    static const auto idOpFull = Id("full");
    static const auto idV = Id("v");
    static const auto idI = Id("i");
    static const auto idP = Id("p");

    // When verification is not required, save-resolution errors are not fatal.
    ErrorConsumer sink;
    ErrorConsumer& e1 = verify ? errors : sink;

    bool st = true;
    if (save.typeName() == idOpDefault) {
        st = tranCore.addAllUnknowns(save, e1);
    } else if (save.typeName() == idOpFull) {
        st = tranCore.addAllNodes(save, e1);
    } else if (save.typeName() == idV) {
        st = tranCore.addNode(save, e1);
    } else if (save.typeName() == idI) {
        st = tranCore.addFlow(save, e1);
    } else if (save.typeName() == idP) {
        st = tranCore.addInstanceOutvar(save, e1);
    } else {
        // Report error only if verification is required
        if (verify) {
            errors.push(AnUnsupportedSaveDirective{save.location()});
            return false;
        } else {
            // No verification required, OK
            return true;
        }
    }
    if (verify && !st) {
        errors.push(AnSaveDirectiveLocation{save.location()});
        return false;
    }

    // No error
    return true;
}

bool Tran::addDefaultOutputDescriptors(ErrorConsumer& errors) {
    // Must be invoked on all cores regardless of return value
    auto s1 = opCore.addDefaultOutputDescriptors(errors);
    auto s2 = tranCore.addDefaultOutputDescriptors(errors);
    return s1 && s2;
}

bool Tran::initializeOutputs(ErrorConsumer& errors) {
    // Any error exits immediately
    if (!opCore.initializeOutputs(prefixedName_+".op", errors)) {
        return false;
    }
    if (!tranCore.initializeOutputs(prefixedName_, errors)) {
        return false;
    }
    return true;
}

bool Tran::finalizeOutputs(ErrorConsumer& errors) {
    // Finalization has to be performed on all cores, regardless of errors
    auto ok1 = opCore.finalizeOutputs(errors);
    auto ok2 = tranCore.finalizeOutputs(errors);
    return ok1 && ok2;
}

bool Tran::deleteOutputs(ErrorConsumer& errors) {
    // Output needs to be deleted for all cores
    auto ok1 = opCore.deleteOutputs(prefixedName_+".op", errors);
    auto ok2 = tranCore.deleteOutputs(prefixedName_, errors);
    return ok1 && ok2;
}

bool Tran::rebuildCores(ErrorConsumer& errors) {
    // Create Jacobian - it is common to Op and tran core, so we need to rebuild it here
    if (!jac.rebuild(circuit.sparsityMap(), circuit.unknownCount(), errors)) {
        return false;
    }

    // std::cout << "Sparsity pattern" << std::endl;
    // jac->dumpSparsityTables(std::cout);
    // std::cout << std::endl;
    // jac->dumpSparsity(std::cout);
    // std::cout << std::endl;

    // First rebuild the tranCore because its rebuild function stores ICs 
    // in slot 2 of opCore's nrSolver's forces. 
    if (!tranCore.rebuild(errors)) {
        return false;
    }
    if (!opCore.rebuild(errors)) {
        return false;
    }
    return true;
}

size_t Tran::analysisStateStorageSize() const { 
    // Only op core has storage
    return opCore.stateStorageSize();
}

size_t Tran::allocateAnalysisStateStorage(size_t n) { 
    // Only op core has storage
    return opCore.allocateStateStorage(n);
}

void Tran::deallocateAnalysisStateStorage(size_t n) { 
    // Only op core has storage
    opCore.deallocateStateStorage(n);
}

bool Tran::storeState(size_t ndx, bool storeDetails) {
    // Only op core has storage
    return opCore.storeState(ndx, storeDetails);
}

bool Tran::restoreState(size_t ndx) {
    // Only op core has storage
    return opCore.restoreState(ndx);
}

void Tran::makeStateIncoherent(size_t ndx) {
    opCore.makeStateIncoherent(ndx);
}

std::tuple<bool, bool> Tran::preMapping(ErrorConsumer& errors) {
    auto [ok, needsMapping] = opCore.preMapping(errors);
    if (!ok) {
        return std::make_tuple(false, needsMapping);
    }
    auto [ok1, map1] = tranCore.preMapping(errors);
    return std::make_tuple(ok&&ok1, needsMapping||map1);

}

bool Tran::populateStructures(ErrorConsumer& errors) {
    auto ok = opCore.populateStructures(errors);
    if (!ok) {
        return false;
    }
    return tranCore.populateStructures(errors);
}

void Tran::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: transient analysis"<< std::endl;
    os << "OP analysis core:" << std::endl;
    opCore.dump(os);
    os << std::endl;
    os << "Transient analysis core:" << std::endl;
    tranCore.dump(os);
}

}
