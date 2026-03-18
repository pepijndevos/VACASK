#include "anacstb.h"
#include "common.h"


namespace NAMESPACE {

template<> SmallSignal<ACStbCore, StbData>::SmallSignal(Id name, Circuit& circuit, PTAnalysis& ptAnalysis) 
    : Analysis(name, circuit, ptAnalysis), 
      opCore(*this, params.core().opParams, circuit, commons, jac, solution, states), 
      smsigCore(*this, params.core(), opCore, circuit, commons, jac, solution, states, acMatrix, acSolution, resultsVector) {
}

template<> bool SmallSignal<ACStbCore, StbData>::resolveSave(const PTSave& save, bool verify, Status& s) {
    // ACStb saves
    
    bool st = true;
    bool handled = true;
    
    // No saves, but those of OP analysis, delegate
    // Handle OP saves
    std::tie(st, handled) = resolveOpSave(save, verify, s); 
    // Not handled error was formatted by resolveOpSave()
    // Also all op errors were formatted
    if (verify) {
        // Verification required, return status
        return st;
    } else {
        // No verification required, OK
        return true;
    }
    
    // Handled save via smsigCore, check error if verification required
    if (verify && !st) {
        // Format error
        smsigCore.formatError(s);
        s.extend(save.location());
        return false;
    } 
    
    // No error
    return true;
}

template<> void SmallSignal<ACStbCore, StbData>::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: STB"<< std::endl;
    os << "OP analysis core:" << std::endl;
    opCore.dump(os);
    os << std::endl;
    os << "STB analysis core:" << std::endl;
    smsigCore.dump(os);
}

}
