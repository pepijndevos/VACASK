#include <cmath>
#include "anop.h"
#include "introspection.h"
#include "devbase.h"
#include <iomanip>
#include <cmath>
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

OperatingPoint::OperatingPoint(Id name, Circuit& circuit, PTAnalysis& ptAnalysis) 
    : Analysis(name, circuit, ptAnalysis),
      core(*this, params.core(), circuit, commons, jac, solution, states, delayLines_, delayBindings_) {
};

Analysis* OperatingPoint::create(PTAnalysis& ptAnalysis, Circuit& circuit, Status& s) {
    auto* an = new OperatingPoint(ptAnalysis.name(), circuit, ptAnalysis);
    return an;
}

void OperatingPoint::clearOutputDescriptors() {
    core.clearOutputDescriptors();
}

bool OperatingPoint::addCommonOutputDescriptor(const OutputDescriptor& desc) {
    return core.addOutputDescriptor(desc);
}

bool OperatingPoint::addCoreOutputDescriptors(ErrorConsumer& errors) {
    if (!core.addCoreOutputDescriptors(errors)) {
        return false;
    }
    return true;
}

bool OperatingPoint::resolveOutputDescriptors(bool strict, ErrorConsumer& errors) {
    // Trigger resolving in core analyses
    if (!core.resolveOutputDescriptors(strict, errors)) {
        if (strict) {
            return false;
        }
    }
    return true;
}

bool OperatingPoint::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
    static const auto idDefault = Id("default");
    static const auto idFull = Id("full");
    static const auto idV = Id("v");
    static const auto idI = Id("i");
    static const auto idP = Id("p");

    // When verification is not required, errors from save resolution are not fatal
    // and must not reach the caller's error consumer.
    ErrorConsumer sink;
    ErrorConsumer& e1 = verify ? errors : sink;

    bool st = true;
    if (save.typeName() == idDefault) {
        st = core.addAllUnknowns(save, e1);
    } else if (save.typeName() == idFull) {
        st = core.addAllNodes(save, e1);
    } else if (save.typeName() == idV) {
        st = core.addNode(save, e1);
    } else if (save.typeName() == idI) {
        st = core.addFlow(save, e1);
    } else if (save.typeName() == idP) {
        st = core.addInstanceOutvar(save, e1);
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

bool OperatingPoint::addDefaultOutputDescriptors(ErrorConsumer& errors) {
    return core.addDefaultOutputDescriptors(errors);
}

bool OperatingPoint::initializeOutputs(ErrorConsumer& errors) {
    return core.initializeOutputs(prefixedName_, errors);
}

bool OperatingPoint::finalizeOutputs(ErrorConsumer& errors) {
    return core.finalizeOutputs(errors);
}

bool OperatingPoint::deleteOutputs(ErrorConsumer& errors) {
    return core.deleteOutputs(prefixedName_, errors);
}

bool OperatingPoint::rebuildCores(ErrorConsumer& errors) {
    // Create Jacobian - it is common to both cores in small-signal analyses
    // Therefore we build it outside the core before the core is rebuilt.
    if (!jac.rebuild(circuit.sparsityMap(), circuit.unknownCount(), errors)) {
        return false;
    }

    return core.rebuild(errors);
}

size_t OperatingPoint::analysisStateStorageSize() const { 
    return core.stateStorageSize();
}

size_t OperatingPoint::allocateAnalysisStateStorage(size_t n) { 
    return core.allocateStateStorage(n);
}

void OperatingPoint::deallocateAnalysisStateStorage(size_t n) { 
    core.deallocateStateStorage(n);
}

bool OperatingPoint::storeState(size_t ndx, bool storeDetails) {
    return core.storeState(ndx, storeDetails);
}

bool OperatingPoint::restoreState(size_t ndx) {
    return core.restoreState(ndx);
}

void OperatingPoint::makeStateIncoherent(size_t ndx) {
    core.makeStateIncoherent(ndx);
}

std::tuple<bool, bool> OperatingPoint::preMapping(ErrorConsumer& errors) {
    return core.preMapping(errors);
}

bool OperatingPoint::populateStructures(ErrorConsumer& errors) {
    return core.populateStructures(errors);
}

void OperatingPoint::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: operating point"<< std::endl;
    os << "OP analysis core:" << std::endl;
    core.dump(os);
}

}
