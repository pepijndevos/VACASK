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

template<> bool SmallSignal<ACCore, AcData>::resolveSave(const PTSave& save, bool verify, Status& s) {
    // AC saves
    static const auto idDefault = Id("default");
    static const auto idFull = Id("full");
    static const auto idDv = Id("dv");
    static const auto idDi = Id("di");

    bool st = true;
    bool handled = true;
    Status& s1 = verify ? s : Status::ignore;
    bool addLoc = true;
    if (save.typeName() == idDefault) {
        st = smsigCore.addAllUnknowns(save, s1);
    } else if (save.typeName() == idFull) {
        st = smsigCore.addAllNodes(save, s1);
    } else if (save.typeName() == idDv) {
        st = smsigCore.addNode(save, s1);
    } else if (save.typeName() == idDi) {
        st = smsigCore.addFlow(save, s1);
    } else {
        // Handle OP saves
        std::tie(st, handled) = resolveOpSave(save, verify, s1); 
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
            s.extend(save.location());
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