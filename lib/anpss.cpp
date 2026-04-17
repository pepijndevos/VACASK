// anpss.cpp
//
// Pss analysis — wires PssCore into the simulator's analysis dispatch layer.
//
// Follows the same structure as antran.cpp. Pss owns the shared KluRealMatrix,
// solution, and states repositories and passes them by reference to PssCore.
// All output descriptor management routes through PssCore's proxy methods,
// which forward to the internal PssTranCore that actually writes the output.

#include "anpss.h"
#include "common.h"


namespace NAMESPACE {

Pss::Pss(Id name, Circuit& circuit, PTAnalysis& ptAnalysis)
    : Analysis(name, circuit, ptAnalysis),
      pssCore_(*this, params_.core(), circuit, commons, jac_, solution_, states_) {
}

Analysis* Pss::create(PTAnalysis& ptAnalysis, Circuit& circuit, Status& s) {
    return new Pss(ptAnalysis.name(), circuit, ptAnalysis);
}

void Pss::clearOutputDescriptors() {
    // Suppress operating-point output; the PSS trajectory is what is saved.
    params_.core().opParams.write = 0;
    pssCore_.clearOutputDescriptors();
}

bool Pss::addCommonOutputDescriptor(const OutputDescriptor& desc) {
    // Route common descriptors (e.g. from parameter sweeps) to pssCore_,
    // which forwards them to the internal shooting transient.
    return pssCore_.addOutputDescriptor(desc);
}

bool Pss::addCoreOutputDescriptors(Status& s) {
    if (!pssCore_.addCoreOutputDescriptors()) {
        pssCore_.formatError(s);
        return false;
    }
    return true;
}

bool Pss::resolveOutputDescriptors(bool strict, Status& s) {
    if (!pssCore_.resolveOutputDescriptors(strict, s)) {
        if (strict) {
            pssCore_.formatError(s);
            return false;
        }
    }
    return true;
}

bool Pss::resolveSave(const PTSave& save, bool verify, Status& s) {
    static const auto idDefault = Id("default");
    static const auto idFull    = Id("full");
    static const auto idV       = Id("v");
    static const auto idI       = Id("i");
    static const auto idP       = Id("p");

    bool st = true;
    if (save.typeName() == idDefault) {
        st = pssCore_.addAllUnknowns(save);
    } else if (save.typeName() == idFull) {
        st = pssCore_.addAllNodes(save);
    } else if (save.typeName() == idV) {
        st = pssCore_.addNode(save);
    } else if (save.typeName() == idI) {
        st = pssCore_.addFlow(save);
    } else if (save.typeName() == idP) {
        st = pssCore_.addInstanceOutvar(save);
    } else {
        if (verify) {
            s.set(Status::Save, std::string("Analysis does not support save directive."));
            s.extend(save.location());
            return false;
        }
        return true;
    }

    if (verify && !st) {
        pssCore_.formatError(s);
        s.extend(save.location());
        return false;
    }
    return true;
}

bool Pss::addDefaultOutputDescriptors() {
    return pssCore_.addDefaultOutputDescriptors();
}

bool Pss::initializeOutputs(Status& s) {
    if (!pssCore_.initializeOutputs(name_, s)) {
        pssCore_.formatError(s);
        return false;
    }
    return true;
}

bool Pss::finalizeOutputs(Status& s) {
    auto ok = pssCore_.finalizeOutputs(s);
    if (!ok) {
        pssCore_.formatError(s);
    }
    return ok;
}

bool Pss::deleteOutputs(Status& s) {
    auto ok = pssCore_.deleteOutputs(name_, s);
    if (!ok) {
        pssCore_.formatError(s);
    }
    return ok;
}

bool Pss::rebuildCores(Status& s) {
    // PssCore::rebuild() handles jac, opCore_, stabilTran_, and pssTran_ in
    // the correct dependency order.
    return pssCore_.rebuild(s);
}

void Pss::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: periodic steady-state (PSS)" << std::endl;
    os << "PSS core:" << std::endl;
    pssCore_.dump(os);
}

} // namespace NAMESPACE
