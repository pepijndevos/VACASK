#include <cmath>
#include "anhb.h"
#include "introspection.h"
#include "devbase.h"
#include <iomanip>
#include <cmath>
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

HB::HB(Id name, Circuit& circuit, PTAnalysis& ptAnalysis) 
    : Analysis(name, circuit, ptAnalysis),
      core(*this, params.core(), circuit, commons, jacColoc, jac, solution, delayLines_, delayBindings_) {
};

Analysis* HB::create(PTAnalysis& ptAnalysis, Circuit& circuit, Status& s) {
    auto* an = new HB(ptAnalysis.name(), circuit, ptAnalysis);
    return an;
}

void HB::clearOutputDescriptors() {
    core.clearOutputDescriptors();
}

bool HB::addCommonOutputDescriptor(const OutputDescriptor& desc) {
    return core.addOutputDescriptor(desc);
}

bool HB::addCoreOutputDescriptors(ErrorConsumer& errors) {
    if (!core.addCoreOutputDescriptors(errors)) {
        return false;
    }
    return true;
}

bool HB::resolveOutputDescriptors(bool strict, ErrorConsumer& errors) {
    // Trigger resolving in core analyses
    return core.resolveOutputDescriptors(strict, errors);
}

bool HB::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
    static const auto idDefault = Id("default");
    static const auto idFull = Id("full");
    static const auto idV = Id("v");
    static const auto idI = Id("i");
    static const auto idP = Id("p");

    // When verification is not required, save-resolution errors are not fatal.
    ErrorConsumer sink;
    ErrorConsumer& e1 = verify ? errors : sink;
    // TODO: handle output variables someday
    bool st = true;
    if (save.typeName() == idDefault) {
        st = core.addAllUnknowns(save, e1);
    } else if (save.typeName() == idFull) {
        st = core.addAllNodes(save, e1);
    } else if (save.typeName() == idV) {
        st = core.addNode(save, e1);
    } else if (save.typeName() == idI) {
        st = core.addFlow(save, e1);
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

bool HB::addDefaultOutputDescriptors(ErrorConsumer& errors) {
    return core.addDefaultOutputDescriptors(errors);
}

bool HB::initializeOutputs(ErrorConsumer& errors) {
    return core.initializeOutputs(prefixedName_, errors);
}

bool HB::finalizeOutputs(ErrorConsumer& errors) {
    return core.finalizeOutputs(errors);
}

bool HB::deleteOutputs(ErrorConsumer& errors) {
    return core.deleteOutputs(prefixedName_, errors);
}

bool HB::rebuildCores(ErrorConsumer& errors) {
    // Jacobian will be built by the core
    return core.rebuild(errors);
}

size_t HB::analysisStateStorageSize() const { 
    return core.stateStorageSize();
}

size_t HB::allocateAnalysisStateStorage(size_t n) { 
    return core.allocateStateStorage(n);
}

void HB::deallocateAnalysisStateStorage(size_t n) { 
    core.deallocateStateStorage(n);
}

bool HB::storeState(size_t ndx, bool storeDetails) {
    return core.storeState(ndx, storeDetails);
}

bool HB::restoreState(size_t ndx) {
    return core.restoreState(ndx);
}

void HB::makeStateIncoherent(size_t ndx) {
    core.makeStateIncoherent(ndx);
}

std::tuple<bool, bool> HB::preMapping(ErrorConsumer& errors) {
    return core.preMapping(errors);
}

bool HB::populateStructures(ErrorConsumer& errors) {
    return core.populateStructures(errors);
}

void HB::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: harmonic balance"<< std::endl;
    os << "HB analysis core:" << std::endl;
    core.dump(os);
}

}
