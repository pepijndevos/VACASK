#include <iomanip>
#include <cmath>
#include <filesystem>
#include "coreacsp.h"
#include "simulator.h"
#include "answeep.h"
#include "context.h"
#include "common.h"
#include <numbers>

namespace NAMESPACE {

// Default parameters
ACSPParameters::ACSPParameters() {
    opParams.write = 0;
}

template<> int Introspection<ACSPParameters>::setup() {
    registerMember(ports);
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
instantiateIntrospection(ACSPParameters);


ACSPCore::ACSPCore(
    OutputDescriptorResolver& parentResolver, ACSPParameters& params, OperatingPointCore& opCore, Circuit& circuit, 
    CommonData& commons, 
    KluRealMatrix& dcJacobian, VectorRepository<double>& dcSolution, VectorRepository<double>& dcStates, 
    KluComplexMatrix& acMatrix, Vector<Complex>& acSolution, 
    DenseMatrix<Complex>& stMatrix
) : AnalysisCore(parentResolver, circuit, commons), params(params), outfile(nullptr), opCore_(opCore), 
    dcSolution(dcSolution), dcStates(dcStates), dcJacobian(dcJacobian), 
    acMatrix(acMatrix), acSolution(acSolution),
    stMatrix(stMatrix) {
    
    // Set analysis type for the initial operating point analysis
    auto& elsSystem = opCore_.solver().evalSetup();
    elsSystem.staticAnalysis = true;
    elsSystem.dcAnalysis = false;
    elsSystem.acAnalysis = true;
}

ACSPCore::~ACSPCore() {
    delete outfile;
}

// Converts an OutputDescriptor into an OutputSource. 
// The former can be used to recreate the latter if the set of unknowns changes. 
bool ACSPCore::resolveOutputDescriptors(bool strict, Status& s) {
    // Clear output sources
    outputSources.clear();
    // Resolve output descriptors
    bool ok = true; 
    for (auto it = outputDescriptors.cbegin(); it != outputDescriptors.cend(); ++it) {
        switch (it->type) {
        case OutdSmat:
            // stMatrix is the transpose of S
            if (it->ndxNdx.ndx1<stMatrix.nRows() && it->ndxNdx.ndx2<stMatrix.nRows()) {
                // Matrix large enough
                outputSources.emplace_back(&stMatrix.data(), stMatrix.indexOf(it->ndxNdx.ndx2, it->ndxNdx.ndx1), it->name);
            } else if (strict) {
                // Outside of matrix, strict mode, error
                setError(SPError::MatrixEntryNotFound);
                ok = false;
            } else {
                // Outside of matrix, default source
                outputSources.emplace_back(it->name);
            }
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
bool ACSPCore::addCoreOutputDescriptors(Status& s) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    
    // Add output descriptors for S matrix entries
    auto portCount = params.ports.size() / 2;
    for(decltype(portCount) i=0; i<portCount; i++) {
        for(decltype(portCount) j=0; j<portCount; j++) {
            auto descName = std::string("s(")+std::to_string(i+1)+","+std::to_string(j+1)+")";
            if (!addOutputDescriptor(OutputDescriptor(OutdSmat, descName, i, j))) {
                s.set(Status::Analysis, std::string("Failed to add output descriptor for "+std::string(descName)+"."));
                return false;
            }
        }
    }
    
    if (!addOutputDescriptor(OutputDescriptor(OutdFrequency, "frequency"))) {
        s.set(Status::Analysis, std::string("Failed to add output descriptor for frequency."));
        return false;
    }
    return true;
}

// These OutputDescriptors are added if no save directives are given
bool ACSPCore::addDefaultOutputDescriptors(Status& s) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    if (savesCount==0) {
    }
    return true;
}

bool ACSPCore::initializeOutputs(const std::string& name, Status& s) {
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
        outfile->setPlotname("AC S-parameter Analysis");
    }
    outfile->prologue();

    return true;
}

bool ACSPCore::finalizeOutputs(Status& s) {
    if (outfile) {
        outfile->epilogue();
        delete outfile;
        outfile = nullptr;
    }
    return true;
}

bool ACSPCore::deleteOutputs(Id name, Status& s) {
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

bool ACSPCore::rebuild(Status& s) {
    clearError();

    // Collect ports, check them, assign ports to matrix rows/columns
    sourceVector.clear();
    resistorVector.clear();
    terminalsVector.clear();
    z0.clear();

    // Collect source-resistor pairs, check device type
    auto portCount = params.ports.size() / 2;
    for(decltype(portCount) i=0; i<portCount; i++) {
        // Get source and resistor
        auto& srcName = params.ports.val<StringVector>()[2*i];
        auto srcInst = circuit.findInstance(srcName);
        if (!srcInst) {
            s.set(Status::NotFound, "Port source '"+std::string(srcName)+"' not found");
            return false;
        }
        auto resName = params.ports.val<StringVector>()[2*i+1];
        auto resInst = circuit.findInstance(resName);
        if (!resInst) {
            s.set(Status::NotFound, "Port resistor '"+std::string(resName)+"' not found");
            return false;
        }
        // Check source type. 
        if (!srcInst->model()->device()->isSource() || !srcInst->model()->device()->isVoltageSource()) {
            s.set(Status::NotFound, "Port source '"+std::string(srcName)+"' must be a voltage source.");
            return false;
        }

        // Check resistor type (must have r, noisy, and $mfactor parameters), 
        // must not be a source, must have 2 terminals. 
        auto [rIndex, rOK] = resInst->parameterIndex("r");
        auto [mIndex, mOK] = resInst->parameterIndex("$mfactor");
        auto [noisyIndex, noisyOK] = resInst->parameterIndex("noisy");
        if (!(rOK && mOK && noisyOK)) { 
            s.set(Status::NotFound, "Port resistor '"+std::string(resName)+"' must have 'r', '$mfactor', and 'noisy' parameters.");
            return false;
        }
        if (!(!resInst->model()->device()->isSource() && resInst->terminalCount()==2)) {
            s.set(Status::NotFound, "Port resistor '"+std::string(resName)+"' must not be a source and must have 2 terminals.");
            return false;
        }
        // Extract parameter values
        Value vr, vm, vn;
        if (!resInst->getParameter(rIndex, vr, Status::ignore)) {
            s.set(Status::NotFound, "Failed to read resistor parameter 'r' for '"+std::string(resName)+"'.");
            return false;
        }
        if (!resInst->getParameter(mIndex, vm, Status::ignore)) {
            s.set(Status::NotFound, "Failed to read resistor parameter '$mfactor' for '"+std::string(resName)+"'.");
            return false;
        }
        if (!resInst->getParameter(noisyIndex, vn, Status::ignore)) {
            s.set(Status::NotFound, "Failed to read resistor parameters 'noisy' for '"+std::string(resName)+"'.");
            return false;
        }
        if (!vr.convertInPlace(Value::Type::Real)) {
            s.set(Status::NotFound, "Resistor parameter 'r' of '"+std::string(resName)+"' is of wrong type.");
            return false;
        }
        if (!vm.convertInPlace(Value::Type::Real)) {
            s.set(Status::NotFound, "Resistor parameter '$mfactor' of '"+std::string(resName)+"' is of wrong type.");
            return false;
        }
        if (!vn.convertInPlace(Value::Type::Int)) {
            s.set(Status::NotFound, "Resistor parameter '$noisy' of '"+std::string(resName)+"' is of wrong type.");
            return false;
        }
        auto r = vr.val<Real>();
        auto m = vm.val<Real>();
        // Extract port impedance
        r = r/m;
        // Check if positive node of the source connects to one of resistors's nodes
        // The other resistor node is the positive port terminal. 
        // The other source node is the negative port terminal. 
        auto s1 = srcInst->terminal(0);
        auto s2 = srcInst->terminal(1);
        auto r1 = resInst->terminal(0);
        auto r2 = resInst->terminal(1);
        Node* portp;
        Node* portn;
        if (s1==r1) {
            // Source + connected to resistor node 1
            portp = r2;
        } else if (s1==r2) {
            // Source + connected to resistor node 2
            portp = r1;
        } else {
            s.set(Status::NotFound, "Port defined by '"+std::string(srcName)+"' and '"+std::string(resName)+"' has incorrect topology.");
            s.extend("Positive source node must be connected to the resistor.");
            return false;
        }
        portn = s2;
        sourceVector.push_back(srcInst);
        resistorVector.push_back(resInst);
        terminalsVector.push_back(std::tuple(portp->unknownIndex(), portn->unknownIndex()));
        z0.push_back(r);
    }

    // Make space
    stMatrix.resize(portCount, portCount, DenseMatrix<Complex>::Major::Column);
    atMatrix.resize(portCount, portCount, DenseMatrix<Complex>::Major::Column);
    rowPerm_.resize(portCount);

    // AC analysis matrix
    if (!acMatrix.rebuild(circuit.sparsityMap(), circuit.unknownCount())) {
        setError(SPError::MatrixError);
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
CoreCoroutine ACSPCore::coroutine(bool continuePrevious) {
    acMatrix.setAccounting(circuit.tables().accounting());
    
    clearError();
    
    auto n = circuit.unknownCount(); 
    // Make sure structures are large enough
    acSolution.resize(n+1);

    // Get port count
    auto portCount = z0.size();
    
    // Compute operating point
    errorFreq = 0;
    auto opOk = opCore_.run(continuePrevious);
    if (!opOk) {
        setError(SPError::OperatingPointError);
        co_yield CoreState::Aborted;
    }

    auto& options = circuit.simulatorOptions().core();
    Int debug = options.smsig_debug;

    if (debug>0) {
        Simulator::dbg() << "Starting AC s-parameter analysis.\n";
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
        setError(SPError::EvalAndLoad);
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
        setError(SPError::Sweeper);
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
            setError(SPError::SweepCompute);
            error = true;
            break;
        }

        // The value, however, must be convertible to real
        if (!v.convertInPlace(Value::Type::Real, errorStatus)) {
            setError(SPError::BadFrequency);
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
            setError(SPError::EvalAndLoad);
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
            setError(SPError::MatrixError);
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
                setError(SPError::MatrixError);
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
                setError(SPError::MatrixError);
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
                setError(SPError::SingularMatrix);
                error = true;
                break;
            }
        }

        // Loop through all ports
        for(decltype(portCount) i=0; i<portCount; i++) {
            // Step 1 - inject current between positive probe node and localgnd
            // We use scalenUnityExcitation() because someday we might allow current sources is probes. 
            zero(acSolution);
            auto [ep, en] = sourceVector[i]->sourceExcitation(circuit);
            acSolution[ep] += sourceVector[i]->scaledUnityExcitation();
            acSolution[en] -= sourceVector[i]->scaledUnityExcitation();
            
            // Solve, set bucket to 0.0
            if (!acMatrix.solve(dataWithoutBucket(acSolution, bucketSize))) {
                setError(SPError::MatrixError);
                if (debug>2) {
                    Simulator::dbg() << "Failed to solve factored system for injected current.\n";
                }
                error = true;
                break;
            }
            acSolution[0] = 0.0;

            if (options.solutioncheck && !acMatrix.isFinite(dataWithoutBucket(acSolution, bucketSize), true, true)) {
                setError(SPError::SolutionError);
                if (options.smsig_debug) {
                    Simulator::dbg() << "A solution entry for excitation at port "+std::to_string(i+1)+" is not finite. Solver failed.\n";
                }
                error = true;
                break;
            }
            
            // Loop through ports
            for(decltype(portCount) j=0; j<portCount; j++) {
                // Get j-th port current and voltage (at interface plane), scale voltage source current with $mfactor
                auto [pi, ni] = terminalsVector[j];
                auto vp = acSolution[pi] - acSolution[ni];
                auto [rp, rn] = sourceVector[j]->sourceResponse(circuit);
                // Voltage source current is positive if it flows into the + terminal, 
                // negate the result to get the port current. 
                auto ip = -(acSolution[rp] - acSolution[rn])*sourceVector[j]->responseScalingFactor();
                
                // Compute incident (a) and reflected (b) wave
                auto zp = z0[j];
                auto a = (vp+zp*ip)/(2*sqrt(zp));
                auto b = (vp-zp*ip)/(2*sqrt(zp));
                
                // Store in matrices in i-th row, j-th column (transposed A and B)
                // B transposed is in sMatrix (where the solution will be in the end)
                // A transposed is in aMatrix
                atMatrix.at(i, j) = a;
                stMatrix.at(i, j) = b;
            } // Response loop ends here (j)
        } // Excitation loop ends here (i)

        // The following must hold
        //   B = S A 
        // where A and B contain incident and reflected waves. 
        // Rows correspond to ports where we observe waves and 
        // columns correspond to excitations (always exciting just one port). 
        // atMatrix and stMatrix are the transposes of A and B. 
        //
        // Solve for s-parameters (transposed)
        //    T  T    T
        //   A  S  = B
        // 
        // Transposed S matrix can be found in stMatrix.
        VectorView<int> rowPermView(rowPerm_);
        if (!atMatrix.factorAndLuSolve(stMatrix, &rowPermView)) {
            if (debug>0) {
                Simulator::dbg() << "S matrix is singular.\n";
            }
            setError(SPError::SingularS);
            error = true;
            break;
        }

        // TODO: compute Y matrix

        // Dump solution point
        if (params.write && !Simulator::noOutput() && outfile) {
            outfile->addPoint();
        }
        
        finished = sweeper.advance();
        
        setProgress(sweeper.at(), frequency);
    } while (!finished && !error);
    
    if (debug>0) {
        Simulator::dbg() << "AC s-parameter frequency sweep " << (finished ? "completed" : "exited prematurely") << ".\n";
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

bool ACSPCore::run(bool continuePrevious) {
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

bool ACSPCore::formatError(Status& s) const {
    auto nr = UnknownNameResolver(circuit);
    std::stringstream ss;
    ss << std::scientific << std::setprecision(4);
    
    // First, handle AnalysisCore errors
    if (lastError!=Error::OK) {
        AnalysisCore::formatError(s);
        return false;
    }
    
    // Then handle ACSPCore errors
    switch (lastAcError) {
        case SPError::Sweeper:
        case SPError::SweepCompute:
            s.set(errorStatus);
            break;
        case SPError::EvalAndLoad:
            s.set(Status::Analysis, "Jacobian evaluation failed.");
            break;
        case SPError::MatrixError:
            acMatrix.formatError(s, &nr);
            break;
        case SPError::SolutionError:
            acMatrix.formatError(s, &nr);
            s.extend("Solution component is not finite.");
            break;
        case SPError::OperatingPointError:
            opCore_.formatError(s);
            break;
        case SPError::SingularMatrix:
            s.set(Status::Analysis, "Matrix is close to singular.");
            break;
        case SPError::BadFrequency:
            s.set(Status::Analysis, "Frequency value cannot be converted to real.");
            break;
        case SPError::SingularS:
            s.set(Status::Analysis, "S-parameter matrix is singular.");
            break;
        case SPError::MatrixEntryNotFound:
            s.set(Status::Analysis, "Matrix entry not found.");
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

void ACSPCore::dump(std::ostream& os) const {
    AnalysisCore::dump(os);
    os << "  Results\n";
    os << "  Transposed S matrix\n";
    stMatrix.dump(os);
}

}
