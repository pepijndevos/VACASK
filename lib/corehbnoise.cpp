#include <numbers>
#include <complex>
#include <filesystem>
#include "corehbnoise.h"
#include "corehbac.h"
#include "spurs.h"
#include "simulator.h"
#include "coresweep.h"
#include "common.h"
#include "densematrix.h"

namespace NAMESPACE {

// Default parameters
HBNoiseParameters::HBNoiseParameters() {
    hbParams.write = 0;
}

template<> int Introspection<HBNoiseParameters>::setup() {
    registerMember(out);
    registerMember(in);
    registerMember(from);
    registerMember(to);
    registerMember(step);
    registerMember(mode);
    registerMember(points);
    registerMember(values);
    registerMember(outspur);
    registerMember(inspur);
    registerMember(maxharm);
    registerMember(maxfreq);
    registerMember(write);
    registerMember(solver);
    registerNamedMember(hbParams.write, "writehb");
    registerNamedMember(hbParams.freq, "freq");
    registerNamedMember(hbParams.nharm, "nharm");
    registerNamedMember(hbParams.immax, "immax");
    registerNamedMember(hbParams.truncate, "truncate");
    registerNamedMember(hbParams.samplefac, "samplefac");
    registerNamedMember(hbParams.tstart, "tstart");
    registerNamedMember(hbParams.nper, "nper");
    registerNamedMember(hbParams.sample, "sample");
    registerNamedMember(hbParams.shift, "shift");
    registerNamedMember(hbParams.nodeset, "nodeset");
    registerNamedMember(hbParams.store, "store");
    registerNamedMember(hbParams.solve, "hbsolve");
    registerNamedMember(hbParams.solver, "hbsolver");

    return 0;
}
instantiateIntrospection(HBNoiseParameters);

HBNoiseCore::HBNoiseCore(
    OutputDescriptorResolver& parentResolver, HBNoiseParameters& params, HBCore& hbCore,
    std::unordered_map<std::pair<Id, Id>, size_t>& contributionOffset,
    Circuit& circuit, CommonData& commons,
    CSCBlockSparseComplexMatrix& jacSpec,
    VectorRepository<Complex>& hbSolution,
    CSCBlockSparseComplexMatrix& acMatrix, Vector<Complex>& acSolution,
    Vector<double>& results, double& powerGain, double& outputNoise,
    DelayLines& delayLines, DelayMatrixBindings<DenseMatrixView<Complex>>& delayBindings
) : AnalysisCore(parentResolver, circuit, commons),
    hbCore_(hbCore),
    outfile(nullptr),
    hbSolution(hbSolution),
    jacSpec(jacSpec),
    acMatrix(acMatrix),
    acSolution(acSolution),
    contributionOffset(contributionOffset),
    results(results), powerGain(powerGain), outputNoise(outputNoise),
    params(params),
    delayLines_(delayLines), delayBindings_(delayBindings),
    frequency(0.0),
    resolver_(circuit),
    cxSolver_(nullptr) {
}

HBNoiseCore::~HBNoiseCore() {
    delete outfile;
}

bool HBNoiseCore::resolveOutputDescriptors(bool strict, ErrorConsumer& errors) {
    // Clear output sources
    outputSources.clear();
    // Clear contribution offsets
    contributionOffset.clear();
    // Clear results
    results.clear();
    // Resolve output descriptors
    bool ok = true;
    for (auto it = outputDescriptors.cbegin(); it != outputDescriptors.cend(); ++it) {
        Id name;
        Id contrib;
        ParameterIndex contribIndex;
        bool found;
        Instance *inst;
        switch (it->type) {
            case OutdNoiseContribInst:
                name = it->id;
                // Find instance
                inst = circuit.findInstance(name);
                if (strict) {
                    if (!inst) {
                        errors.push(HbNoiseInstanceNotFound{name});
                        ok = false;
                        break;
                    }
                }
                if (inst) {
                    // Add to contribution offsets
                    auto [insIt, inserted] = contributionOffset.insert({{name, Id()}, contributionOffset.size()});
                    outputSources.emplace_back(&results, insIt->second, it->name);
                } else {
                    // Instance not found, constant source
                    outputSources.emplace_back(it->name);
                }
                break;
            case OutdNoiseContribInstPartial:
                name = it->idId.id1;
                contrib = it->idId.id2;
                // Find instance
                inst = circuit.findInstance(name);
                if (strict && !inst) {
                    errors.push(HbNoiseInstanceNotFound{name});
                    ok = false;
                    break;
                }
                // Find contrib
                if (inst) {
                    std::tie(contribIndex, found) = inst->uniqueNoiseSourceIndex(contrib);
                    if (strict && !found) {
                        errors.push(HbNoiseContribNotFound{name, contrib});
                        ok = false;
                        break;
                    }
                    if (found) {
                        // Add to contribution offsets
                        auto [insIt, inserted] = contributionOffset.insert({{name, contrib}, contributionOffset.size()});
                        outputSources.emplace_back(&results, insIt->second, it->name);
                    } else {
                        // Contribution not found, constant source
                        outputSources.emplace_back(it->name);
                    }
                } else {
                    // Instance not found, constant source
                    outputSources.emplace_back(it->name);
                }
                break;
            case OutdFrequency:
                outputSources.emplace_back(&frequency, it->name);
                break;
            case OutdOutputNoise:
                outputSources.emplace_back(&outputNoise, it->name);
                break;
            case OutdPowerGain:
                outputSources.emplace_back(&powerGain, it->name);
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

bool HBNoiseCore::addCoreOutputDescriptors(ErrorConsumer& errors) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    if (!addOutputDescriptor(OutputDescriptor(OutdFrequency, "frequency"))) {
        errors.push(CoreAddOutputDescriptor{"frequency"});
        return false;
    }
    if (!addOutputDescriptor(OutputDescriptor(OutdOutputNoise, "onoise"))) {
        errors.push(CoreAddOutputDescriptor{"output noise"});
        return false;
    }
    if (!addOutputDescriptor(OutputDescriptor(OutdPowerGain, "gain"))) {
        errors.push(CoreAddOutputDescriptor{"gain"});
        return false;
    }
    return true;
}

bool HBNoiseCore::addDefaultOutputDescriptors(ErrorConsumer& errors) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    if (savesCount==0) {
        // Add total noise contributions of all instances (details=false)
        return addAllNoiseContribInst(PTSave("default", Id(), Id()), false, errors);
    }
    return true;
}

bool HBNoiseCore::initializeOutputs(const std::string& name, ErrorConsumer& errors) {
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
        outfile->setPlotname("HB Periodic Noise Analysis");
    }
    outfile->prologue();

    return true;
}

bool HBNoiseCore::finalizeOutputs(ErrorConsumer& errors) {
    if (outfile) {
        outfile->epilogue();
        delete outfile;
        outfile = nullptr;
    }
    return true;
}

bool HBNoiseCore::deleteOutputs(Id name, ErrorConsumer& errors) {
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

// Construct omega vector: omega[n] = 2*pi*(f + f_n)
// where f_n = smsigFreq[n] is the signed spur frequency from the HB operating point.
// f    - small-signal output/baseband frequency (Hz)
// omega - output vector, resized to nf (number of spurs)
void HBNoiseCore::computeOmega(Real f) {
    auto& smsigFreq = spurs_.smsigFreq();
    auto nf = smsigFreq.size();
    for (size_t n = 0; n < nf; n++) {
        omega[n] = 2.0 * std::numbers::pi * (f + smsigFreq[n]);
    }
}

// Explicit instantiation for HBNoiseParameters; definition lives in corehbac.h, see there.
template bool HBACCore::rebuildCore<HBNoiseParameters>(
    HBNoiseParameters& params, HBCore& hbCore, Circuit& circuit,
    Spurs& spurs,
    CSCBlockSparseComplexMatrix& jacSpec, CSCBlockSparseComplexMatrix& acMatrix,
    DelayLines& delayLines, DelayMatrixBindings<DenseMatrixView<Complex>>& delayBindings,
    ErrorConsumer& errors
);

bool HBNoiseCore::rebuild(ErrorConsumer& errors) {

    if (!HBACCore::rebuildCore<HBNoiseParameters>(
        params, hbCore_, circuit, spurs_, jacSpec, acMatrix, delayLines_, delayBindings_, errors
    )) {
        return false;
    }
    auto nf = spurs_.mixingStencil().nRows();
    resolver_.setFreqCount(nf);
    acMatrix.setResolver(&resolver_);

    // Resolve output and input spurs (each a single scalar real frequency or
    // integer weight vector - unlike HBAC, no list of spurs is supported).
    auto [outOk, outNdx] = spurs_.smsigFreqIndex(params.outspur);
    if (!outOk) {
        errors.push(HbNoiseOutspurNotFound{});
        return false;
    }
    outSpurIndex = static_cast<int>(outNdx);

    auto [inOk, inNdx] = spurs_.smsigFreqIndex(params.inspur);
    if (!inOk) {
        errors.push(HbNoiseInspurNotFound{});
        return false;
    }
    inSpurIndex = static_cast<int>(inNdx);

    return true;
}

CoreCoroutine HBNoiseCore::coroutine(bool continuePrevious, ErrorConsumer& errors) {
    acMatrix.setAccounting(circuit.tables().accounting());

    auto& options = circuit.simulatorOptions().core();
    Int debug = options.smsig_debug;

    auto n = circuit.unknownCount();
    auto& stencil = spurs_.mixingStencil();
    auto nf = stencil.nRows();

    // Make sure structures are large enough
    // One bucket for each spur
    acSolution.resize((n+1)*nf);
    results.resize(contributionOffset.size());
    zero(results);

    // Get output unknowns
    auto [outOk, up, un] = getDiffNodePair(params.out, errors);
    if (!outOk) {
        co_yield CoreState::Aborted;
        co_return;
    }

    // Get input source
    auto [inOk, inputSource] = getExcitation(params.in, errors);
    if (!inOk) {
        co_yield CoreState::Aborted;
        co_return;
    }

    // Frequencies near spurs
    omega.resize(nf);

    // Compute HB solution
    if (params.hbParams.solve) {
        // Solve HB
        auto hbOk = hbCore_.run(continuePrevious, errors);
        if (!hbOk) {
            errors.push(HbNoiseHbFailed{});
            co_yield CoreState::Aborted;
            co_return;
        }
    }

    // Evaluate at the (solved or given) nodeset to obtain the time-domain
    // noise modulation function values (only computed by evaluateAtNodeset())
    if (!hbCore_.evaluateAtNodeset(true, errors)) {
        errors.push(HbNoiseHbFailed{});
        co_yield CoreState::Aborted;
        co_return;
    }

    // Collect frequency-domain Jacobians and noise modulation function spectra
    noiseModulationSpec.resize(circuit.modulatedNoiseCount()*nf);
    hbCore_.getFrequencyDomainJacobians(jacSpec, spurs_, &noiseModulationSpec);

    // Check if the Jacobians are finite
    if (options.matrixcheck && !jacSpec.isFinite(true, true, errors)) {
        errors.push(HbNoiseMatrixError{});
        if (debug>0) {
            Simulator::dbg() << "A frequency-domain Jacobian matrix entry is not finite.\n";
        }
        co_yield CoreState::Aborted;
        co_return;
    }

    if (debug>0) {
        Simulator::dbg() << "Starting HB periodic noise analysis.\n";
    }

    // Create sweeper
    ScalarSweep sweeper;
    if (!sweeper.setup(params, errors)) {
        errors.push(HbNoiseSweepSetupFailed{});
        co_yield CoreState::Aborted;
        co_return;
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
            errors.push(HbNoiseSweepComputeFailed{});
            error = true;
            break;
        }

        // The value, however, must be convertible to real
        if (!v.convertInPlace(Value::Type::Real)) {
            errors.push(HbNoiseBadFrequency{});
            error = true;
            break;
        }
        frequency = v.val<Real>();
        computeOmega(frequency);

        if (debug>0) {
            ss.str(""); ss << frequency;
            Simulator::dbg() << "frequency=" << ss.str() << "\n";
        }

        // Construct matrix
        HBACCore::fillMatrix(circuit, spurs_, jacSpec, acMatrix, omega, delayLines_, delayBindings_);

        // Check if matrix entries are finite, no need to check RHS
        // since we loaded it without any computation (i.e. we only used mag and phase)
        if (options.matrixcheck && !acMatrix.isFinite(true, true, errors)) {
            errors.push(HbNoiseMatrixError{});
            if (debug>0) {
                Simulator::dbg() << "A matrix entry is not finite.\n";
            }
            error = true;
            break;
        }

        // Factor
        bool forceFullFactorization = false;
        if (cxSolver_->isFactored()) {
            // Refactor (if possible). A refactor failure is not fatal here.
            if (!cxSolver_->refactor(errors)) {
                // Failed, try again by fully factoring
                forceFullFactorization = true;
            }
        }
        if (forceFullFactorization || !cxSolver_->isFactored()) {
            // Full factorization
            if (!cxSolver_->factor(errors)) {
                // Failed, give up
                errors.push(HbNoiseMatrixError{});
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
            auto [rcondOk, rcond] = cxSolver_->rcond(errors);
            if (!rcondOk) {
                errors.push(HbNoiseMatrixError{});
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
                errors.push(HbNoiseSingularMatrix{});
                error = true;
                break;
            }
        }

        // We solve the adjoint problem, specify the output as excitation and
        // solve the resulting system. The adjoint solution then gives the
        // (forward) transfer function from any excitation to the output via
        // a dot product against that excitation's RHS vector, with no extra
        // forward solve needed - reused below for every noise source too.
        zero(acSolution);
        acSolution[up*nf+outSpurIndex] += 1.0;
        acSolution[un*nf+outSpurIndex] -= 1.0;

        if (debug>=100) {
            Simulator::dbg() << "Adjoint linear system, output excitation\n";
            acMatrix.dump(Simulator::dbg(), dataWithoutBucket(acSolution, nf));
            Simulator::dbg() << "\n";
        }

        // Solve the transposed system
        if (!cxSolver_->tsolve(dataWithoutBucket(acSolution, nf), errors)) {
            errors.push(HbNoiseMatrixError{});
            if (debug>2) {
                Simulator::dbg() << "Failed to solve transposed factored system.\n";
            }
            error = true;
            break;
        }
        // Set bucket to 0
        VectorView(acSolution, nf) = Complex(0.0, 0.0);

        if (options.solutioncheck && !acMatrix.isFinite(dataWithoutBucket(acSolution, nf), true, true, errors)) {
            errors.push(HbNoiseSolutionNotFinite{});
            if (options.smsig_debug) {
                Simulator::dbg() << "A solution entry is not finite. Solver failed.\n";
            }
            error = true;
            break;
        }

        // Power gain from the input spur to the output spur: dot the adjoint
        // solution with the input source's excitation vector.
        auto [e1, e2] = inputSource->sourceExcitation(circuit);
        auto unity = inputSource->scaledUnityExcitation();
        auto tf = unity * (acSolution[e1*nf+inSpurIndex] - acSolution[e2*nf+inSpurIndex]);
        powerGain = std::abs(tf);
        powerGain *= powerGain;

        Vector<double> noiseDensity;

        Vector<Complex> noiseSourceToOutputGain(nf);
        
        // Set total output noise to 0
        outputNoise = 0.0;

        // Zero results vector
        zero(results);

        // Go through all instances
        auto ndev = circuit.deviceCount();
        for(decltype(ndev) idev=0; idev<ndev; idev++) {
            auto dev = circuit.device(idev);
            auto nmod = dev->modelCount();
            for(decltype(nmod) imod=0; imod<nmod; imod++) {
                auto mod = dev->model(imod);
                auto ninst = mod->instanceCount();
                for(decltype(ninst) iinst=0; iinst<ninst; iinst++) {
                    auto inst = mod->instance(iinst);

                    // Skip instances without noise sources
                    auto nSources = inst->noiseSourceCount();
                    if (nSources<=0) {
                        continue;
                    }

                    // Instance name
                    auto name = inst->name();

                    if (debug>1) {
                        Simulator::dbg() << "  instance '" << std::string(name) << "'\n";
                    }
                    
                    // TODO: Loop through all frequencies, evaluate noise at each frequency
                    // Store in a vector with nf slots, one slot per one frequency. 
                    // slot size equals number of noise sources. 
                    noiseDensity.resize(nSources*nf);
                    if (!inst->loadNoise(circuit, frequency, noiseDensity.data())) {
                        errors.push(HbNoisePsdFailed{});
                        if (debug>0) {
                            Simulator::dbg() << "Failed to compute noise.\n";
                        }
                        error = true;
                        break;
                    }

                    // Go through all noise sources
                    double sourceContribution = 0.0;
                    double totalInstanceContribution = 0.0;
                    for(decltype(nSources) ndx=0; ndx<nSources; ndx++) {
                        // Compute gain from noise source to output
                        // We have the adjoint solution yr with n*nf components + bucket
                        // Compute gain from noise source to output spur (zr) with nf components, 
                        // one per noise source spur. 
                        auto [e1, e2] = inst->noiseExcitation(circuit, ndx);
                        VectorView<Complex> e1Spurs(acSolution, e1*nf, nf, 1);
                        VectorView<Complex> e2Spurs(acSolution, e2*nf, nf, 1);
                        VectorView<Complex> zrSpurs(noiseSourceToOutputGain);
                        zrSpurs.vectorPlusScaledVector(e1Spurs, e2Spurs, -1);

                        // Compute wr = M^H z^conj with nf components
                        // Conjugate columns of M are rows od M^H. 
                        // Dot product each row with z^conj to get one component wr. 
                        // Do this without assembling M. 
                        
                        // Compute sum_i |zr_i|^2 RN_{ii}
                        // RN is a diagonal matrix holding PSDs at spur frequencies
                        
                        // Add to instance contribution
                    }
                    // End of noise sources loop

                    // Store total instance contribution
                    auto it = contributionOffset.find({name, Id()});
                    if (it!=contributionOffset.end()) {
                        results[it->second] = totalInstanceContribution;
                        if (debug>1) {
                            Simulator::dbg() << "    instance total=" << results[it->second] << "\n";
                        }
                    }

                    // Add to output noise
                    outputNoise += totalInstanceContribution;

                    if (error) {
                        break;
                    }
                }
                // End of instances loop

                if (error) {
                    break;
                }
            }
            // End of models loop

            if (error) {
                break;
            }
        }
        // End of devices loop

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
        Simulator::dbg() << "HB periodic noise frequency sweep " << (finished ? "completed" : "exited prematurely") << ".\n";
    }

    if (finished) {
        co_yield CoreState::Finished;
    } else {
        errors.push(HbNoiseSweepAborted{frequency});
        co_yield CoreState::Aborted;
    }
}

bool HBNoiseCore::run(bool continuePrevious, ErrorConsumer& errors) {
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

void HBNoiseCore::dump(std::ostream& os) const {
    AnalysisCore::dump(os);
    os << "  Results\n";
    os << "    Output noise: " << outputNoise << "\n";
    os << "    Power gain: " << powerGain << "\n";
    for(auto& it : contributionOffset) {
        auto [inst, contrib] = it.first;
        auto ndx = it.second;
        if (contrib) {
            os << "    n("+std::string(inst)+","+std::string(contrib)+") " << results[ndx] << "\n";
        } else {
            os << "    n("+std::string(inst)+") " << results[ndx] << "\n";
        }
    }
}

}
