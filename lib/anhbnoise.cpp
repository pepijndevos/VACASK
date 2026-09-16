#include "anhbnoise.h"
#include "common.h"


namespace NAMESPACE {

template<> HBSmallSignal<HBNoiseCore, HBNoiseData>::HBSmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis)
    : Analysis(name, circuit, ptAnalysis),
      hbCore(*this, params.core().hbParams, circuit, commons, jacColoc, jac, solution, delayLines_, hbDelayBindings_),
      smsigCore(
        *this, params.core(), hbCore, contributionOffset, circuit, commons,
        jacSpec, hbSolution, acMatrix, acSolution, results, powerGain, outputNoise,
        delayLines_, smsigDelayBindings_
      ) {
}

template<> bool HBSmallSignal<HBNoiseCore, HBNoiseData>::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
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
        // Handle HB saves
        std::tie(st, handled) = resolveHbSave(save, verify, e1);
        // resolveHbSave() adds location to error
        addLoc = false;
        // Not handled error was formatted by resolveHbSave()
        // Also all HB errors were formatted
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

template<> void HBSmallSignal<HBNoiseCore, HBNoiseData>::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: HBNOISE (quasi)periodic small-signal noise" << std::endl;
    os << "HB analysis core:" << std::endl;
    hbCore.dump(os);
    os << std::endl;
    os << "HB noise analysis core:" << std::endl;
    smsigCore.dump(os);
}

}
