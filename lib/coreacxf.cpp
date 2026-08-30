#include <iomanip>
#include <cmath>
#include <filesystem>
#include "coreacxf.h"
#include "simulator.h"
#include "coresweep.h"
#include "context.h"
#include "common.h"
#include <numbers>

namespace NAMESPACE {

// Default parameters
ACXFParameters::ACXFParameters() {
    opParams.write = 0;
}

template<> int Introspection<ACXFParameters>::setup() {
    registerMember(out);
    registerMember(from);
    registerMember(to);
    registerMember(step);
    registerMember(mode);
    registerMember(points);
    registerMember(values);
    registerMember(write);
    registerNamedMember(opParams.write, "writeop");
    registerNamedMember(opParams.nodeset, "nodeset");
    registerNamedMember(opParams.store, "store");

    return 0;
}
instantiateIntrospection(ACXFParameters);


ACXFCore::ACXFCore(
    OutputDescriptorResolver& parentResolver, ACXFParameters& params, OperatingPointCore& opCore, std::unordered_map<Id,size_t>& sourceIndex, 
    Circuit& circuit, CommonData& commons, 
    KluRealMatrix& dcJacobian, VectorRepository<double>& dcSolution, VectorRepository<double>& dcStates, 
    KluComplexMatrix& acMatrix, Vector<Complex>& acSolution, 
    std::vector<Instance*>& sources, Vector<Complex>& tf, Vector<Complex>& yin, Vector<Complex>& zin, 
    DelayLines& delayLines, DelayMatrixBindings<Complex*>& delayBindings
) : AnalysisCore(parentResolver, circuit, commons), params(params), outfile(nullptr), opCore_(opCore), sourceIndex(sourceIndex), 
    dcSolution(dcSolution), dcStates(dcStates), dcJacobian(dcJacobian), 
    acMatrix(acMatrix), acSolution(acSolution), sources(sources), tf(tf), yin(yin), zin(zin), 
    delayLines_(delayLines), delayBindings_(delayBindings) {
    
    // Set analysis type for the initial operating point analysis
    auto& elsSystem = opCore_.solver().evalSetup();
    elsSystem.staticAnalysis = true;
    elsSystem.dcAnalysis = false;
    elsSystem.acAnalysis = true;
}

ACXFCore::~ACXFCore() {
    delete outfile;
}

bool ACXFCore::resolveOutputDescriptors(bool strict, ErrorConsumer& errors) {
    // Clear output sources
    outputSources.clear();
    // Clear source instance pointers, initialize to nullptrs
    sources.clear();
    sources.resize(sourceIndex.size(), nullptr);
    // Resolve output descriptors
    bool ok = true; 
    for (auto it = outputDescriptors.cbegin(); it != outputDescriptors.cend(); ++it) {
        Id name;
        size_t ndx;
        Instance *inst;
        switch (it->type) {
            case OutdTf:
            case OutdZ:    
            case OutdY:
                // Get instance name and index
                name = it->idNdx.id;
                ndx = it->idNdx.ndx;
                // Find instance
                inst = circuit.findInstance(name);
                sources[ndx] = inst;
                if (strict) {
                    if (!inst) {
                        errors.push(AcxfSourceNotFound{name});
                        ok = false;
                        break;
                    }
                }
                // Instance found, but is not a source... this is always an error
                if (inst && !inst->model()->device()->isSource()) {
                    errors.push(AcxfNotSource{name});
                    ok = false;
                    break;
                }
                // No instance and we reached this point, create constant source
                if (!inst) {
                    outputSources.emplace_back(it->name);
                }
                break;
        }
        if (!ok) {
            break;
        }
        switch (it->type) {
        case OutdFrequency:
            outputSources.emplace_back(&frequency, it->name);
            break;
        case OutdTf:
            outputSources.emplace_back(&tf, ndx, it->name);
            break;
        case OutdZ:
            outputSources.emplace_back(&zin, ndx, it->name);
            break;
        case OutdY:
            outputSources.emplace_back(&yin, ndx, it->name);
            break; 
        default:
            // Delegate to parent
            ok = parentResolver.resolveOutputDescriptor(*it, outputSources, strict, errors);
        }
        if (!ok) {
            break;
        }
    }
    return ok;
}

bool ACXFCore::addCoreOutputDescriptors(ErrorConsumer& errors) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    if (!addOutputDescriptor(OutputDescriptor(OutdFrequency, "frequency"))) {
        errors.push(CoreAddOutputDescriptor{"frequency"});
        return false;
    }
    return true;
}

bool ACXFCore::addDefaultOutputDescriptors(ErrorConsumer& errors) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    if (savesCount==0) {
        return addAllTfZin(PTSave("default", Id(), Id()), sourceIndex, errors);
    }
    return true;
}

bool ACXFCore::initializeOutputs(const std::string& name, ErrorConsumer& errors) {
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
        outfile->setPlotname("AC Small Signal Transfer Function Analysis");
    }
    outfile->prologue();

    return true;
}

bool ACXFCore::finalizeOutputs(ErrorConsumer& errors) {
    if (outfile) {
        outfile->epilogue();
        delete outfile;
        outfile = nullptr;
    }
    return true;
}

bool ACXFCore::deleteOutputs(Id name, ErrorConsumer& errors) {
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
    
bool ACXFCore::rebuild(ErrorConsumer& errors) {
    // AC analysis matrix
    if (!acMatrix.rebuild(circuit.sparsityMap(), circuit.unknownCount(), errors)) {
        return false;
    }

    // Delay lines: bind this core's complex bindings into acMatrix.
    // The shared DelayLines object is already sized here - it is scaled by
    // OperatingPointCore::rebuild(), which SmallSignal::rebuildCores() always
    // runs before this core's rebuild(). Do not call delayLines_.scale() again.
    if (!delayLines_.bindToMatrix(acMatrix, std::nullopt, delayBindings_, errors)) {
        errors.push(AcxfDelayBindFailed{});
        return false;
    }

    // Resistive Jacobian entries remain bound to OP Jacobian,
    // reactive parts will be bound to imaginary entries of acMatrix
    if (!circuit.bind(nullptr, Component::Real, std::nullopt, &acMatrix, Component::Imaginary, std::nullopt, nullptr, errors)) {
        errors.push(AcxfBindFailed{});
        return false;
    }
    
    return true;
}

// System of equations is 
//   (G(x) + i C(x)) dx = dJ
CoreCoroutine ACXFCore::coroutine(bool continuePrevious, ErrorConsumer& errors) {
    acMatrix.setAccounting(circuit.tables().accounting());
    

    auto n = circuit.unknownCount(); 
    // Make sure structures are large enough
    acSolution.resize(n+1);
    tf.resize(sources.size());
    yin.resize(sources.size());
    zin.resize(sources.size());

    // Get output unknowns
    auto [ok, up, un] = getDiffNodePair(params.out, errors);
    if (!ok) {
        co_yield CoreState::Aborted;
    }

    // Compute operating point
    auto opOk = opCore_.run(continuePrevious, errors);
    if (!opOk) {
        errors.push(AcxfOperatingPointFailed{});
        co_yield CoreState::Aborted;
    }

    auto& options = circuit.simulatorOptions().core();
    Int debug = options.smsig_debug;

    if (debug>0) {
        Simulator::dbg() << "Starting AC small-signal transfer function analysis.\n";
    }
    
    // Evaluate resistive and reactive Jacobian, bypass is not allowed
    EvalSetup esReactive { 
        // Inputs, can be set here (we do not rotate)
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
    };

    // Copy OP Jacobian to real part of acMatrix, zero out imaginary part. 
    // Because the real part is taken from OP Jacobian it includes
    // the shunt resistors. 
    auto nnz = dcJacobian.nnz();
    auto Jr = dcJacobian.data();
    auto M = acMatrix.data();
    for(decltype(nnz) i=0; i<nnz; i++) {
        M[i] = Jr[i];
    }
    
    // Evaluate Jacobians 
    // Actually we only need to evaluate the reactive Jacobian 
    // because the resistive part was evaluated by OP analysis
    // We do both here in case OpenVAF-Reloaded has bugs with this corner case :)
    if (!circuit.evalAndLoad(commons, &esReactive, nullptr, nullptr, errors)) {
        // Load error
        errors.push(AcxfEvalAndLoadFailed{});
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

    // Create sweeper, put it in unique ptr to free it when method returns
    ScalarSweep sweeper;
    if (!sweeper.setup(params, errors)) {
        errors.push(AcxfSweepSetupFailed{});
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
        if (!sweeper.compute(v, errors)) {
            errors.push(AcxfSweepComputeFailed{});
            error = true;
            break;
        }

        // The value, however, must be convertible to real
        if (!v.convertInPlace(Value::Type::Real)) {
            errors.push(AcxfBadFrequency{});
            error = true;
            break;
        }
        frequency = v.val<Real>();
        double omega = 2*std::numbers::pi*frequency;

        if (debug>0) {
            ss.str(""); ss << frequency;
            Simulator::dbg() << "frequency=" << ss.str() << "\n";
        }

        // Load AC matrix, we must update the imaginary part only
        acMatrix.zero(Component::Imaginary);
        lsReactive.reactiveJacobianFactor = omega;
        if (!circuit.evalAndLoad(commons, nullptr, &lsReactive, nullptr, errors)) {
            // Load error
            errors.push(AcxfEvalAndLoadFailed{});
            if (debug>0) {
                Simulator::dbg() << "Error in AC Jacobian load.\n";
            }
            error = true;
            break;
        }

        // Load delay line contributions
        auto nDelay = circuit.delayHistoryCount();
        if (nDelay>0) {
            for(decltype(nDelay) i=0; i<nDelay; i++) {
                // Get input and output unknowns
                auto inU = delayLines_.inputUnknown(i);
                auto outU = delayLines_.outputUnknown(i);
                // Equation -out + exp(-j w delay) in = 0
                // Get Jacobian Pointers
                auto [outIn, outOut] = delayBindings_[i];
                // Load Jacobian, set values, not add because we are the sole contributor to this equation
                // Also op Jacobian left a real value in outIn which should be overwritten
                *outIn = std::exp(Complex(0, - omega * delayLines_.delay(i)));
                // outOut is kept as loaded by op 
            }
        }

        if (debug>=101) {
            Simulator::dbg() << "Linear system matrix\n";
            acMatrix.dump(Simulator::dbg()); 
            Simulator::dbg() << "\n";
        }
        
        // Check if matrix entries are finite, no need to check RHS 
        // since we loaded it without any computation (i.e. we only used mag and phase)
        if (options.matrixcheck && !acMatrix.isFinite(true, true, errors)) {
            auto nr = UnknownNameResolver(circuit);
            errors.push(AcxfMatrixError{});
            if (debug>2) {
                Simulator::dbg() << "A matrix entry is not finite.\n";
            }
            error = true;
            break;
        }

        // Factor
        bool forceFullFactorization = false;        
        if (acMatrix.isFactored()) {
            // Refactor (if possible). A refactor failure is not fatal here.
            if (!acMatrix.refactor(errors)) {
                // Failed, try again by fully factoring
                forceFullFactorization = true;
            } 
        }
        if (forceFullFactorization || !acMatrix.isFactored()) {
            // Full factorization
            if (!acMatrix.factor(errors)) {
                // Failed, give up
                errors.push(AcxfMatrixError{});
                if (debug>0) {
                    Simulator::dbg() << "LU factorization failed.\n";
                }
                error = true;
                break;
            }
            // Full factorization recovered, drop the non-fatal refactor error
            if (forceFullFactorization) {
                errors.clear();
            }
        }
        // Check if matrix is singular
        if (options.rcondcheck>0) { 
            double rcond;
            if (!acMatrix.rcond(rcond, errors)) {
                errors.push(AcxfMatrixError{});
                if (debug>0) {
                    Simulator::dbg() << "Condition number estimation failed.\n";
                }
                error = true;
                break;
            }
            if (rcond<options.rcondcheck) {
                errors.push(AcxfMatrixError{});
                if (debug>0) {
                    Simulator::dbg() << "Matrix is close to singular.\n";
                }
                error = true;
                break;
            }
        }

        // Go through all sources listed in sources
        auto nSrc = sources.size();
        for(decltype(nSrc) i=0; i<nSrc; i++) {
            // Prepare RHS
            zero(acSolution); 

            // Get instance
            auto inst = sources[i];
            // No instance, continue to next
            if (!inst) {
                continue;
            }

            if (debug>1) {
                Simulator::dbg() << "Computing response to '" << std::string(inst->name()) << "'.\n";
            }

            // Get excitation equations and response unknowns
            auto [e1, e2] = inst->sourceExcitation(circuit);
            auto [r1, r2] = inst->sourceResponse(circuit);
            
            // Set RHS
            acSolution[e1] += inst->scaledUnityExcitation();
            acSolution[e2] -= inst->scaledUnityExcitation();

            // Set RHS bucket
            acSolution[0] = 0.0;

            if (debug>=100) {
                Simulator::dbg() << "Linear system for instance " << inst->name() << "\n";
                acMatrix.dump(Simulator::dbg(), dataWithoutBucket(acSolution, bucketSize)); 
                Simulator::dbg() << "\n";
            }

            // Solve, set bucket to 0.0
            if (!acMatrix.solve(dataWithoutBucket(acSolution, bucketSize), errors)) {
                errors.push(AcxfMatrixError{});
                if (debug>2) {
                    Simulator::dbg() << "Failed to solve factored system.\n";
                }
                error = true;
                break;
            }
            acSolution[0] = 0.0;

            if (options.solutioncheck && !acMatrix.isFinite(dataWithoutBucket(acSolution, bucketSize), true, true, errors)) {
                errors.push(AcxfSolutionNotFinite{});
                if (options.smsig_debug) {
                    Simulator::dbg() << "A solution entry is not finite. Solver failed.\n";
                }
                error = true;
                break;
            }

            // Collect results
            tf[i] = acSolution[up] - acSolution[un];

            // Compute Yin and Zin
            if (inst->model()->device()->isVoltageSource()) {
                // Voltage source excitation
                // We get as response the total current flowing into the + node 
                // of all parallel instances combined. Need to negate the value 
                // to get the current flowing into the surrounding circuit. 
                // To get the actual admittance we need to divide by the total 
                // excitation introduced by all of the source's parallel instances. 
                yin[i] = -(acSolution[r1] - acSolution[r2])*inst->responseScalingFactor()/inst->scaledUnityExcitation();
                if (yin[i]!=0.0) {
                    zin[i] = 1.0/yin[i];
                } else {
                    // Infinity
                    zin[i] = 1e20;
                }
            } else {
                // Current source excitation
                // We get as response the voltage between the node where the current is pulled 
                // and the node where the current is pushed. This is the negative of the voltage 
                // we are interested in to compute Zin. 
                // To get the actual impedance we need to divide by the total 
                // excitation introduced by all of the source's parallel instances. 
                zin[i] = -(acSolution[r1] - acSolution[r2])*inst->responseScalingFactor()/inst->scaledUnityExcitation();
                if (zin[i]!=0.0) {
                    yin[i] = 1.0/zin[i];
                } else {
                    // Infinity
                    yin[i] = 1e20;
                }
            }
        }
        
        if (error) {
            break;
        }

        // Dump solution
        if (params.write && !Simulator::noOutput() && outfile) {
            outfile->addPoint();
        }

        finished = sweeper.advance();

        setProgress(sweeper.at(), frequency);
    } while (!finished && !error);

    if (debug>0) {
        Simulator::dbg() << "AC transfer function frequency sweep " << (finished ? "completed" : "exited prematurely") << ".\n";
    }

    // No need to bind resistive Jacobian enatries.
    // OP analysis will still work fine, even in sweep.
    // We only changed the bindings of the reactive Jacobian entries.

    if (finished) {
        co_yield CoreState::Finished;
    } else {
        errors.push(AcxfSweepAborted{frequency});
        co_yield CoreState::Aborted;
    }
}

bool ACXFCore::run(bool continuePrevious, ErrorConsumer& errors) {
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

void ACXFCore::dump(std::ostream& os) const {
    AnalysisCore::dump(os);
    os << "  Results\n";
    auto nSrc = sources.size();
    for(decltype(nSrc) i=0; i<nSrc; i++) {
        auto inst = sources[i];
        if (!inst) {
            continue;
        }
        os << "    tf(" << inst->name() << ") " << tf[i] << "\n";
        os << "    zin(" << inst->name() << ") " << zin[i] << "\n";
        os << "    yin(" << inst->name() << ") " << yin[i] << "\n";
    }
}

}
