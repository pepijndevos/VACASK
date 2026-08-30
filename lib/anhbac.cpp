#include "anhbac.h"
#include "common.h"


namespace NAMESPACE {

HBAC::HBAC(Id name, Circuit& circuit, PTAnalysis& ptAnalysis)
    : Analysis(name, circuit, ptAnalysis),
      hbCore(*this, params.core().hbParams, circuit, commons, jacColoc, jac, solution, delayLines_, hbDelayBindings_),
      hbacCore(*this, params.core(), hbCore, circuit, commons, jacSpec, hbSolution, acMatrix, acSolution, delayLines_, hbacDelayBindings_) {
}

Analysis* HBAC::create(PTAnalysis& ptAnalysis, Circuit& circuit, Status& s) {
    return new HBAC(ptAnalysis.name(), circuit, ptAnalysis);
}

bool HBAC::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
    static const auto idDefault  = Id("default");
    static const auto idFull     = Id("full");
    static const auto idDv       = Id("dv");
    static const auto idDi       = Id("di");

    // When verification is not required, save-resolution errors are not fatal.
    ErrorConsumer sink;
    ErrorConsumer& e1 = verify ? errors : sink;

    bool st = true;
    bool handled = true;
    if (save.typeName() == idDefault) {
        st = hbacCore.addAllUnknowns(save, e1);
    } else if (save.typeName() == idFull) {
        st = hbacCore.addAllNodes(save, e1);
    } else if (save.typeName() == idDv) {
        st = hbacCore.addNode(save, e1);
    } else if (save.typeName() == idDi) {
        st = hbacCore.addFlow(save, e1);
    } else {
        std::tie(st, handled) = resolveHbSave(save, verify, e1);
        if (!verify) {
            st = true;
        }
    }

    if (verify && !st) {
        errors.push(AnSaveDirectiveLocation{save.location()});
        return false;
    }
    return true;
}

std::tuple<bool, bool> HBAC::resolveHbSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
    static const auto idHbDefault = Id("hbdefault");
    static const auto idHbFull    = Id("hbfull");
    static const auto idV         = Id("v");
    static const auto idI         = Id("i");
    static const auto idP         = Id("p");

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
    } else if (save.typeName() == idP) {
        st = hbCore.addInstanceOutvar(save, e1);
    } else {
        if (verify) {
            errors.push(AnUnsupportedSaveDirective{save.location()});
        }
        return std::make_tuple(false, false);
    }
    if (verify && !st) {
        errors.push(AnSaveDirectiveLocation{save.location()});
    }
    return std::make_tuple(st, true);
}

bool HBAC::addCommonOutputDescriptor(const OutputDescriptor& desc) {
    bool s1 = hbCore.addOutputDescriptor(desc);
    bool s2 = hbacCore.addOutputDescriptor(desc);
    return s1 && s2;
}

bool HBAC::addCoreOutputDescriptors(ErrorConsumer& errors) {
    if (!hbCore.addCoreOutputDescriptors(errors)) {
        return false;
    }
    if (!hbacCore.addCoreOutputDescriptors(errors)) {
        return false;
    }
    return true;
}

bool HBAC::addDefaultOutputDescriptors(ErrorConsumer& errors) {
    auto s1 = hbCore.addDefaultOutputDescriptors(errors);
    auto s2 = hbacCore.addDefaultOutputDescriptors(errors);
    return s1 && s2;
}

void HBAC::clearOutputDescriptors() {
    hbCore.clearOutputDescriptors();
    hbacCore.clearOutputDescriptors();
}

bool HBAC::resolveOutputDescriptors(bool strict, ErrorConsumer& errors) {
    if (!hbCore.resolveOutputDescriptors(strict, errors)) {
        if (strict) {
            return false;
        }
    }
    if (!hbacCore.resolveOutputDescriptors(strict, errors)) {
        if (strict) {
            return false;
        }
    }
    return true;
}

std::tuple<bool, bool> HBAC::preMapping(ErrorConsumer& errors) {
    auto [ok, needsMapping] = hbCore.preMapping(errors);
    if (!ok) {
        return std::make_tuple(false, needsMapping);
    }
    auto [ok1, map1] = hbacCore.preMapping(errors);
    return std::make_tuple(ok && ok1, needsMapping || map1);
}

bool HBAC::populateStructures(ErrorConsumer& errors) {
    if (!hbCore.populateStructures(errors)) {
        return false;
    }
    return hbacCore.populateStructures(errors);
}

bool HBAC::rebuildCores(ErrorConsumer& errors) {
    if (!hbCore.rebuild(errors)) {
        return false;
    }
    if (!hbacCore.rebuild(errors)) {
        return false;
    }
    return true;
}

bool HBAC::initializeOutputs(ErrorConsumer& errors) {
    if (!hbCore.initializeOutputs(prefixedName_+".hb", errors)) {
        return false;
    }
    if (!hbacCore.initializeOutputs(prefixedName_, errors)) {
        return false;
    }
    return true;
}

bool HBAC::finalizeOutputs(ErrorConsumer& errors) {
    auto ok1 = hbCore.finalizeOutputs(errors);
    auto ok2 = hbacCore.finalizeOutputs(errors);
    return ok1 && ok2;
}

bool HBAC::deleteOutputs(ErrorConsumer& errors) {
    auto ok1 = hbCore.deleteOutputs(prefixedName_+".hb", errors);
    auto ok2 = hbacCore.deleteOutputs(prefixedName_, errors);
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
