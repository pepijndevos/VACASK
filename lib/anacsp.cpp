#include "anacsp.h"
#include "common.h"


namespace NAMESPACE {

template<> SmallSignal<ACSPCore, ACSPData>::SmallSignal(Id name, Circuit& circuit, PTAnalysis& ptAnalysis) 
    : Analysis(name, circuit, ptAnalysis), 
      opCore(*this, params.core().opParams, circuit, commons, jac, solution, states), 
      smsigCore(*this, params.core(), opCore, circuit, commons, jac, solution, states, acMatrix, acSolution, yMatrix, stMatrix) {
}

template<> bool SmallSignal<ACSPCore, ACSPData>::resolveSave(const PTSave& save, bool verify, Status& s) {
    // ACSP saves
    
    bool st = true;
    bool handled = true;

    // Check number of ports
    auto portCount = params.core().ports.size() / 2;
    if (portCount<=0) {
        s.set(Status::BadArguments, "Ports vector must define at least one port.");
        return false;
    }
    if (params.core().ports.size() % 2 != 0) {
        s.set(Status::BadArguments, "Ports vector must define an even number of components.");
        return false;
    }

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

template<> void SmallSignal<ACSPCore, ACSPData>::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: STB"<< std::endl;
    os << "OP analysis core:" << std::endl;
    opCore.dump(os);
    os << std::endl;
    os << "SP analysis core:" << std::endl;
    smsigCore.dump(os);
}

}
