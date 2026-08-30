#include "annoise.h"
#include "common.h"


namespace NAMESPACE {

template<> SmallSignal<NoiseCore, NoiseData>::SmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis) 
    : Analysis(name, circuit, ptAnalysis),
      opCore(*this, params.core().opParams, circuit, commons, jac, solution, states, delayLines_, opDelayBindings_),
      smsigCore(
        *this, params.core(), opCore, contributionOffset, circuit, commons,
        jac, solution, states, acMatrix, acSolution, results, powerGain, outputNoise,
        delayLines_, smsigDelayBindings_
      ) {
}

template<> bool SmallSignal<NoiseCore, NoiseData>::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
    // Noise saves
    static const auto idDefault = Id("default");
    static const auto idFull = Id("full");
    static const auto idN  = Id("n");
    static const auto idNc = Id("nc");

    // When verification is not required, save-resolution errors are not fatal.
    ErrorConsumer sink;
    ErrorConsumer& e1 = verify ? errors : sink;

    bool st = true;
    bool handled = true;
    bool addLoc = true;
    if (save.typeName() == idDefault) {
        st = smsigCore.addAllNoiseContribInst(save, false, e1);
    } else if (save.typeName() == idFull) {
        st = smsigCore.addAllNoiseContribInst(save, true, e1);
    } else if (save.typeName() == idN) {
        st = smsigCore.addNoiseContribInst(save, false, e1);
    } else if (save.typeName() == idNc) {
        st = smsigCore.addNoiseContribInst(save, true, e1);
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

template<> void SmallSignal<NoiseCore, NoiseData>::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: noise analysis"<< std::endl;
    os << "OP analysis core:" << std::endl;
    opCore.dump(os);
    os << std::endl;
    os << "Noise analysis core:" << std::endl;
    smsigCore.dump(os);
}

}
