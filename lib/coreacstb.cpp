#include <iomanip>
#include <cmath>
#include <filesystem>
#include "coreacstb.h"
#include "simulator.h"
#include "answeep.h"
#include "context.h"
#include "common.h"
#include <numbers>

namespace NAMESPACE {

// Default parameters
ACStbParameters::ACStbParameters() {
    opParams.write = 0;
}

template<> int Introspection<ACStbParameters>::setup() {
    registerMember(probe);
    registerMember(localgnd);
    registerMember(from);
    registerMember(to);
    registerMember(step);
    registerMember(mode);
    registerMember(points);
    registerMember(values);
    registerMember(writeop);
    registerMember(write);
    registerNamedMember(opParams.nodeset, "nodeset");
    registerNamedMember(opParams.store, "store");
    
    return 0;
}
instantiateIntrospection(ACStbParameters);


ACStbCore::ACStbCore(
    OutputDescriptorResolver& parentResolver, ACStbParameters& params, OperatingPointCore& opCore, Circuit& circuit, 
    CommonData& commons, 
    KluRealMatrix& dcJacobian, VectorRepository<double>& dcSolution, VectorRepository<double>& dcStates, 
    KluComplexMatrix& acMatrix, Vector<Complex>& acSolution, Vector<Complex>& resultsVector
) : AnalysisCore(parentResolver, circuit, commons), params(params), outfile(nullptr), opCore_(opCore), 
    dcSolution(dcSolution), dcStates(dcStates), dcJacobian(dcJacobian), 
    acMatrix(acMatrix), acSolution(acSolution), resultsVector(resultsVector) {
    
    // Set analysis type for the initial operating point analysis
    auto& elsSystem = opCore_.solver().evalSetup();
    elsSystem.staticAnalysis = true;
    elsSystem.dcAnalysis = false;
    elsSystem.acAnalysis = true;
}

ACStbCore::~ACStbCore() {
    delete outfile;
}

// Converts an OutputDescriptor into an OutputSource. 
// The former can be used to recreate the latter if the set of unknowns changes. 
bool ACStbCore::resolveOutputDescriptors(bool strict, Status& s) {
    // Clear output sources
    outputSources.clear();
    // Resolve output descriptors
    bool ok = true; 
    for (auto it = outputDescriptors.cbegin(); it != outputDescriptors.cend(); ++it) {
        switch (it->type) {
        case OutdGain:
            outputSources.emplace_back(&resultsVector, it->ndx, it->name);
            break;
        case OutdY:
            outputSources.emplace_back(&resultsVector, it->ndx, it->name);
            break;
        case OutdFrequency:
            outputSources.emplace_back(&frequency, it->name);
            break;
        default:
            // Delegate to parent
            ok = parentResolver.resolveOutputDescriptor(*it, outputSources, strict, s);
            break;
        }
        if (!ok) {
            break;
        }
    }
    return ok;
}

// These OutputDescriptors are always added
bool ACStbCore::addCoreOutputDescriptors(Status& s) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    if (!addOutputDescriptor(OutputDescriptor(OutdFrequency, "frequency"))) {
        s.set(Status::Analysis, std::string("Failed to add output descriptor for frequency."));
        return false;
    }
    // Forward/reverse/total open loop gain
    if (!addOutputDescriptor(OutputDescriptor(OutdGain, "wf", to_int(StbResult::Wf)))) {
        s.set(Status::Analysis, std::string("Failed to add output descriptor for forward open-loop gain."));
        return false;
    }
    if (!addOutputDescriptor(OutputDescriptor(OutdGain, "wr", to_int(StbResult::Wr)))) {
        s.set(Status::Analysis, std::string("Failed to add output descriptor for reverse open-loop gain."));
        return false;
    }
    if (!addOutputDescriptor(OutputDescriptor(OutdGain, "w", to_int(StbResult::W)))) {
        s.set(Status::Analysis, std::string("Failed to add output descriptor for open-loop gain."));
        return false;
    }
    if (!addOutputDescriptor(OutputDescriptor(OutdY, "y(1,1)", to_int(StbResult::y11)))) {
        s.set(Status::Analysis, std::string("Failed to add output descriptor for y11."));
        return false;
    }
    if (!addOutputDescriptor(OutputDescriptor(OutdY, "y(1,2)", to_int(StbResult::y12)))) {
        s.set(Status::Analysis, std::string("Failed to add output descriptor for y12."));
        return false;
    }
    if (!addOutputDescriptor(OutputDescriptor(OutdY, "y(2,1)", to_int(StbResult::y21)))) {
        s.set(Status::Analysis, std::string("Failed to add output descriptor for y21."));
        return false;
    }
    if (!addOutputDescriptor(OutputDescriptor(OutdY, "y(2,2)", to_int(StbResult::y22)))) {
        s.set(Status::Analysis, std::string("Failed to add output descriptor for y22."));
        return false;
    }
    return true;
}

// These OutputDescriptors are added if no save directives are given
bool ACStbCore::addDefaultOutputDescriptors(Status& s) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    if (savesCount==0) {
    }
    return true;
}

bool ACStbCore::initializeOutputs(const std::string& name, Status& s) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    // Create output file if not created yet
    if (!outfile) {
        outfile = new OutputRawfile(
            name, outputSources,
            (circuit.simulatorOptions().core().rawfile==SimulatorOptions::rawfileBinary ? OutputRawfile::Flags::Binary : OutputRawfile::Flags::None) |
                OutputRawfile::Flags::Padded | OutputRawfile::Flags::Complex);
        outfile->setTitle(circuit.title());
        outfile->setPlotname("AC Stability Analysis");
    }
    outfile->prologue();

    return true;
}

bool ACStbCore::finalizeOutputs(Status& s) {
    if (outfile) {
        outfile->epilogue();
        delete outfile;
        outfile = nullptr;
    }
    return true;
}

bool ACStbCore::deleteOutputs(Id name, Status& s) {
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

bool ACStbCore::rebuild(Status& s) {
    clearError();
    // AC analysis matrix
    if (!acMatrix.rebuild(circuit.sparsityMap(), circuit.unknownCount())) {
        setError(StbError::MatrixError);
        return false;
    }
    
    // Resistive Jacobian entries remain bound to OP Jacobian, 
    // reactive parts will be bound to imaginary entries of acMatrix
    if (!circuit.bind(nullptr, Component::Real, std::nullopt, &acMatrix, Component::Imaginary, std::nullopt, s)) {
        return false;
    }
    
    return true;
}

// System of equations is 
//   (G(x) + i C(x)) dx = dJ
CoreCoroutine ACStbCore::coroutine(bool continuePrevious) {
    acMatrix.setAccounting(circuit.tables().accounting());
    
    clearError();
    
    auto n = circuit.unknownCount(); 
    // Make sure structures are large enough
    acSolution.resize(n+1);

    // Get probe voltage source
    auto [probeOk, probeSource] = getExcitation(params.probe);
    if (!probeOk) {
        co_yield CoreState::Aborted;
    }

    // Is probe source a voltage source? 
    auto sdev = probeSource->model()->device();
    if (!(sdev->isSource() && sdev->isVoltageSource())) {
        setError(StbError::BadProbe);
        co_yield CoreState::Aborted;
    }

    // Get response scaling factor
    auto probeResponseScalingFactor = probeSource->responseScalingFactor();

    // Extract DUT input (pnode) and output (nnode) node
    auto np = probeSource->terminal(0);
    auto nn = probeSource->terminal(1);
    auto pnodeUnknown = probeSource->terminal(0)->unknownIndex();
    auto nnodeUnknown = probeSource->terminal(1)->unknownIndex();

    // Get probe response unknown
    auto [r1, r2] = probeSource->sourceResponse(circuit);
    auto [e1, e2] = probeSource->sourceExcitation(circuit);
    
    // Get reference ground node
    UnknownIndex refGnd = 0;
    // Valid Id and not an empty string. 
    if (params.localgnd && *params.localgnd.c_str()!=0) {
        auto node = circuit.findNode(params.localgnd);
        if (!node) {
            setError(StbError::BadLocalGnd);
            co_yield CoreState::Aborted;
        }
        refGnd = node->unknownIndex();
    }

    // Make space for results at the given frequency
    resultsVector.resize(to_int(StbResult::COUNT));
    
    // Compute operating point
    errorFreq = 0;
    auto opOk = opCore_.run(continuePrevious);
    if (!opOk) {
        setError(StbError::OperatingPointError);
        co_yield CoreState::Aborted;
    }

    auto& options = circuit.simulatorOptions().core();
    Int debug = options.smsig_debug;

    if (debug>0) {
        Simulator::dbg() << "Starting AC stability analysis.\n";
    }
    
    // Evaluate resistive and reactive Jacobian, bypass is not allowed
    EvalSetup esReactive { 
        // Inputs
        .solution = &dcSolution, 
        .states = &dcStates, 
        
        // Evaluation type reported to the model
        .acAnalysis = true, 
        
        // Evaluation 
        .enableLimiting = false, 
        .evaluateResistiveJacobian = true, 
        .evaluateReactiveJacobian = true, 
    };

    LoadSetup lsReactive { 
        // Outputs
        .loadResistiveJacobian = false, 
        .loadReactiveJacobian = true, 
        // Do not load AC RHS (AC residual)
        // .acResidual = acSolution.data()
    };

    // Copy OP Jacobian to real part of acMatrix, zero out imaginary part
    auto nnz = dcJacobian.nnz();
    auto Jr = dcJacobian.data();
    auto M = acMatrix.data();
    for(decltype(nnz) i=0; i<nnz; i++) {
        M[i] = Jr[i];
    }
    
    // Evaluate Jacobians 
    // Actually we only need to evaluate the reactive Jacobian 
    // because the resistive part was evaluated by OP analysis
    // We do both here in case OpenVAF has bugs with this corner case :)
    if (!circuit.evalAndLoad(commons, &esReactive, nullptr, nullptr)) {
        // Load error
        setError(StbError::EvalAndLoad);
        if (debug>0) {
            Simulator::dbg() << "Error in AC Jacobian evaluation.\n";
        }
        co_yield CoreState::Aborted;
    }

    // Handle Abort, Finish, Stop
    if (esReactive.requests.abort) {
        if (debug>0) {
            Simulator::dbg() << "Abort requested during AC Jacobian evaluation. Exiting.\n";
        }
        co_yield CoreState::Aborted;
    }
    if (esReactive.requests.finish) {
        if (debug>0) {
            Simulator::dbg() << "Finish requested during AC Jacobian evaluation. Exiting.\n";
        }
        co_yield CoreState::Finished;
    }
    if (esReactive.requests.stop) {
        if (debug>0) {
            Simulator::dbg() << "Stop requested during AC Jacobian evaluation. Exiting.\n";
        }
        co_yield CoreState::Stopped;
    }

    // Create sweeper
    ScalarSweep sweeper;
    if (!sweeper.setup(params, errorStatus)) {
        setError(StbError::Sweeper);
        co_yield CoreState::Aborted;
    }
    if (progressReporter) {
        progressReporter->setValueFormat(ProgressReporter::ValueFormat::Scientific, 6);
        progressReporter->setValueDecoration("", "Hz");    
    }
    initProgress(sweeper.count(), 0);
    
    // Frequency sweep
    sweeper.reset();
    bool finished = false;
    frequency = -1.0;
    std::stringstream ss;
    ss << std::scientific << std::setprecision(4);
    bool error = false;
    do {
        // Compute should always succeed
        Value v;
        if (!sweeper.compute(v, errorStatus)) {
            setError(StbError::SweepCompute);
            error = true;
            break;
        }

        // The value, however, must be convertible to real
        if (!v.convertInPlace(Value::Type::Real, errorStatus)) {
            setError(StbError::BadFrequency);
            if (debug>0) {
                Simulator::dbg() << "Frequency value cannot be converted to real.\n";
            }
            error = true;
            break;
        }
        frequency = v.val<Real>();
        double omega = 2*std::numbers::pi*frequency;

        if (debug>0) {
            ss.str(""); ss << frequency;
            Simulator::dbg() << "frequency=" << ss.str() << "\n";
        }

        // Zero out imaginary part, and RHS. 
        // Because the real part is taken from OP Jacobian it includes
        // the shunt resistors. 
        // Load imaginary part and AC residual. 
        acMatrix.zero(Component::Imaginary);
        zero(acSolution);
        lsReactive.reactiveJacobianFactor = omega;
        if (!circuit.evalAndLoad(commons, nullptr, &lsReactive, nullptr)) {
            // Load error
            setError(StbError::EvalAndLoad);
            if (debug>0) {
                Simulator::dbg() << "Error in AC Jacobian load.\n";
            }
            error = true;
            break;
        }
        
        if (debug>=101) {
            Simulator::dbg() << "Linear system matrix\n";
            acMatrix.dump(Simulator::dbg()); 
            Simulator::dbg() << "\n";
        }

        // Check if matrix entries are finite, no need to check RHS 
        // since we loaded it without any computation (i.e. we only used mag and phase)
        if (options.matrixcheck && !acMatrix.isFinite(true, true)) {
            setError(StbError::MatrixError);
            if (debug>0) {
                Simulator::dbg() << "A matrix entry is not finite.\n";
            }
            error = true;
            break;
        }

        // Factor
        bool forceFullFactorization = false;        
        if (acMatrix.isFactored()) {
            // Refactor (if possible)
            if (!acMatrix.refactor()) {
                // Failed, try again by fully factoring
                forceFullFactorization = true;
            } 
        }
        if (forceFullFactorization || !acMatrix.isFactored()) {
            // Full factorization
            if (!acMatrix.factor()) {
                // Failed, give up
                setError(StbError::MatrixError);
                if (debug>0) {
                    Simulator::dbg() << "LU factorization failed.\n";
                }
                error = true;
                break;
            }
        }
        // Check if matrix is singular
        if (options.rcondcheck>0) { 
            double rcond;
            if (!acMatrix.rcond(rcond)) {
                setError(StbError::MatrixError);
                if (debug>0) {
                    Simulator::dbg() << "Condition number estimation failed.\n";
                }
                error = true;
                break;
            }
            if (rcond<options.rcondcheck) {
                if (debug>0) {
                    Simulator::dbg() << "Matrix is close to singular.\n";
                }
                setError(StbError::SingularMatrix);
                error = true;
                break;
            }
        }

        // Step 1 - inject current between positive probe node and localgnd
        // There is not current source for that. We inject it manually into the RHS. 
        zero(acSolution);
        acSolution[pnodeUnknown] += 1;
        acSolution[refGnd]       -= 1;

        // Solve, set bucket to 0.0
        if (!acMatrix.solve(dataWithoutBucket(acSolution, bucketSize))) {
            setError(StbError::MatrixError);
            if (debug>2) {
                Simulator::dbg() << "Failed to solve factored system for injected current.\n";
            }
            error = true;
            break;
        }
        acSolution[0] = 0.0;

        if (options.solutioncheck && !acMatrix.isFinite(dataWithoutBucket(acSolution, bucketSize), true, true)) {
            setError(StbError::SolutionError);
            if (options.smsig_debug) {
                Simulator::dbg() << "A solution entry for injected current is not finite. Solver failed.\n";
            }
            error = true;
            break;
        }

        // Extract A and C, apply probe response scaling factor for voltage source current 
        // because the $mfactor of the voltage source may not be 1
        auto A = (acSolution[r1] - acSolution[r2])*probeResponseScalingFactor;
        auto C = acSolution[pnodeUnknown] - acSolution[refGnd];

        // Step 2 - inject voltage into the feedback loop, no need to apply scaledUnityExcitation()
        // because we know the probe is a voltage source and the factor is 1. 
        zero(acSolution);
        acSolution[e1] += 1;
        acSolution[e2] -= 1;

        // Solve, set bucket to 0.0
        if (!acMatrix.solve(dataWithoutBucket(acSolution, bucketSize))) {
            setError(StbError::MatrixError);
            if (debug>2) {
                Simulator::dbg() << "Failed to solve factored system for injected voltage.\n";
            }
            error = true;
            break;
        }
        acSolution[0] = 0.0;

        if (options.solutioncheck && !acMatrix.isFinite(dataWithoutBucket(acSolution, bucketSize), true, true)) {
            setError(StbError::SolutionError);
            if (options.smsig_debug) {
                Simulator::dbg() << "A solution entry for injected voltage is not finite. Solver failed.\n";
            }
            error = true;
            break;
        }

        // Extract B and D, apply probe respinse scaling factor for voltage source current 
        // because the $mfactor of the voltage source may not be 1
        auto B = (acSolution[r1] - acSolution[r2])*probeResponseScalingFactor;
        auto D = acSolution[pnodeUnknown] - acSolution[refGnd];

        // Compute admittance parameters
        auto ADBC = A*D - B*C;
        resultsVector[to_int(StbResult::y11)] = (1.0+ADBC-A-D)/C;
        resultsVector[to_int(StbResult::y12)] = (-ADBC+D)/C;
        resultsVector[to_int(StbResult::y21)] = (-ADBC+A)/C;
        resultsVector[to_int(StbResult::y22)] = ADBC/C;

        // Compute forward and reverse open-loop gain
        auto den = 1.0+2.0*ADBC-A-D;
        auto Wf = (-ADBC+A)/den;
        auto Wr = (-ADBC+D)/den;
        resultsVector[to_int(StbResult::Wf)] = Wf;
        resultsVector[to_int(StbResult::Wr)] = Wr;
        resultsVector[to_int(StbResult::W)] = Wf + Wr;

        // Dump solution point
        if (params.write && !Simulator::noOutput() && outfile) {
            outfile->addPoint();
        }
        
        finished = sweeper.advance();
        
        setProgress(sweeper.at(), frequency);
    } while (!finished && !error);
    
    if (debug>0) {
        Simulator::dbg() << "AC stability frequency sweep " << (finished ? "completed" : "exited prematurely") << ".\n";
    }

    if (!finished) {
        errorFreq = frequency;
    }

    // No need to bind resistive Jacobian enatries. 
    // OP analysis will still work fine, even in sweep. 
    // We only changed the bindings of the reactive Jacobian entries. 
    
    if (finished) {
        co_yield CoreState::Finished;
    } else {
        co_yield CoreState::Aborted;
    }
}

bool ACStbCore::run(bool continuePrevious) {
    auto c = coroutine(continuePrevious);
    bool ok = true;
    while (!c.done()) {
        if (c.resume()==CoreState::Aborted) {
            ok = false;
            break;
        };
    }
    return ok;
}

bool ACStbCore::formatError(Status& s) const {
    auto nr = UnknownNameResolver(circuit);
    std::stringstream ss;
    ss << std::scientific << std::setprecision(4);
    
    // First, handle AnalysisCore errors
    if (lastError!=Error::OK) {
        AnalysisCore::formatError(s);
        return false;
    }
    
    // Then handle ACStbCore errors
    switch (lastAcError) {
        case StbError::Sweeper:
        case StbError::SweepCompute:
            s.set(errorStatus);
            break;
        case StbError::EvalAndLoad:
            s.set(Status::Analysis, "Jacobian evaluation failed.");
            break;
        case StbError::MatrixError:
            acMatrix.formatError(s, &nr);
            break;
        case StbError::SolutionError:
            acMatrix.formatError(s, &nr);
            s.extend("Solution component is not finite.");
            break;
        case StbError::OperatingPointError:
            opCore_.formatError(s);
            break;
        case StbError::SingularMatrix:
            s.set(Status::Analysis, "Matrix is close to singular.");
            break;
        case StbError::BadFrequency:
            s.set(Status::Analysis, "Frequency value cannot be converted to real.");
            break;
        case StbError::BadProbe:
            s.set(Status::Analysis, "Probe must be a voltage source.");
            break;
        case StbError::BadLocalGnd:
            s.set(Status::Analysis, "Local ground node not found.");
            break;
        default:
            return true;
    }
    if (errorFreq>=0) {
        ss.str(""); ss << errorFreq;
        s.extend(std::string("Leaving frequency sweep at frequency=")+ss.str()+".");
    } else {
        s.extend("Leaving frequency sweep.");
    }
    return false;
}

void ACStbCore::dump(std::ostream& os) const {
    AnalysisCore::dump(os);
    os << "  Results\n";
    auto n = circuit.unknownCount();
    for(decltype(n) i=1; i<=n; i++) {
        auto rn = circuit.reprNode(i);
        auto c = resultsVector.data()[i];
        os << "    " << i << " : " << c.real();
        if (c.imag()>=0) {
            os << "+";
        }
        os << c.imag();
        os << "i\n";
    }
}

}
