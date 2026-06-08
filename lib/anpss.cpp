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
      opCore_(*this, params.core().opParams, circuit, commons, jac_, solution_, states_),
      stabilTran_(*this, params.core().stabilParams, opCore_, circuit, commons, jac_, solution_, states_),
      pssTran_(*this, params.core().shootParams, opCore_, circuit, commons, jac_, solution_, states_),
      pssCore_(*this, params.core(), circuit, commons, jac_, solution_, states_, opCore_, stabilTran_, pssTran_) {
}

Analysis* Pss::create(PTAnalysis& ptAnalysis, Circuit& circuit, Status& s) {
    return new Pss(ptAnalysis.name(), circuit, ptAnalysis);
}

void Pss::clearOutputDescriptors() {
    // Suppress operating-point output; the PSS trajectory is what is saved.
    params.core().opParams.write = 0;
    opCore_.clearOutputDescriptors();
    stabilTran_.clearOutputDescriptors();
    pssTran_.clearOutputDescriptors();
    pssCore_.clearOutputDescriptors();
}

bool Pss::addCommonOutputDescriptor(const OutputDescriptor& desc) {
    // Common output descriptor (i.e. sweep variables)
    // opCore_ and pssCore_ output nothing
    bool b2 = stabilTran_.addOutputDescriptor(desc);
    bool b3 = pssTran_.addOutputDescriptor(desc);
    return b2 && b3;
}

// shootParams.write is not exposed directly because it is manipulated during shooting. 
// Therefore it must be set to write so that descriptors are populated. 

bool Pss::addCoreOutputDescriptors(Status& s) {
    // Output descriptors that are specific to a core (i.e. time, frequency)
    // opCore_ and pssCore_ output nothing
    params.core().shootParams.write = params.core().write;
    if (!stabilTran_.addCoreOutputDescriptors(s)) {
        return false;
    }
    if (!pssTran_.addCoreOutputDescriptors(s)) {
        return false;
    }
    return true;
}

bool Pss::resolveOutputDescriptors(bool strict, Status& s) {
    // Resolve output descriptors into output sources
    // opCore_ and pssCore_ output nothing
    if (!stabilTran_.resolveOutputDescriptors(strict, s)) {
        if (strict) {
            return false;
        }
    }
    if (!pssTran_.resolveOutputDescriptors(strict, s)) {
        if (strict) {
            return false;
        }
    }
    return true;
}

bool Pss::resolveSave(const PTSave& save, bool verify, Status& s) {
    // Dispatch saves to cores - create output descriptors
    // opCore_ and pssCore_ output nothing
    static const auto idDefault = Id("default");
    static const auto idFull    = Id("full");
    static const auto idV       = Id("v");
    static const auto idI       = Id("i");
    static const auto idP       = Id("p");

    bool st = true;
    if (save.typeName() == idDefault) {
        st = stabilTran_.addAllUnknowns(save, s);
        st = st && pssTran_.addAllUnknowns(save, s);
    } else if (save.typeName() == idFull) {
        st = stabilTran_.addAllNodes(save, s);
        st = st && stabilTran_.addAllNodes(save, s);
    } else if (save.typeName() == idV) {
        st = stabilTran_.addNode(save, s);
        st = st && stabilTran_.addNode(save,s );
    } else if (save.typeName() == idI) {
        st = stabilTran_.addFlow(save, s);
        st = st && stabilTran_.addFlow(save, s);
    } else if (save.typeName() == idP) {
        st = stabilTran_.addInstanceOutvar(save, s);
        st = st && stabilTran_.addInstanceOutvar(save, s);
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

bool Pss::addDefaultOutputDescriptors(Status& s) {
    // opCore_ and pssCore_ output nothing
    params.core().shootParams.write = params.core().write;
    auto s1 = stabilTran_.addDefaultOutputDescriptors(s);
    auto s2 = pssTran_.addDefaultOutputDescriptors(s);
    return s1 && s2;
}

bool Pss::initializeOutputs(Status& s) {
    // opCore_ and pssCore_ output nothing
    if (!stabilTran_.initializeOutputs(std::string(name_)+".tran", s)) {
        return false;
    }
    if (!pssTran_.initializeOutputs(name_, s)) {
        return false;
    }
    return true;
}

bool Pss::finalizeOutputs(Status& s) {
    // opCore_ and pssCore_ output nothing
    auto ok1 = stabilTran_.finalizeOutputs(s);
    auto ok2 = pssTran_.finalizeOutputs(s);
    return ok1 && ok2;
}

bool Pss::deleteOutputs(Status& s) {
    params.core().shootParams.write = params.core().write;
    auto ok1 = stabilTran_.deleteOutputs(std::string(name_)+".tran", s);
    auto ok2 = pssTran_.deleteOutputs(name_, s);
    return ok1 && ok2;
}

std::tuple<bool, bool> Pss::preMapping(Status& s) {
    // Forward IC and icmode to stabilParams so stabilTran_.preMapping()
    // sees them and populates preprocessedIc before rebuild() is called.
    auto& core = params.core();
    bool hasIc = (core.ic.type() == Value::Type::ValueVec);
    if (hasIc) {
        core.stabilParams.ic     = core.ic;
        core.stabilParams.icmode = TranCore::icmodeUic;
    }
    auto [ok, needsMapping] = stabilTran_.preMapping(s);
    return std::make_tuple(ok, needsMapping);
}

bool Pss::rebuildCores(Status& s) {
    // Create Jacobian - it is common to PSS, Op and tran core, so we need to rebuild it here
    if (!jac_.rebuild(circuit.sparsityMap(), circuit.unknownCount())) {
        jac_.formatError(s);
        return false;
    }

    // First rebuild the stabilTran core because its rebuild function stores ICs 
    // in slot 2 of opCore's nrSolver's forces. 
    if (!stabilTran_.rebuild(s)) {
        return false;
    }
    if (!opCore_.rebuild(s)) {
        return false;
    }
    if (!pssTran_.rebuild(s)) {
        return false;
    }
    if (!pssCore_.rebuild(s)) {
        return false;
    }
    return true;
}

void Pss::dump(std::ostream& os) const {
    Analysis::dump(os);
    os << "Analysis type: periodic steady-state (PSS)" << std::endl;
    os << "PSS core:" << std::endl;
    pssCore_.dump(os);
}

} // namespace NAMESPACE
