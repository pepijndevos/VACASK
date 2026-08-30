#include "andcxf.h"
#include "common.h"


namespace NAMESPACE {

template<> SmallSignal<DCXFCore, DCXFData>::SmallSignal(const std::string& name, Circuit& circuit, PTAnalysis& ptAnalysis) 
    : Analysis(name, circuit, ptAnalysis),
      opCore(*this, params.core().opParams, circuit, commons, jac, solution, states, delayLines_, opDelayBindings_),
      smsigCore(*this, params.core(), opCore, sourceIndex, circuit, commons, jac, incrementalSolution, sources, tf, yin, zin) {
}

template<> bool SmallSignal<DCXFCore, DCXFData>::resolveSave(const PTSave& save, bool verify, ErrorConsumer& errors) {
    // DCXF saves
    static const auto idDefault = Id("default");
    static const auto idTf  = Id("tf");
    static const auto idZin = Id("zin");
    static const auto idYin = Id("yin");

    // When verification is not required, save-resolution errors are not fatal.
    ErrorConsumer sink;
    ErrorConsumer& e1 = verify ? errors : sink;

    bool st = true;
    bool handled = true;
    bool addLoc = true;
    if (save.typeName() == idDefault) {
        st = smsigCore.addAllTfZin(save, sourceIndex, e1);
    } else if (save.typeName() == idTf) {
        st = smsigCore.addTf(save, sourceIndex, e1);
    } else if (save.typeName() == idZin) {
        st = smsigCore.addZin(save, sourceIndex, e1);
    } else if (save.typeName() == idYin) {
        st = smsigCore.addYin(save, sourceIndex, e1);
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

template<> void SmallSignal<DCXFCore, DCXFData>::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: DC TF"<< std::endl;
    os << "OP analysis core:" << std::endl;
    opCore.dump(os);
    os << std::endl;
    os << "DC TF analysis core:" << std::endl;
    smsigCore.dump(os);
}

}