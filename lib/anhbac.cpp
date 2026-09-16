#include "anhbac.h"
#include "simulator.h"
#include "common.h"


namespace NAMESPACE {

template<> HBSmallSignal<HBACCore, HBAcData>::HBSmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis)
    : Analysis(name, circuit, ptAnalysis),
      hbCore(*this, params.core().hbParams, circuit, commons, jacColoc, jac, solution, delayLines_, hbDelayBindings_),
      smsigCore(*this, params.core(), hbCore, circuit, commons, jacSpec, hbSolution, acMatrix, acSolution, delayLines_, smsigDelayBindings_) {
}

template<> bool HBSmallSignal<HBACCore, HBAcData>::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
    static const auto idDefault  = Id("default");
    static const auto idFull     = Id("full");
    static const auto idDv       = Id("dv");
    static const auto idDi       = Id("di");

    // When verification is not required, save-resolution errors are not fatal.
    ErrorConsumer sink;
    ErrorConsumer& e1 = verify ? errors : sink;

    bool st = true;
    bool handled = true;
    if (save.typeName() == idDefault) {
        st = smsigCore.addAllUnknowns(save, e1);
    } else if (save.typeName() == idFull) {
        st = smsigCore.addAllNodes(save, e1);
    } else if (save.typeName() == idDv) {
        st = smsigCore.addNode(save, e1);
    } else if (save.typeName() == idDi) {
        st = smsigCore.addFlow(save, e1);
    } else {
        std::tie(st, handled) = resolveHbSave(save, verify, e1);
        if (!verify) {
            st = true;
        }
    }

    if (verify && !st) {
        errors.push(AnSaveDirectiveLocation{save.location()});
        return false;
    }
    return true;
}

template<> void HBSmallSignal<HBACCore, HBAcData>::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: HBAC (quasi)periodic small-signal\n";
    os << "HB analysis core:\n";
    hbCore.dump(os);
    os << "\n";
    os << "HBAC small-signal analysis core:\n";
    smsigCore.dump(os);
}

}
