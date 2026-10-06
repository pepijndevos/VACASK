#include <numbers>
#include <complex>
#include <filesystem>
#include "corepnoise.h"
#include "corepac.h"
#include "simulator.h"
#include "coresweep.h"
#include "common.h"
#include "densematrix.h"

namespace NAMESPACE {

// Default parameters
PNoiseParameters::PNoiseParameters() {
    pssParams.write = 0;
}

template<> int Introspection<PNoiseParameters>::setup() {
    registerMember(out);
    registerMember(in);
    registerMember(from);
    registerMember(to);
    registerMember(step);
    registerMember(mode);
    registerMember(points);
    registerMember(values);
    registerMember(outharm);
    registerMember(inharm);
    registerMember(truncharm);
    registerMember(write);
    registerMember(solver);
    registerNamedMember(pssParams.maxharm, "maxharm");
    registerNamedMember(pssParams.maxacfreq, "maxacfreq");
    registerNamedMember(pssParams.write, "writepss");
    registerNamedMember(pssParams.oscillator, "oscillator");
    registerNamedMember(pssParams.tper, "tper");
    registerNamedMember(pssParams.tstab, "tstab");
    registerNamedMember(pssParams.stabstep, "stabstep");
    registerNamedMember(pssParams.icmode, "icmode");
    registerNamedMember(pssParams.store, "store");
    registerNamedMember(pssParams.stabilParams.ic, "ic");
    registerNamedMember(pssParams.stabilParams.write, "writestab");
    registerNamedMember(pssParams.opParams.nodeset, "nodeset");
    registerNamedMember(pssParams.opParams.solver, "pssopsolver");
    registerNamedMember(pssParams.solve, "psssolve");

    return 0;
}
instantiateIntrospection(PNoiseParameters);

PNoiseCore::PNoiseCore(
    OutputDescriptorResolver& parentResolver, PNoiseParameters& params, PssCore& pssCore,
    std::unordered_map<std::pair<Id, Id>, size_t>& contributionOffset,
    Circuit& circuit, CommonData& commons,
    CSCBlockSparseComplexMatrix& jacSpec,
    CSCBlockSparseComplexMatrix& acMatrix, Vector<Complex>& acSolution,
    Vector<double>& results, double& powerGain, double& outputNoise
) : AnalysisCore(parentResolver, circuit, commons),
    pssCore_(pssCore),
    outfile(nullptr),
    jacSpec(jacSpec),
    acMatrix(acMatrix),
    acSolution(acSolution),
    contributionOffset(contributionOffset),
    results(results), powerGain(powerGain), outputNoise(outputNoise),
    params(params),
    maxharm_(0),
    outHarmIndex(0),
    inHarmIndex(0),
    frequency(0.0),
    resolver_(circuit),
    cxSolver_(nullptr) {
}

PNoiseCore::~PNoiseCore() {
    outfile.reset();
}

bool PNoiseCore::resolveOutputDescriptors(bool strict, ErrorConsumer& errors) {
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
                        errors.push(PNoiseInstanceNotFound{name});
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
                    errors.push(PNoiseInstanceNotFound{name});
                    ok = false;
                    break;
                }
                // Find contrib
                if (inst) {
                    std::tie(contribIndex, found) = inst->uniqueNoiseSourceIndex(contrib);
                    if (strict && !found) {
                        errors.push(PNoiseContribNotFound{name, contrib});
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

bool PNoiseCore::addCoreOutputDescriptors(ErrorConsumer& errors) {
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

bool PNoiseCore::addDefaultOutputDescriptors(ErrorConsumer& errors) {
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

bool PNoiseCore::initializeOutputs(const std::string& name, ErrorConsumer& errors) {
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    // Create output file if not created yet
    if (!outfile) {
        outfile = std::make_unique<OutputRawfile>(
            name, outputSources,
            (circuit.simulatorOptions().core().rawfile==SimulatorOptions::rawfileBinary ? OutputRawfile::Flags::Binary : OutputRawfile::Flags::None) |
                OutputRawfile::Flags::Padded);
        outfile->setTitle(circuit.title());
        outfile->setPlotname("PSS Periodic Noise Analysis");
    }
    if (!outfile->prologue(errors)) {
        return false;
    }

    return true;
}

bool PNoiseCore::finalizeOutputs(ErrorConsumer& errors) {
    if (outfile) {
        bool ok = outfile->epilogue(errors);
        outfile.reset();
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool PNoiseCore::deleteOutputs(Id name, ErrorConsumer& errors) {
    // Close output file (if open), Windows cannot delete an open file
    outfile.reset();

    if (!params.write || Simulator::noOutput()) {
        return true;
    }

    // Cannot assume outfile is available
    auto fname = std::string(name)+".raw";
    std::error_code ec;
    std::filesystem::remove(fname, ec);
    return true;
}

// Construct omega vector: omega[row] = 2*pi*(f + h*f0)
// where h = row-maxharm_ is the signed harmonic number and f0 = 1/T0.
void PNoiseCore::computeOmega(Real f) {
    auto f0 = 1.0 / pssCore_.convergedPeriod();
    auto nf = static_cast<size_t>(2*maxharm_+1);
    for (size_t row = 0; row < nf; row++) {
        auto h = static_cast<Real>(static_cast<Int>(row) - maxharm_);
        omega[row] = 2.0 * std::numbers::pi * (f + h*f0);
    }
}

// See declaration in corepnoise.h; own copy of PACCore::smsigFreqIndex's logic
std::tuple<bool, size_t> PNoiseCore::smsigFreqIndex(const Value& v, bool allowFrequency, ErrorConsumer& errors) const {
    if (v.type()==Value::Type::Int) {
        auto h = v.val<const Int>();
        if (h < -maxharm_ || h > maxharm_) {
            return std::make_tuple(false, size_t(0));
        }
        return std::make_tuple(true, static_cast<size_t>(h+maxharm_));
    }
    if (v.type()==Value::Type::Real) {
        if (!allowFrequency) {
            errors.push(PNoiseFrequencySidebandNotAllowed{});
            return std::make_tuple(false, size_t(0));
        }
        auto T0 = pssCore_.convergedPeriod();
        if (T0<=0) {
            throw std::logic_error("PNoiseCore::smsigFreqIndex(): PSS period unknown despite allowFrequency=true.");
        }
        auto f0 = 1.0 / T0;
        auto hReal = v.val<const Real>() / f0;
        auto h = std::round(hReal);
        auto freqtol = 1e-14;
        auto tol = freqtol * std::max(1.0, std::fabs(h));
        if (std::fabs(hReal - h) < tol && h >= -maxharm_ && h <= maxharm_) {
            auto hint = static_cast<Int>(h);
            return std::make_tuple(true, static_cast<size_t>(hint+maxharm_));
        }
    }
    errors.push(PNoiseBadSidebandSpec{});
    return std::make_tuple(false, size_t(0));
}

// See declaration in corepnoise.h
void PNoiseCore::applyModulationAdjoint(
    Int maxharm, const VectorView<Complex>& ma, const Vector<Complex>& zr, Vector<Complex>& wr
) {
    auto nf = static_cast<size_t>(2*maxharm+1);
    for (size_t m = 0; m < nf; m++) {
        auto iStart = static_cast<size_t>(std::max<Int>(0, static_cast<Int>(m) - maxharm));
        auto iEnd   = static_cast<size_t>(std::min<Int>(static_cast<Int>(nf) - 1, static_cast<Int>(m) + maxharm));
        Complex acc(0.0, 0.0);
        for (size_t i = iStart; i <= iEnd; i++) {
            auto k = static_cast<Int>(i) - static_cast<Int>(m);
            Complex mk = (k>=0) ? ma[static_cast<size_t>(k)] : std::conj(ma[static_cast<size_t>(-k)]);
            acc += std::conj(mk) * std::conj(zr[i]);
        }
        wr[m] = acc;
    }
}

bool PNoiseCore::rebuild(ErrorConsumer& errors) {
    maxharm_ = params.truncharm;

    if (maxharm_ < 0) {
        errors.push(PNoiseMaxharmInvalid{});
        return false;
    }

    // We do not have PSS results yet so we don't know the period
    auto nFreq = maxharm_ + 1;      // jacSpec rows: DC..truncharm (Toeplitz basis)
    auto nf    = 2*maxharm_ + 1;    // acMatrix rows/cols: sidebands -maxharm..maxharm

    // Jacobian spectral components
    if (!jacSpec.rebuild(circuit.sparsityMap(), circuit.unknownCount(), nFreq, 2, errors, true)) {
        return false;
    }

    // Conversion matrix
    if (!acMatrix.rebuild(circuit.sparsityMap(), circuit.unknownCount(), nf, nf, errors)) {
        return false;
    }

    resolver_.setFreqCount(nf);
    acMatrix.setResolver(&resolver_);

    omega.resize(nf);

    // Resolve output/input harmonics. PSS period is not known yet, so only
    // index (not frequency) specification is allowed here.
    auto [outOk, outNdx] = smsigFreqIndex(params.outharm, false, errors);
    if (!outOk) {
        errors.push(PNoiseOutharmNotFound{});
        return false;
    }
    outHarmIndex = static_cast<int>(outNdx);

    auto [inOk, inNdx] = smsigFreqIndex(params.inharm, false, errors);
    if (!inOk) {
        errors.push(PNoiseInharmNotFound{});
        return false;
    }
    inHarmIndex = static_cast<int>(inNdx);

    return true;
}

CoreCoroutine PNoiseCore::coroutine(bool continuePrevious, ErrorConsumer& errors) {
    acMatrix.setAccounting(circuit.tables().accounting());

    auto& options = circuit.simulatorOptions().core();
    Int debug = options.smsig_debug;

    auto n = circuit.unknownCount();
    auto nf = static_cast<size_t>(2*maxharm_+1);

    // Make sure structures are large enough
    // One bucket for each sideband
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

    omega.resize(nf);

    // Compute PSS solution
    if (params.pssParams.solve) {
        // Solve PSS
        auto pssOk = pssCore_.run(continuePrevious, errors);
        if (!pssOk) {
            errors.push(PNoisePssFailed{});
            co_yield CoreState::Aborted;
            co_return;
        }
    }

    // Evaluate PSS at the (solved or given) IC, also collecting the
    // time-domain noise modulation function values (only computed by evaluate())
    if (!pssCore_.evaluate(!params.pssParams.solve, true, -1, errors)) {
        errors.push(PNoisePssFailed{});
        co_yield CoreState::Aborted;
        co_return;
    }
    const Vector<double>& noiseExp = pssCore_.noiseExponents();

    // Collect frequency-domain Jacobians and noise modulation function
    // spectra (DC..maxharm_, Toeplitz basis - see PACCore::fillMatrix)
    noiseModulationSpec.resize(circuit.noiseModulationSlotsCount()*(maxharm_+1));
    if (!pssCore_.getFrequencyDomainJacobians(jacSpec, maxharm_, &noiseModulationSpec, errors)) {
        co_yield CoreState::Aborted;
        co_return;
    }

    // Check if the Jacobians are finite
    if (options.matrixcheck && !jacSpec.isFinite(true, true, errors)) {
        errors.push(PNoiseMatrixError{});
        if (debug>0) {
            Simulator::dbg() << "A frequency-domain Jacobian matrix entry is not finite.\n";
        }
        co_yield CoreState::Aborted;
        co_return;
    }

    if (debug>0) {
        Simulator::dbg() << "Starting PSS periodic noise analysis.\n";
    }

    // Create sweeper
    ScalarSweep sweeper;
    if (!sweeper.setup(params, errors)) {
        errors.push(PNoiseSweepSetupFailed{});
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
    auto f0 = 1.0 / pssCore_.convergedPeriod();
    const double freqTol = 1e-14;
    Vector<double> noiseDensity;
    Vector<Complex> zr(nf);
    Vector<Complex> wr(nf);
    do {
        // Compute should always succeed
        Value v;
        if (!sweeper.compute(v, errors)) {
            errors.push(PNoiseSweepComputeFailed{});
            error = true;
            break;
        }

        // The value, however, must be convertible to real
        if (!v.convertInPlace(Value::Type::Real)) {
            errors.push(PNoiseBadFrequency{});
            error = true;
            break;
        }
        frequency = v.val<Real>();
        computeOmega(frequency);

        if (debug>0) {
            ss.str(""); ss << frequency;
            Simulator::dbg() << "frequency=" << ss.str() << "\n";
        }

        // Construct matrix, like in PAC
        PACCore::fillMatrix(circuit, maxharm_, jacSpec, acMatrix, omega);

        // Check if matrix entries are finite, no need to check RHS
        // since we loaded it without any computation (i.e. we only used mag and phase)
        if (options.matrixcheck && !acMatrix.isFinite(true, true, errors)) {
            errors.push(PNoiseMatrixError{});
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
                errors.push(PNoiseMatrixError{});
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
                errors.push(PNoiseMatrixError{});
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
                errors.push(PNoiseSingularMatrix{});
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
        acSolution[up*nf+outHarmIndex] += 1.0;
        acSolution[un*nf+outHarmIndex] -= 1.0;

        if (debug>=100) {
            Simulator::dbg() << "Adjoint linear system, output excitation\n";
            acMatrix.dump(Simulator::dbg(), dataWithoutBucket(acSolution, nf));
            Simulator::dbg() << "\n";
        }

        // Solve the transposed system
        if (!cxSolver_->tsolve(dataWithoutBucket(acSolution, nf), errors)) {
            errors.push(PNoiseMatrixError{});
            if (debug>2) {
                Simulator::dbg() << "Failed to solve transposed factored system.\n";
            }
            error = true;
            break;
        }
        // Set bucket to 0
        VectorView(acSolution, nf) = Complex(0.0, 0.0);

        if (options.solutioncheck && !acMatrix.isFinite(dataWithoutBucket(acSolution, nf), true, true, errors)) {
            errors.push(PNoiseSolutionNotFinite{});
            if (options.smsig_debug) {
                Simulator::dbg() << "A solution entry is not finite. Solver failed.\n";
            }
            error = true;
            break;
        }

        // Power gain from the input harmonic to the output harmonic: dot the
        // adjoint solution with the input source's excitation vector.
        auto [e1, e2] = inputSource->sourceExcitation(circuit);
        auto unity = inputSource->scaledUnityExcitation();
        auto tf = unity * (acSolution[e1*nf+inHarmIndex] - acSolution[e2*nf+inHarmIndex]);
        powerGain = std::abs(tf);
        powerGain *= powerGain;

        // Set total output noise to 0
        outputNoise = 0.0;

        // Zero results vector
        zero(results);

        // Set when a flicker source meets a spur at zero frequency
        bool flickerAtDc = false;

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

                    // Base slot of this instance's modulated noise sources
                    auto noiseModBase = inst->noiseModulationBase();

                    // Loop through all sidebands, evaluate noise at each frequency
                    // Store in a vector with nf slots, one slot per one frequency.
                    // slot size equals number of noise sources.
                    noiseDensity.resize(nSources*nf);
                    for (decltype(nf) i=0; i<nf; i++) {
                        auto h = static_cast<Real>(static_cast<Int>(i) - maxharm_);
                        auto freqAtSpur = std::abs(frequency + h*f0);
                        if (!inst->loadNoise(circuit, freqAtSpur, noiseDensity.data()+i*nSources)) {
                            errors.push(PNoisePsdFailed{});
                            if (debug>0) {
                                Simulator::dbg() << "Failed to compute noise.\n";
                            }
                            error = true;
                            break;
                        }
                        // White/flicker noise is A/f^alpha. We absorbed A into the modulation function.
                        // All computations work with two-sided PSD.
                        // The fact that OSDI returns one-sided PSD was compensated in the
                        // modulation function when we absorbed A (see osdiinstance.cpp).
                        // Overwrite white noise with 1 and flicker noise with 1/f^alpha
                        auto modulatedNoiseSlot = noiseModBase;
                        for (decltype(nSources) ndx=0; ndx<nSources; ndx++) {
                            switch (inst->noiseSourceType(ndx)) {
                                case NoiseType::White:
                                    noiseDensity[i*nSources+ndx] = 1;
                                    modulatedNoiseSlot++;
                                    break;
                                case NoiseType::Flicker:
                                    // 1/f blows up (or overflows) where a spur lands on
                                    // or near DC, i.e. the offset frequency equals or
                                    // nearly equals a pump harmonic. Leave that spur out
                                    // and warn below instead of propagating inf/nan.
                                    if (freqAtSpur>freqTol*std::max(1.0, std::fabs(h))*f0) {
                                        auto ef = noiseExp[modulatedNoiseSlot];
                                        noiseDensity[i*nSources+ndx] = std::pow(freqAtSpur, -ef);
                                    } else {
                                        noiseDensity[i*nSources+ndx] = 0;
                                        flickerAtDc = true;
                                    }
                                    modulatedNoiseSlot++;
                                    break;
                                case NoiseType::Table:
                                    // For table noise the modulation function is ma(t) = 1.
                                    // We need to take into account that the returned PSD
                                    // is one-sided by dividing it with 2.
                                    noiseDensity[i*nSources+ndx] /= 2;
                                    break;
                                default:
                                    break;
                            }
                        }
                    }
                    if (error) {
                        break;
                    }

                    // Go through all noise sources
                    double totalInstanceContribution = 0.0;
                    // Base slot of this instance's modulated noise sources;
                    // modulatedNoiseSlot counts non-Table sources seen so far
                    // (matches the order slots were allocated in, see setup())
                    auto modulatedNoiseBase = inst->noiseModulationBase();
                    GlobalStorageIndex modulatedNoiseSlot = 0;
                    for(decltype(nSources) ndx=0; ndx<nSources; ndx++) {
                        // Compute gain from noise source to output
                        // We have the adjoint solution yr with n*nf components + bucket
                        // Compute gain from noise source to output harmonic (zr) with nf components,
                        // one per noise source sideband.
                        auto [e1, e2] = inst->noiseExcitation(circuit, ndx);
                        VectorView<Complex> e1Spurs(acSolution, e1*nf, nf, 1);
                        VectorView<Complex> e2Spurs(acSolution, e2*nf, nf, 1);
                        VectorView<Complex> zrView(zr);
                        zrView.vectorPlusScaledVector(e1Spurs, e2Spurs, -1);

                        // Compute wr = M^H zr^conj with nf components, without
                        // assembling the Toeplitz modulation matrix M
                        bool isTable = inst->noiseSourceType(ndx)==NoiseType::Table;
                        if (!isTable && modulatedNoiseBase!=SIM_SIZE_T_MAX) {
                            auto mSlotBase = (modulatedNoiseBase+modulatedNoiseSlot)*(maxharm_+1);
                            VectorView sourceModulationSpectrum(noiseModulationSpec, mSlotBase, maxharm_+1, 1);
                            applyModulationAdjoint(maxharm_, sourceModulationSpectrum, zr, wr);
                        } else {
                            // No modulation function (Table-type source): M = I
                            for (decltype(nf) m=0; m<nf; m++) {
                                wr[m] = std::conj(zr[m]);
                            }
                        }
                        if (!isTable) {
                            modulatedNoiseSlot++;
                        }

                        // Compute sum_i |wr_i|^2 RN_{ii}
                        // RN is a diagonal matrix holding PSDs at spur frequencies
                        // We never form it. We get its diagonal from noiseDensity.
                        double sourceContribution = 0.0;
                        // Offset is noise source index, length is nf, stride is nSources
                        VectorView psdAtSpur(noiseDensity, ndx, nf, nSources);
                        for (decltype(nf) i=0; i<nf; i++) {
                            auto w = std::abs(wr[i]);
                            sourceContribution += w*w*psdAtSpur[i];
                        }

                        // We just computed two-sided PSD. We need to return the one-sided PSD.
                        sourceContribution *= 2;

                        // Find slot to which we store the contribution of this noise source
                        auto contrib = inst->noiseSourceName(ndx);
                        auto it = contributionOffset.find({name, contrib});
                        if (it!=contributionOffset.end()) {
                            // Store contribution
                            results[it->second] += sourceContribution;
                        }
                        if (debug>1) {
                            // Name and output PSD, index, if saved
                            Simulator::dbg() << "    contribution '" << std::string(contrib)
                                             << "' output psd=" << sourceContribution;
                            if (it!=contributionOffset.end()) {
                                Simulator::dbg() << " ndx=" << it->second;
                            }
                            Simulator::dbg() << "\n";
                            if (it!=contributionOffset.end()) {
                                // Running sum over contributions with same name
                                Simulator::dbg() << "      total=" << results[it->second] << "\n";
                            }
                        }

                        // Add to instance contribution
                        totalInstanceContribution += sourceContribution;
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

        if (flickerAtDc) {
            Simulator::wrn() << "Warning, offset frequency " << frequency
                             << " coincides with a pump harmonic. Flicker noise of the spur at zero frequency is undefined and was left out.\n";
        }

        // Dump solution
        if (params.write && !Simulator::noOutput() && outfile) {
            if (!outfile->addPoint(errors)) {
                error = true;
                break;
            }
        }

        finished = sweeper.advance();

        setProgress(sweeper.at(), frequency);
    } while (!finished && !error);

    if (debug>0) {
        Simulator::dbg() << "PSS periodic noise frequency sweep " << (finished ? "completed" : "exited prematurely") << ".\n";
    }

    if (finished) {
        co_yield CoreState::Finished;
    } else {
        errors.push(PNoiseSweepAborted{frequency});
        co_yield CoreState::Aborted;
    }
}

bool PNoiseCore::run(bool continuePrevious, ErrorConsumer& errors) {
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

void PNoiseCore::dump(std::ostream& os) const {
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
