#include <iomanip>
#include <cmath>
#include <filesystem>
#include "coredcinc.h"
#include "simulator.h"
#include "common.h"
#include "densematrix.h"

namespace NAMESPACE {

// Default parameters
DCIncrementalParameters::DCIncrementalParameters() {
    opParams.write = 0; 
}

template<> int Introspection<DCIncrementalParameters>::setup() {
    registerMember(write);
    registerNamedMember(opParams.write, "writeop");
    registerNamedMember(opParams.nodeset, "nodeset");
    registerNamedMember(opParams.store, "store");

    return 0;
}
instantiateIntrospection(DCIncrementalParameters);


DCIncrementalCore::DCIncrementalCore(
    OutputDescriptorResolver& parentResolver, DCIncrementalParameters& params, OperatingPointCore& opCore, Circuit& circuit, 
    CommonData& commons, KluRealMatrix& jacobian, Vector<double>& incrementalSolution
) : AnalysisCore(parentResolver, circuit, commons), params(params), outfile(nullptr), opCore_(opCore), 
    jacobian(jacobian), incrementalSolution(incrementalSolution) {

    // Set analysis type for the initial operating point analysis
    auto& elsSystem = opCore_.solver().evalSetup();
    elsSystem.staticAnalysis = true;
    elsSystem.dcAnalysis = false;
    elsSystem.acAnalysis = true;
}

DCIncrementalCore::~DCIncrementalCore() {
    delete outfile;
}

bool DCIncrementalCore::resolveOutputDescriptors(bool strict, ErrorConsumer& errors) {
    // Clear output sources
    outputSources.clear();
    // Resolve output descriptors
    bool ok = true; 
    for (auto it = outputDescriptors.cbegin(); it != outputDescriptors.cend(); ++it) {
        Node *node;
        Instance *inst;
        switch (it->type) {
        case OutdSolComponent:
            ok = addRealVarOutputSource(strict, it->id, incrementalSolution, it->id, errors);
            break;
        default:
            // Delegate to parent
            ok = parentResolver.resolveOutputDescriptor(*it, outputSources, strict, errors);
            break;
        }
        if (!ok) {
            break;
        }
    }
    return ok;
}

bool DCIncrementalCore::addDefaultOutputDescriptors(ErrorConsumer& errors) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    if (savesCount==0) {
        return addAllUnknowns(PTSave("default", Id(), Id()), errors);
    }
    return true;
}

bool DCIncrementalCore::initializeOutputs(const std::string& name, ErrorConsumer& errors) {
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    // Create output file if not created yet
    if (!outfile) {
        outfile = new OutputRawfile(
            name, outputSources,
            (circuit.simulatorOptions().core().rawfile==SimulatorOptions::rawfileBinary ? OutputRawfile::Flags::Binary : OutputRawfile::Flags::None) |
                OutputRawfile::Flags::Padded);
        outfile->setTitle(circuit.title());
        outfile->setPlotname("DC Incremental Response Analysis");
    }
    outfile->prologue();

    return true;
}

bool DCIncrementalCore::finalizeOutputs(ErrorConsumer& errors) {
    if (outfile) {
        outfile->epilogue();
        delete outfile;
        outfile = nullptr;
    }
    return true;
}

bool DCIncrementalCore::deleteOutputs(Id name, ErrorConsumer& errors) {
    if (!params.write || Simulator::noOutput()) {
        return true;
    }

    // Cannot assume outfile is available
    auto fname = std::string(name)+".raw";
    if (std::filesystem::exists(fname)) {
        std::filesystem::remove(fname);
    }
    return true;
}
    
bool DCIncrementalCore::rebuild(ErrorConsumer& errors) {
    return true;
}

// System of equations is 
//   G(x) dx = dJ
CoreCoroutine DCIncrementalCore::coroutine(bool continuePrevious, ErrorConsumer& errors) {
    initProgress(1, 0);

    jacobian.setAccounting(circuit.tables().accounting());

    auto n = circuit.unknownCount();
    // Make sure structures are large enough
    incrementalSolution.resize(n+1);
    
    auto opOk = opCore_.run(continuePrevious, errors);
    if (!opOk) {
        errors.push(DcIncOperatingPointFailed{});
        co_yield CoreState::Aborted;
    }

    LoadSetup lsRhs { 
        // Outputs
        .dcIncrementResidual = incrementalSolution.data()
    };

    auto& options = circuit.simulatorOptions().core();
    Int debug = options.smsig_debug;

    if (debug>0) {
        Simulator::dbg() << "Starting DC incremental analysis.\n";
    }

    // Jacobian is already factored (done by op core)
    
    // Prepare RHS (add excitations given by delta parameter)
    zero(incrementalSolution);
    auto filter = [](Device* device) { return device->checkFlags(Device::Flags::GeneratesDCIncremental); };
    if (!circuit.evalAndLoad(commons, nullptr, &lsRhs, filter, errors)) {
        // Load error
        errors.push(DcIncEvalAndLoadFailed{});
        if (debug>0) {
            Simulator::dbg() << "Error in DC incremental excitation load.\n";
        }
        co_yield CoreState::Aborted;
    }

    // Change sign of residual because it is on the RHS
    // and we need the small signal response with the correct sign
    VectorView incrementalSolutionView(incrementalSolution);
    incrementalSolutionView.scale(-1.0);

    if (debug>=100) {
        Simulator::dbg() << "Linear system\n";
        jacobian.dump(Simulator::dbg(), dataWithoutBucket(incrementalSolution, bucketSize)); 
        Simulator::dbg() << "\n";
    }

    // We don't need max residual contribution because we do not check residual

    // Solve 
    if (!jacobian.solve(dataWithoutBucket(incrementalSolution, bucketSize), errors)) {
        errors.push(DcIncMatrixError{});
        co_yield CoreState::Aborted;
    }

    // Set solution bucket to 0
    incrementalSolution[0] = 0.0;

    if (options.solutioncheck && !jacobian.isFinite(dataWithoutBucket(incrementalSolution, bucketSize), true, true, errors)) {
        errors.push(DcIncSolutionNotFinite{});
        if (options.smsig_debug) {
            Simulator::dbg() << "A solution entry is not finite. Solver failed.\n";
        }
        co_yield CoreState::Aborted;
    }
    
    if (debug>0) {
        Simulator::dbg() << "DC incremental analysis finished.\n";
    }

    // Dump solution
    if (params.write && !Simulator::noOutput() && outfile) {
        outfile->addPoint();
    }
    
    setProgress(1);

    co_yield CoreState::Finished;
}

bool DCIncrementalCore::run(bool continuePrevious, ErrorConsumer& errors) {
    auto c = coroutine(continuePrevious, errors);
    bool ok = true;
    while (!c.done()) {
        if (c.resume()==CoreState::Aborted) {
            ok = false;
            break;
        };
    }
    return ok;
}

void DCIncrementalCore::dump(std::ostream& os) const {
    AnalysisCore::dump(os);
    os << "  Results" << std::endl;
    auto n = circuit.unknownCount();
    for(decltype(n) i=1; i<=n; i++) {
        auto rn = circuit.reprNode(i);
        os << "    " << rn->name() << " : " << incrementalSolution.data()[i] << "\n";
    }
}


}
