#include <numbers>
#include <complex>
#include <algorithm>
#include "corepac.h"
#include "simulator.h"
#include "common.h"
#include "densematrix.h"

namespace NAMESPACE {

// Default parameters
PACParameters::PACParameters() {
    pssParams.write = 0;
    outharm = ValueVector();
}

template<> int Introspection<PACParameters>::setup() {
    registerMember(from);
    registerMember(to);
    registerMember(step);
    registerMember(mode);
    registerMember(points);
    registerMember(values);
    registerMember(outharm);
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
    registerNamedMember(pssParams.maxharm, "maxharm");
    registerNamedMember(pssParams.maxacfreq, "maxacfreq");
    registerNamedMember(pssParams.store, "store");
    registerNamedMember(pssParams.stabilParams.ic, "ic");
    registerNamedMember(pssParams.stabilParams.write, "writestab");
    registerNamedMember(pssParams.opParams.nodeset, "nodeset");
    registerNamedMember(pssParams.opParams.solver, "pssopsolver");
    registerNamedMember(pssParams.solve, "psssolve");

    return 0;
}
instantiateIntrospection(PACParameters);

PACCore::PACCore(
    OutputDescriptorResolver& parentResolver, PACParameters& params, PssCore& pssCore,
    Circuit& circuit, CommonData& commons,
    CSCBlockSparseComplexMatrix& jacSpec,
    CSCBlockSparseComplexMatrix& pacMatrix, Vector<Complex>& pacSolution
) : AnalysisCore(parentResolver, circuit, commons),
    pssCore_(pssCore),
    outfile(nullptr),
    jacSpec(jacSpec),
    pacMatrix(pacMatrix),
    pacSolution(pacSolution),
    params(params),
    maxharm_(0),
    frequency(0.0),
    pacResolver_(circuit),
    cxSolver_(nullptr) {
}

PACCore::~PACCore() {
    outfile.reset();
}

// The methods are called in the following order
// rebuild()
// resolveOutputDescriptors()
// run()
// 
// Before run() we don't know trhe period/fundamental frequency. 
// Therefore we cannot select output harmonics based on the frequency. 
// PSS determines the period for driven circuit based on the tper 
// parameter or initial conditions set by the ic parameter. 
// Therefore the period is not 100% known until PSS runs. 
// Consequently we can select harmonics only by their index. 

bool PACCore::resolveOutputDescriptors(bool strict, ErrorConsumer& errors) {
    // Clear output sources
    outputSources.clear();
    // Resolve output descriptors
    bool ok = true;
    auto nStoredHarms = outHarmRows.size();
    auto nf = static_cast<size_t>(2*maxharm_+1);

    for (auto it = outputDescriptors.cbegin(); it != outputDescriptors.cend(); ++it) {
        switch (it->type) {
        case OutdSolComponent:
            for(decltype(nStoredHarms) i=0; i<nStoredHarms; i++) {
                Id name = std::string(it->id)+";"+suffixes[i];
                if (!addComplexVarOutputSource(strict, it->id, pacSolution, nf, outHarmRows[i], name, errors)) {
                    ok = false;
                    break;
                }
            }
            break;
        case OutdFrequency:
            outputSources.emplace_back(&frequency, it->name);
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

bool PACCore::addCoreOutputDescriptors(ErrorConsumer& errors) {
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

bool PACCore::addDefaultOutputDescriptors(ErrorConsumer& errors) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    if (savesCount==0) {
        return addAllUnknowns(PTSave("default", Id(), Id()), errors);
    }
    return true;
}

bool PACCore::initializeOutputs(Id name, ErrorConsumer& errors) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    // Create output file if not created yet
    if (!outfile) {
        outfile = std::make_unique<OutputRawfile>(
            name, outputSources,
            (circuit.simulatorOptions().core().rawfile==SimulatorOptions::rawfileBinary ? OutputRawfile::Flags::Binary : OutputRawfile::Flags::None) |
                OutputRawfile::Flags::Padded | OutputRawfile::Flags::Complex);
        outfile->setTitle(circuit.title());
        outfile->setPlotname("PAC Small Signal Analysis");
    }
    if (!outfile->prologue(errors)) {
        return false;
    }

    return true;
}

bool PACCore::finalizeOutputs(ErrorConsumer& errors) {
    if (outfile) {
        bool ok = outfile->epilogue(errors);
        outfile.reset();
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool PACCore::deleteOutputs(Id name, ErrorConsumer& errors) {
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

void PACCore::constructSuffixes() {
    suffixes.clear();
    for (auto row : outHarmRows) {
        suffixes.push_back(std::to_string(row - maxharm_));
    }
}

// Construct omega vector: omega[row] = 2*pi*(f + h*f0)
// where h = row-maxharm_ is the signed harmonic number and f0 = 1/T0.
// f    - small-signal probe frequency (Hz)
// omega - output vector, resized to 2*maxharm_+1 in rebuild()
void PACCore::computeOmega(Real f) {
    auto f0 = 1.0 / pssCore_.convergedPeriod();
    auto nf = static_cast<size_t>(2*maxharm_+1);
    for (size_t row = 0; row < nf; row++) {
        auto h = static_cast<Real>(static_cast<Int>(row) - maxharm_);
        omega[row] = 2.0 * std::numbers::pi * (f + h*f0);
    }
}

// Used in place of Spurs::smsigFreqIndex()
std::tuple<bool, size_t> PACCore::smsigFreqIndex(const Value& v, bool allowFrequency, ErrorConsumer& errors) const {
    if (v.type()==Value::Type::Int) {
        auto h = v.val<const Int>();
        if (h < -maxharm_ || h > maxharm_) {
            return std::make_tuple(false, size_t(0));
        }
        return std::make_tuple(true, static_cast<size_t>(h+maxharm_));
    }
    if (v.type()==Value::Type::Real) {
        if (!allowFrequency) {
            errors.push(PacFrequencySidebandNotAllowed{});
            return std::make_tuple(false, size_t(0));
        }
        // Evaluate sets the period, pss also sets the period
        // pss sets the period, but only after it is run. 
        // Callers should disable spur specification by freqeuncy for autonomous pss. 
        auto T0 = pssCore_.convergedPeriod();
        if (T0<=0) {
            throw std::logic_error("PACCore::smsigFreqIndex(): PSS period unknown despite allowFrequency=true.");
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
    errors.push(PacBadSidebandSpec{});
    return std::make_tuple(false, size_t(0));
}

// Fill one (row,col) subblock of the PAC conversion matrix H(omega). The
// matrix is Toeplitz: the entry depends only on k=row-col, since the LPTV
// Jacobian has a single fundamental (period T0). For k>=0 the coefficient is
// the Jacobian's own k-th Fourier harmonic; for k<0 it is the conjugate of
// the |k|-th harmonic (jG(t)/jC(t) are real time signals). Harmonics beyond
// maxharm are truncated - left at zero, as pre-zeroed by pacMatrix.zero().
//
// Parameters:
//   maxharm - number of harmonics kept beyond DC
//   G, C  - Jacobian's own Fourier coefficients [G_k]_pq / [C_k]_pq for
//           k=0..maxharm
//   omega - small-signal frequencies 2*pi*(f+h*f0), one per output row
//   block - (p,q) subblock of H(omega) to fill, size nf x nf, nf=2*maxharm+1
// Shared (static) implementation, see declaration in corepac.h
void PACCore::fillDenseBlock(
    Int maxharm,
    const VectorView<Complex>& G,
    const VectorView<Complex>& C,
    const Vector<Real>& omega,
    DenseMatrixView<Complex>& block
) {
    auto nf = static_cast<size_t>(2*maxharm+1);
    for (size_t col = 0; col < nf; col++) {
        auto rowStart = static_cast<size_t>(std::max<Int>(0, static_cast<Int>(col) - maxharm));
        auto rowEnd   = static_cast<size_t>(std::min<Int>(static_cast<Int>(nf) - 1, static_cast<Int>(col) + maxharm));
        for (size_t row = rowStart; row <= rowEnd; row++) {
            auto k = static_cast<Int>(row) - static_cast<Int>(col);
            Complex Gk, Ck;
            if (k >= 0) {
                Gk = G[static_cast<size_t>(k)];
                Ck = C[static_cast<size_t>(k)];
            } else {
                Gk = std::conj(G[static_cast<size_t>(-k)]);
                Ck = std::conj(C[static_cast<size_t>(-k)]);
            }
            block.at(row, col) = Gk + Complex(0.0, omega[row]) * Ck;
        }
    }
}

// Shared (static) implementation, see declaration in corepac.h
void PACCore::fillMatrix(
    Circuit& circuit, Int maxharm,
    CSCBlockSparseComplexMatrix& jacSpec, CSCBlockSparseComplexMatrix& pacMatrix,
    const Vector<Real>& omega
) {
    pacMatrix.zero();
    auto& positions = circuit.sparsityMap().positions();
    for (MatrixEntryIndex nzIndex = 0; nzIndex < positions.size(); nzIndex++) {
        auto& [pos, flags] = positions[nzIndex];
        // Delay only blocks are skipped. PSS currently rejects circuits using
        // absdelay outright (see PssCore::rebuild()), so this never triggers
        // today, but is kept for consistency with the other small-signal cores.
        if ((flags & EntryFlags::EntryType) == EntryFlags::Delay) {
            continue;
        }

        // Jacobian spectrum block, column 0 is G, column 1 is C
        auto [jacSpecBlock, jacSpecPos, jacSpecFlags] = jacSpec.blockFromIndex(nzIndex);
        auto G = jacSpecBlock.column(0);
        auto C = jacSpecBlock.column(1);

        // Get PAC matrix block
        auto [block, blockPos, blockFlags] = pacMatrix.blockFromIndex(nzIndex);

        // Fill block
        fillDenseBlock(maxharm, G, C, omega, block);
    }
}

bool PACCore::rebuild(ErrorConsumer& errors) {
    maxharm_ = params.truncharm;

    if (maxharm_ < 0) {
        errors.push(PacMaxharmInvalid{});
        return false;
    }
 
    // We do not have PSS results yet so we dont't know the period
    auto nFreq = maxharm_ + 1;      // jacSpec rows: DC..truncharm (Toeplitz basis)
    auto nf    = 2*maxharm_ + 1;    // pacMatrix rows/cols: sidebands -maxharm..maxharm
    
    // Jacobian spectral components
    if (!jacSpec.rebuild(circuit.sparsityMap(), circuit.unknownCount(), nFreq, 2, errors, true)) {
        return false;
    }

    // PAC conversion matrix
    if (!pacMatrix.rebuild(circuit.sparsityMap(), circuit.unknownCount(), nf, nf, errors)) {
        return false;
    }

    pacResolver_.setFreqCount(nf);
    pacMatrix.setResolver(&pacResolver_);

    omega.resize(nf);

    // Collect output harmonic sidebands
    std::vector<int> newOutHarmRows;
    if (params.outharm.type() == Value::Type::ValueVec) {
        // List of sidebands: each element is a signed harmonic
        // Empty list means all sidebands
        if (params.outharm.size()==0) {
            for(decltype(nf) freqNdx=0; freqNdx<nf; freqNdx++) {
                newOutHarmRows.push_back(static_cast<int>(freqNdx));
            }
        } else {
            size_t cnt=0;
            for (const auto& v : params.outharm.val<ValueVector>()) {
                // Allow spur specification only by index
                auto [ok, freqNdx] = smsigFreqIndex(v, false, errors);
                if (!ok) {
                    errors.push(PacOutharmNotFound{cnt});
                    return false;
                }
                newOutHarmRows.push_back(static_cast<int>(freqNdx));
                cnt++;
            }
        }
    } else {
        // Single sideband: scalar signed harmonic or real frequency
        // Allow spur specification only by index
        auto [ok, freqNdx] = smsigFreqIndex(params.outharm, false, errors);
        if (!ok) {
            errors.push(PacOutharmSingleNotFound{});
            return false;
        }
        newOutHarmRows.push_back(static_cast<int>(freqNdx));
    }

    // No output sidebands, error
    if (newOutHarmRows.empty()) {
        errors.push(PacNoOutharm{});
        return false;
    }

    // Check for change
    if (outHarmRows.size()!=0 && newOutHarmRows!=outHarmRows) {
        errors.push(PacOutharmChanged{});
        return false;
    }

    outHarmRows = std::move(newOutHarmRows);
    constructSuffixes();

    return true;
}

bool PACCore::collectExcitations(ErrorConsumer& errors) {
    excitations.clear();

    auto ndev = circuit.deviceCount();
    for(decltype(ndev) idev=0; idev<ndev; idev++) {
        auto dev = circuit.device(idev);
        if (!dev->isSource()) {
            continue;
        }
        auto nmod = dev->modelCount();
        for(decltype(nmod) imod=0; imod<nmod; imod++) {
            auto mod = dev->model(imod);
            auto ninst = mod->instanceCount();
            for(decltype(ninst) iinst=0; iinst<ninst; iinst++) {
                auto inst = mod->instance(iinst);
                // Extract excitation harmonics
                auto [spurs, mags, phases] = inst->spur();

                // Check vector lengths
                auto nSpurs = spurs.size();
                if (mags.size()>nSpurs) {
                    errors.push(PacMagLength{inst->name()});
                    return false;
                }
                if (phases.size()>nSpurs) {
                    errors.push(PacPhaseLength{inst->name()});
                    return false;
                }

                if (nSpurs==0) {
                    continue;
                }

                excitations.push_back(Excitation{inst, {}, {}});
                for(decltype(nSpurs) i=0; i<nSpurs; i++) {
                    // Allow spur specification also by freqeuncy because bny now pss has
                    // run and the period is known
                    auto [ok, freqNdx] = smsigFreqIndex(spurs[i], true, errors);
                    if (!ok) {
                        errors.push(PacExcitationHarmNotFound{i, inst->name()});
                        return false;
                    }
                    auto mag = (mags.size()>i) ? mags[i] : 0.0;
                    auto ph = (phases.size()>i) ? phases[i] : 0.0;

                    double re = mag*std::cos(ph*std::numbers::pi/180);
                    double im = mag*std::sin(ph*std::numbers::pi/180);

                    excitations.back().harm.push_back(freqNdx);
                    excitations.back().value.push_back(Complex(re, im));
                }
            }
        }
    }

    return true;
}

CoreCoroutine PACCore::coroutine(bool continuePrevious, ErrorConsumer& errors) {
    pacMatrix.setAccounting(circuit.tables().accounting());

    auto& options = circuit.simulatorOptions().core();
    Int debug = options.smsig_debug;

    auto n = circuit.unknownCount();
    auto nf = 2*maxharm_+1;

    // Make sure structures are large enough
    // One bucket for each sideband
    pacSolution.resize((n+1)*nf);
    omega.resize(nf);

    // Compute PSS solution
    if (params.pssParams.solve) {
        // Solve PSS
        auto pssOk = pssCore_.run(continuePrevious, errors);
        if (!pssOk) {
            errors.push(PacPssFailed{});
            co_yield CoreState::Aborted;
            co_return;
        }
    } 
    
    // Evaluate PSS, sampling enough points to satisfy maxacfreq and maxharm. 
    // We do this always, even if we ran PSS due to solve=1. 
    // Evaluate at ic if solve=0
    if (!pssCore_.evaluate(!params.pssParams.solve, false, -1, errors)) {
        errors.push(PacPssFailed{});
        co_yield CoreState::Aborted;
        co_return;
    }
    
    // Collect frequency-domain Jacobians (DC..maxharm_, Toeplitz basis)
    if (!pssCore_.getFrequencyDomainJacobians(jacSpec, maxharm_, nullptr, errors)) {
        co_yield CoreState::Aborted;
        co_return;
    }

    // Collect excitations - we need the period for this so call pss/evaluate first
    if (!collectExcitations(errors)) {
        co_yield CoreState::Aborted;
        co_return;
    }

    // Check if the Jacobians are finite
    if (options.matrixcheck && !jacSpec.isFinite(true, true, errors)) {
        errors.push(PacMatrixError{});
        if (debug>0) {
            Simulator::dbg() << "A frequency-domain Jacobian matrix entry is not finite.\n";
        }
        co_yield CoreState::Aborted;
        co_return;
    }

    if (debug>0) {
        Simulator::dbg() << "Starting PAC small-signal analysis.\n";
    }

    // Create sweeper
    ScalarSweep sweeper;
    if (!sweeper.setup(params, errors)) {
        errors.push(PacSweepSetupFailed{});
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
            errors.push(PacSweepComputeFailed{});
            error = true;
            break;
        }

        // The value, however, must be convertible to real
        if (!v.convertInPlace(Value::Type::Real)) {
            errors.push(PacBadFrequency{});
            if (debug>0) {
                Simulator::dbg() << "Frequency value cannot be converted to real.\n";
            }
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
        fillMatrix(circuit, maxharm_, jacSpec, pacMatrix, omega);

        // Fill RHS
        zero(pacSolution);
        // Go through sources, go through excitation harmonics
        for (auto& exc : excitations) {
            // Collect source excitation unknowns
            auto [pe, ne] = exc.source->sourceExcitation(circuit);

            // Unity excitation accounting for $mfactor
            auto unity = exc.source->scaledUnityExcitation();

            for (size_t i = 0; i < exc.harm.size(); i++) {
                auto row = exc.harm[i];
                auto mag = unity*exc.value[i];
                pacSolution[pe*nf+row] += mag;
                pacSolution[ne*nf+row] -= mag;
            }
        }

        if (debug>=100) {
            Simulator::dbg() << "Linear system at frequency " << frequency << "\n";
            pacMatrix.dump(Simulator::dbg(), dataWithoutBucket(pacSolution, nf));
            Simulator::dbg() << "\n";
        }

        // Check if matrix entries are finite, no need to check RHS
        // since we loaded it without any computation (i.e. we only used mag and phase)
        if (options.matrixcheck && !pacMatrix.isFinite(true, true, errors)) {
            errors.push(PacMatrixError{});
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
                errors.push(PacMatrixError{});
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
                errors.push(PacMatrixError{});
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
                errors.push(PacSingularMatrix{});
                error = true;
                break;
            }
        }

        // Solve
        if (!cxSolver_->solve(dataWithoutBucket(pacSolution, nf), errors)) {
            errors.push(PacMatrixError{});
            if (debug>2) {
                Simulator::dbg() << "Failed to solve factored system.\n";
            }
            error = true;
            break;
        }
        // Set bucket to 0
        VectorView(pacSolution, nf) = Complex(0.0, 0.0);

        if (options.solutioncheck && !pacMatrix.isFinite(dataWithoutBucket(pacSolution, nf), true, true, errors)) {
            errors.push(PacSolutionNotFinite{});
            if (options.smsig_debug) {
                Simulator::dbg() << "A solution entry is not finite. Solver failed.\n";
            }
            error = true;
            break;
        }

        // Dump solution point
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
        Simulator::dbg() << "PAC frequency sweep " << (finished ? "completed" : "exited prematurely") << ".\n";
    }

    if (finished) {
        co_yield CoreState::Finished;
    } else {
        errors.push(PacSweepAborted{frequency});
        co_yield CoreState::Aborted;
    }
}

bool PACCore::run(bool continuePrevious, ErrorConsumer& errors) {
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

void PACCore::dump(std::ostream& os) const {
    AnalysisCore::dump(os);
    os << "  Results\n";
    auto n = circuit.unknownCount();
    auto nf = 2*maxharm_+1;
    for(decltype(n) i=1; i<=n; i++) {
        auto rn = circuit.reprNode(i);
        for(decltype(nf) j=0; j<nf; j++) {
            auto c = pacSolution.data()[i*nf+j];
            os << "    " << rn->name() << ", harm=" << (static_cast<Int>(j)-maxharm_) <<  " : " << c.real();
            if (c.imag()>=0) {
                os << "+";
            }
            os << c.imag();
            os << "i\n";
        }
    }
}

}
