#include "anacsp.h"
#include "common.h"


namespace NAMESPACE {

template<> SmallSignal<ACSPCore, ACSPData>::SmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis) 
    : Analysis(name, circuit, ptAnalysis),
      opCore(*this, params.core().opParams, circuit, commons, jac, solution, states, delayLines_, opDelayBindings_),
      smsigCore(
        *this, params.core(), opCore, circuit, commons,
        jac, solution, states, acMatrix, acSolution, stMatrix,
        delayLines_, smsigDelayBindings_
      ) {
}

template<> bool SmallSignal<ACSPCore, ACSPData>::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
    // ACSP saves

    bool st = true;
    bool handled = true;

    // Check number of ports
    auto portCount = params.core().ports.size() / 2;
    if (portCount<=0) {
        errors.push(SpPortsVectorEmpty{});
        return false;
    }
    if (params.core().ports.size() % 2 != 0) {
        errors.push(SpPortsVectorOdd{});
        return false;
    }

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
