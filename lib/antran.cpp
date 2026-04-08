#include "antran.h"
#include "common.h"


namespace NAMESPACE {

Tran::Tran(Id name, Circuit& circuit, PTAnalysis& ptAnalysis) 
    : Analysis(name, circuit, ptAnalysis), 
      opCore(*this, params.core().opParams, circuit, commons, jac, solution, states), 
      tranCore(*this, params.core(), opCore, circuit, commons, jac, solution, states) { 
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

bool Tran::addCoreOutputDescriptors(Status& s) {
    // False is returned if something goes wrong
    if (!opCore.addCoreOutputDescriptors(s)) {
        return false;
    }
    if (!tranCore.addCoreOutputDescriptors(s)) {
        return false;
    }
    return true;
}

bool Tran::resolveOutputDescriptors(bool strict, Status& s) {
    // Any error causes immediate exit if strict is true
    // Before exit an error message is formatted and status is set
    if (!opCore.resolveOutputDescriptors(strict, s)) {
        if (strict) {
            return false;
        }
    }
    if (!tranCore.resolveOutputDescriptors(strict, s)) {
        if (strict) {
            return false;
        }
    }
    return true;
}

bool Tran::resolveSave(const PTSave& save, bool verify, Status& s) {
    // Tran saves
    static const auto idOpDefault = Id("default");
    static const auto idOpFull = Id("full");
    static const auto idV = Id("v");
    static const auto idI = Id("i");
    static const auto idP = Id("p");

    bool st = true;
    Status& s1 = verify ? s : Status::ignore;
    if (save.typeName() == idOpDefault) {
        st = tranCore.addAllUnknowns(save, s1);
    } else if (save.typeName() == idOpFull) {
        st = tranCore.addAllNodes(save, s1);
    } else if (save.typeName() == idV) {
        st = tranCore.addNode(save, s1);
    } else if (save.typeName() == idI) {
        st = tranCore.addFlow(save, s1);
    } else if (save.typeName() == idP) {
        st = tranCore.addInstanceOutvar(save, s1);
    } else {
        // Report error only if verification is required
        if (verify) {
            s.set(Status::Save, std::string("Analysis does not support save directive."));
            s.extend(save.location());
            return false;
        } else {
            // No verification required, OK
            return true;
        }
    }
    if (verify && !st) {
        // Format error
        s.extend(save.location());
        return false;
    }

    // No error
    return true;
}

bool Tran::addDefaultOutputDescriptors(Status& s) {
    // Must be invoked on all cores regardless of return value
    auto s1 = opCore.addDefaultOutputDescriptors(s);
    auto s2 = tranCore.addDefaultOutputDescriptors(s);
    return s1 && s2;
}

bool Tran::initializeOutputs(Status& s) {
    // Any error exits immediately
    if (!opCore.initializeOutputs(prefixedName_+".op", s)) {
        opCore.formatError(s);
        return false;
    }
    if (!tranCore.initializeOutputs(prefixedName_, s)) {
        tranCore.formatError(s);
        return false;
    }
    return true;
}

bool Tran::finalizeOutputs(Status& s) {
    // Finalization has to be performed on all cores, regardless of errors
    Status s1, s2;
    auto ok1 = opCore.finalizeOutputs(s1);
    auto ok2 = tranCore.finalizeOutputs(s2);
    if (!ok1) {
        s.set(s1);
    }
    if (!ok2) {
        // Error in tranCore will mask the error in op core
        s.set(s2);
    }
    return ok1 && ok2;
}

bool Tran::deleteOutputs(Status& s) {
    // Output needs to be deleted for all cores
    Status s1, s2;
    auto ok1 = opCore.deleteOutputs(prefixedName_+".op", s1);
    auto ok2 = tranCore.deleteOutputs(prefixedName_, s2);
    if (!ok1) {
        s.set(s1);
    }
    if (!ok2) {
        // Error in tranCore will mask the error in op core
        s.set(s2);
    }
    return ok1 && ok2;
}

bool Tran::rebuildCores(Status& s) {
    // Create Jacobian - it is common to Op and tran core, so we need to rebuild it here
    if (!jac.rebuild(circuit.sparsityMap(), circuit.unknownCount())) {
        jac.formatError(s);
        return false;
    }

    // std::cout << "Sparsity pattern" << std::endl;
    // jac->dumpSparsityTables(std::cout);
    // std::cout << std::endl;
    // jac->dumpSparsity(std::cout);
    // std::cout << std::endl;

    // First rebuild the tranCore because its rebuild function stores ICs 
    // in slot 2 of opCore's nrSolver's forces. 
    if (!tranCore.rebuild(s)) {
        return false;
    }
    if (!opCore.rebuild(s)) {
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

std::tuple<bool, bool> Tran::preMapping(Status& s) {
    auto [ok, needsMapping] = opCore.preMapping(s);
    if (!ok) {
        return std::make_tuple(false, needsMapping);
    }
    auto [ok1, map1] = tranCore.preMapping(s);
    return std::make_tuple(ok&&ok1, needsMapping||map1);
    
}

bool Tran::populateStructures(Status& s) {
    auto ok = opCore.populateStructures(s);
    if (!ok) {
        return false;
    }
    return tranCore.populateStructures(s);
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
