#include "anac.h"
#include "common.h"


namespace NAMESPACE {

template<> SmallSignal<ACCore, AcData>::SmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis) 
    : Analysis(name, circuit, ptAnalysis),
      opCore(*this, params.core().opParams, circuit, commons, jac, solution, states, delayLines_, opDelayBindings_),
      smsigCore(
        *this, params.core(), opCore, circuit, commons, 
        jac, solution, states, acMatrix, acSolution, 
        delayLines_, smsigDelayBindings_
      ) {
}

template<> bool SmallSignal<ACCore, AcData>::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
    // AC saves
    static const auto idDefault = Id("default");
    static const auto idFull = Id("full");
    static const auto idDv = Id("dv");
    static const auto idDi = Id("di");

    // When verification is not required, save-resolution errors are not fatal.
    ErrorConsumer sink;
    ErrorConsumer& e1 = verify ? errors : sink;

    bool st = true;
    bool handled = true;
    bool addLoc = true;
    if (save.typeName() == idDefault) {
        st = smsigCore.addAllUnknowns(save, e1);
    } else if (save.typeName() == idFull) {
        st = smsigCore.addAllNodes(save, e1);
    } else if (save.typeName() == idDv) {
        st = smsigCore.addNode(save, e1);
    } else if (save.typeName() == idDi) {
        st = smsigCore.addFlow(save, e1);
    } else {
        // Handle OP saves
        std::tie(st, handled) = resolveOpSave(save, verify, e1);
        // resolveOpSave() adds location to error
        addLoc = false;
        // Not handled error was formatted by resolveOpSave()
        // Also all op errors were formatted
        if (!verify) {
            // No checking, assume status is OK
            st = true;
        }
    }

    // Handled save via smsigCore, check error if verification required
    if (verify && !st) {
        // Format error
        if (addLoc) {
            errors.push(AnSaveDirectiveLocation{save.location()});
        }
        return false;
    }

    // No error
    return true;
}

template<> void SmallSignal<ACCore, AcData>::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: AC small-signal\n";
    os << "OP analysis core:\n";
    opCore.dump(os);
    os << "\n";
    os << "AC small-signal analysis core:\n";
    smsigCore.dump(os);
}

}