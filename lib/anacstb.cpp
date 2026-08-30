#include "anacstb.h"
#include "common.h"


namespace NAMESPACE {

template<> SmallSignal<ACStbCore, ACStbData>::SmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis) 
    : Analysis(name, circuit, ptAnalysis),
      opCore(*this, params.core().opParams, circuit, commons, jac, solution, states, delayLines_, opDelayBindings_),
      smsigCore(
        *this, params.core(), opCore, circuit, commons,
        jac, solution, states, acMatrix, acSolution, resultsVector,
        delayLines_, smsigDelayBindings_
      ) {
}

template<> bool SmallSignal<ACStbCore, ACStbData>::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
    // ACStb saves

    bool st = true;
    bool handled = true;

    // No saves, but those of OP analysis, delegate
    // Handle OP saves
    std::tie(st, handled) = resolveOpSave(save, verify, errors);
    // Not handled error was pushed by resolveOpSave()
    // Also all op errors were pushed
    if (verify) {
        // Verification required, return status
        return st;
    }
    // No verification required, OK
    return true;
}

template<> void SmallSignal<ACStbCore, ACStbData>::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: STB"<< std::endl;
    os << "OP analysis core:" << std::endl;
    opCore.dump(os);
    os << std::endl;
    os << "STB analysis core:" << std::endl;
    smsigCore.dump(os);
}

}
