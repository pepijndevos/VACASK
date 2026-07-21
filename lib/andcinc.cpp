#include "andcinc.h"
#include "common.h"


namespace NAMESPACE {

template<> SmallSignal<DCIncrementalCore, DCIncrementalData>::SmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis) 
    : Analysis(name, circuit, ptAnalysis), 
      opCore(*this, params.core().opParams, circuit, commons, jac, solution, states), 
      smsigCore(*this, params.core(), opCore, circuit, commons, jac, incrementalSolution) {
}

template<> bool SmallSignal<DCIncrementalCore, DCIncrementalData>::resolveSave(const PTSave& save, bool verify, Status& s) {
    // DC incremental saves
    static const auto idDefault = Id("default");
    static const auto idFull = Id("full");
    static const auto idDv = Id("dv");
    static const auto idDi = Id("di");

    bool st = true;
    bool handled = true;
    bool addLoc = true;
    Status& s1 = verify ? s : Status::ignore;
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

template<> void SmallSignal<DCIncrementalCore, DCIncrementalData>::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: DC incremental"<< std::endl;
    os << "OP analysis core:" << std::endl;
    opCore.dump(os);
    os << std::endl;
    os << "DC incremental analysis core:" << std::endl;
    smsigCore.dump(os);
}

}
