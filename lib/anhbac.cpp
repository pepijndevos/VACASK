#include "anhbac.h"
#include "common.h"


namespace NAMESPACE {

HBAC::HBAC(Id name, Circuit& circuit, PTAnalysis& ptAnalysis)
    : Analysis(name, circuit, ptAnalysis),
      hbCore(*this, params.core().hbParams, circuit, commons, jacColoc, jac, solution),
      hbacCore(*this, params.core(), hbCore, circuit, commons, jacSpec, hbSolution, acMatrix, acSolution) {
}

Analysis* HBAC::create(PTAnalysis& ptAnalysis, Circuit& circuit, Status& s) {
    return new HBAC(ptAnalysis.name(), circuit, ptAnalysis);
}

bool HBAC::resolveSave(const PTSave& save, bool verify, Status& s) {
    static const auto idDefault  = Id("default");
    static const auto idFull     = Id("full");
    static const auto idDv       = Id("dv");
    static const auto idDi       = Id("di");

    bool st = true;
    bool handled = true;
    Status& s1 = verify ? s : Status::ignore;
    if (save.typeName() == idDefault) {
        st = hbacCore.addAllUnknowns(save, s1);
    } else if (save.typeName() == idFull) {
        st = hbacCore.addAllNodes(save, s1);
    } else if (save.typeName() == idDv) {
        st = hbacCore.addNode(save, s1);
    } else if (save.typeName() == idDi) {
        st = hbacCore.addFlow(save, s1);
    } else {
        std::tie(st, handled) = resolveHbSave(save, verify, s1);
        if (!verify) {
            st = true;
        }
    }

    if (verify && !st) {
        s.extend(save.location());
        return false;
    }
    return true;
}

std::tuple<bool, bool> HBAC::resolveHbSave(const PTSave& save, bool verify, Status& s) {
    static const auto idHbDefault = Id("hbdefault");
    static const auto idHbFull    = Id("hbfull");
    static const auto idV         = Id("v");
    static const auto idI         = Id("i");
    static const auto idP         = Id("p");

    bool st = true;
    Status& s1 = verify ? s : Status::ignore;
    if (save.typeName() == idHbDefault) {
        st = hbCore.addAllUnknowns(save, s1);
    } else if (save.typeName() == idHbFull) {
        st = hbCore.addAllNodes(save, s1);
    } else if (save.typeName() == idV) {
        st = hbCore.addNode(save, s1);
    } else if (save.typeName() == idI) {
        st = hbCore.addFlow(save, s1);
    } else if (save.typeName() == idP) {
        st = hbCore.addInstanceOutvar(save, s1);
    } else {
        if (verify) {
            s.set(Status::Save, std::string("Analysis does not support save directive."));
            s.extend(save.location());
        }
        return std::make_tuple(false, false);
    }
    if (verify && !st) {
        s.extend(save.location());
    }
    return std::make_tuple(st, true);
}

bool HBAC::addCommonOutputDescriptor(const OutputDescriptor& desc) {
    bool s1 = hbCore.addOutputDescriptor(desc);
    bool s2 = hbacCore.addOutputDescriptor(desc);
    return s1 && s2;
}

bool HBAC::addCoreOutputDescriptors(Status& s) {
    if (!hbCore.addCoreOutputDescriptors(s)) {
        return false;
    }
    if (!hbacCore.addCoreOutputDescriptors(s)) {
        return false;
    }
    return true;
}

bool HBAC::addDefaultOutputDescriptors(Status& s) {
    auto s1 = hbCore.addDefaultOutputDescriptors(s);
    auto s2 = hbacCore.addDefaultOutputDescriptors(s);
    return s1 && s2;
}

void HBAC::clearOutputDescriptors() {
    params.core().hbParams.write = params.core().writehb;
    hbCore.clearOutputDescriptors();
    hbacCore.clearOutputDescriptors();
}

bool HBAC::resolveOutputDescriptors(bool strict, Status& s) {
    if (!hbCore.resolveOutputDescriptors(strict)) {
        if (strict) {
            return false;
        }
    }
    if (!hbacCore.resolveOutputDescriptors(strict)) {
        if (strict) {
            return false;
        }
    }
    return true;
}

std::tuple<bool, bool> HBAC::preMapping(Status& s) {
    auto [ok, needsMapping] = hbCore.preMapping(s);
    if (!ok) {
        return std::make_tuple(false, needsMapping);
    }
    auto [ok1, map1] = hbacCore.preMapping(s);
    return std::make_tuple(ok && ok1, needsMapping || map1);
}

bool HBAC::populateStructures(Status& s) {
    if (!hbCore.populateStructures(s)) {
        return false;
    }
    return hbacCore.populateStructures(s);
}

bool HBAC::rebuildCores(Status& s) {
    if (!hbCore.rebuild(s)) {
        return false;
    }
    if (!hbacCore.rebuild(s)) {
        return false;
    }
    return true;
}

bool HBAC::initializeOutputs(Status& s) {
    if (!hbCore.initializeOutputs(std::string(name_)+".hb")) {
        hbCore.formatError(s);
        return false;
    }
    if (!hbacCore.initializeOutputs(name_)) {
        hbacCore.formatError(s);
        return false;
    }
    return true;
}

bool HBAC::finalizeOutputs(Status& s) {
    auto ok1 = hbCore.finalizeOutputs();
    auto ok2 = hbacCore.finalizeOutputs();
    if (!ok1) {
        hbCore.formatError(s);
    }
    if (!ok2) {
        hbacCore.formatError(s);
    }
    return ok1 && ok2;
}

bool HBAC::deleteOutputs(Status& s) {
    auto ok1 = hbCore.deleteOutputs(std::string(name_)+".hb");
    auto ok2 = hbacCore.deleteOutputs(name_);
    if (!ok1) {
        hbCore.formatError(s);
    }
    if (!ok2) {
        hbacCore.formatError(s);
    }
    return ok1 && ok2;
}

size_t HBAC::analysisStateStorageSize() const {
    return hbCore.stateStorageSize();
}

size_t HBAC::allocateAnalysisStateStorage(size_t n) {
    return hbCore.allocateStateStorage(n);
}

void HBAC::deallocateAnalysisStateStorage(size_t n) {
    hbCore.deallocateStateStorage(n);
}

bool HBAC::storeState(size_t ndx, bool storeDetails) {
    return hbCore.storeState(ndx, storeDetails);
}

bool HBAC::restoreState(size_t ndx) {
    return hbCore.restoreState(ndx);
}

void HBAC::makeStateIncoherent(size_t ndx) {
    hbCore.makeStateIncoherent(ndx);
}

void HBAC::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: HBAC (quasi)periodic small-signal\n";
    os << "HB analysis core:\n";
    hbCore.dump(os);
    os << "\n";
    os << "HBAC small-signal analysis core:\n";
    hbacCore.dump(os);
}

}
