#include <numbers>
#include <complex>
#include "corehbac.h"
#include "spurs.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

// Default parameters
HBACParameters::HBACParameters() {
    hbParams.write = 0;
    outspur = ValueVector();
}

template<> int Introspection<HBACParameters>::setup() {
    registerMember(from);
    registerMember(to);
    registerMember(step);
    registerMember(mode);
    registerMember(points);
    registerMember(values);
    registerMember(outspur);
    registerMember(maxharm);
    registerMember(maxfreq);
    registerMember(write);
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
    
    return 0;
}
instantiateIntrospection(HBACParameters);

class HBACUnknownNameResolver : public NameResolver {
public:
    HBACUnknownNameResolver(Circuit& circuit, size_t nf) : circuit(circuit), nf(nf) {};

    virtual Id operator()(MatrixEntryIndex u) {
        return circuit.reprNode(u/nf+1)->name();
    };

private:
    Circuit& circuit;
    size_t nf;
};

HBACCore::HBACCore(
    OutputDescriptorResolver& parentResolver, HBACParameters& params, HBCore& hbCore, 
    Circuit& circuit, CommonData& commons, 
    KluBlockSparseComplexMatrix& jacSpec, 
    VectorRepository<Complex>& hbSolution, 
    KluBlockSparseComplexMatrix& acMatrix, Vector<Complex>& acSolution
) : AnalysisCore(parentResolver, circuit, commons), params(params), outfile(nullptr), hbCore_(hbCore), 
    jacSpec(jacSpec), 
    hbSolution(hbSolution), 
    acMatrix(acMatrix), acSolution(acSolution) {
}

HBACCore::~HBACCore() {
    delete outfile;
}

bool HBACCore::resolveOutputDescriptors(bool strict, Status& s) {
    // Clear output sources
    outputSources.clear();
    // Resolve output descriptors
    bool ok = true; 
    auto nStoredSpurs = spurIndices.size();
    auto nSpurs = spurs_.smsigFreq().size();

    for (auto it = outputDescriptors.cbegin(); it != outputDescriptors.cend(); ++it) {
        Node *node;
        Instance *inst;
        switch (it->type) {
        case OutdSolComponent:
            for(decltype(nSpurs) i=0; i<nStoredSpurs; i++) {
                Id name = std::string(it->id)+";"+suffixes[i];
                ok = addComplexVarOutputSource(strict, it->id, acSolution, nSpurs, spurIndices[i], name, s);
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

bool HBACCore::addCoreOutputDescriptors(Status& s) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    if (!addOutputDescriptor(OutputDescriptor(OutdFrequency, "frequency"))) {
        s.set(Status::Analysis, std::string("Failed to add output descriptor for frequency."));
        return false;
    }
    return true;
}

bool HBACCore::addDefaultOutputDescriptors(Status& s) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    if (savesCount==0) {
        return addAllUnknowns(PTSave("default", Id(), Id()), s);
    }
    return true;
}

bool HBACCore::initializeOutputs(Id name, Status& s) {
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
        outfile->setPlotname("HBAC Small Signal Analysis");
    }
    outfile->prologue();

    return true;
}

bool HBACCore::finalizeOutputs(Status& s) {
    if (outfile) {
        outfile->epilogue();
        delete outfile;
        outfile = nullptr;
    }
    return true;
}

bool HBACCore::deleteOutputs(Id name, Status& s) {
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
  
void HBACCore::constructSuffixes() {
    auto nf = spurs_.smsigFreq().size();
    suffixes.clear();
    for (auto i : spurIndices) {
        auto w = spurs_.smsigFreqWeights(i);
        std::string s;
        for (size_t k = 0; k < w.n(); k++) {
            if (k > 0) s += ',';
            s += std::to_string(w[k]);
        }
        suffixes.push_back(std::move(s));
    }
}


// Construct omega vector: omega[n] = 2*pi*(f + f_n)
// where f_n = smsigFreq[n] is the signed spur frequency from the HB operating point.
// f    - small-signal input frequency (Hz)
// omega - output vector, resized to nf (number of spurs)
void HBACCore::computeOmega(Real f) {
    auto& smsigFreq = spurs_.smsigFreq();
    auto nf = smsigFreq.size();
    for (size_t n = 0; n < nf; n++) {
        omega[n] = 2.0 * std::numbers::pi * (f + smsigFreq[n]);
    }
}

// Fill one (p,q) subblock of the conversion matrix H(omega).
//
// The (n,m) entry of the subblock is:
//   h_nm = G[k] + j*(omega+omega_n)*C[k]
// where k is the Jacobian harmonic index determined by the mixing stencil,
// and omega is the small-signal frequency for output row n.
//
// Parameters:
//   G    - Fourier coefficients [G_k]_pq of the resistive Jacobian,
//          indexed by 0-based Jacobian frequency index
//          only positive part of the spectrum
//   C    - Fourier coefficients [C_k]_pq of the reactive Jacobian,
//          indexed by 0-based Jacobian frequency index
//          only positive part of the spectrum
//   omega - small-signal frequencies 2 pi (f+f_n), one per output row n
//   block - (p,q) subblock of H(omega) to fill, size nf x nf
//           column-major assumed
void HBACCore::fillDenseBlock(
    const VectorView<Complex>& G,
    const VectorView<Complex>& C,
    const Vector<Real>& omega,
    DenseMatrixView<Complex>& block
) {
    auto& stencil = spurs_.mixingStencil();
    auto nf = stencil.nRows();
    
    // G.dump(std::cout);
    // C.dump(std::cout);
    // std::cout << "\n";

    // Outer loop over columns (assume column major matrix)
    auto* o = &omega.at(0);
    for (size_t m = 0; m < nf; m++) {
        // Omega is common for the whole row
        auto [start, end] = spurs_.rowRange(m);
        auto om = o + start;
        auto p1 = &block.at(start, m);
        auto jacIndex = &stencil.at(start, m);;
        
        // TODO: remove
        // start=0;
        // end=nf;

        for(size_t n = start; n < end; n++) {
            // std::cout << "col " << m << " row " << n << " ji=" << *jacIndex << "\n";
            if (*jacIndex>=0) {
                auto k = *jacIndex;
                *p1 = G[k] + Complex(0.0, *om) * C[k];
                // std::cout << "  " << g << " " << c << "\n";
            }
            p1++;
            jacIndex++;
            om++;
        }

        // block.dump(std::cout);
        // std::cout << "\n";
    }
}

void HBACCore::fillMatrix() {
    acMatrix.zero();
    for(auto& pos : circuit.sparsityMap().positions()) {
        // Jacobian spectrum block, column 1 is G, column 2 is C
        auto [jacSpecBlock, found1] = jacSpec.block(pos);
        auto G = jacSpecBlock.column(0);
        auto C = jacSpecBlock.column(1);

        // TODO: remove
        // if (pos.first==3 && pos.second==3) {
        //     int a=1;
        // }

        // Get AC matrix block
        auto [block, found2] = acMatrix.block(pos);
        
        // Fill block
        fillDenseBlock(G, C, omega, block);

        // std::cout << pos.first << " " << pos.second << "\n";
        // std::cout << "Jg: ";
        // G.dump(std::cout);
        // std::cout << "Jc: ";
        // C.dump(std::cout);
        // block.dump(std::cout);
    }
}

bool HBACCore::rebuild(Status& s) {
    clearError();

    auto& options = circuit.simulatorOptions().core();

    // Make a local copy of spurs structure
    spurs_ = Spurs(hbCore_.spurs());

    // Get maxharm and maxfreq
    Vector<Int> maxharm(spurs_.fundamentals().size());
    if (params.maxharm.isVector()) {
        // Vector maxharm
        if (params.maxharm.type()!=Value::Type::IntVec) {
            s.set(Status::BadArguments, "Maxharm vector must be an integer vector.");
            return false;
        }
        if (params.maxharm.size()!=spurs_.fundamentals().size()) {
            s.set(Status::BadArguments, "Maxharm vector size must match the number of fundamental frequencies.");
            return false;
        }
        maxharm = params.maxharm.val<IntVector>();
    } else {
        // Scalar maxharm
        if (params.maxharm.type()!=Value::Type::Int) {
            s.set(Status::BadArguments, "Maxharm scalar must be an integer.");
            return false;
        }
        maxharm.assign(spurs_.fundamentals().size(), params.maxharm.val<Int>());
    }
    auto maxfreq = params.maxfreq;
    
    // Prune spurs
    if (!spurs_.prune(maxharm, maxfreq, s)) {
        return false;
    }
    
    // Build mixing map
    if (!spurs_.buildMixingMap(options.smsig_debug>0, s)) {
        return false;
    }
    auto& stencil = spurs_.mixingStencil();
    auto nf = stencil.nRows();

    // Collect output spurs
    std::vector<int> newSpurIndices;
    if (params.outspur.type() == Value::Type::ValueVec) {
        // List of spurs: each element is a real frequency or integer weight vector
        // Empty list means all spurs
        if (params.outspur.size()==0) {
            // Empty list means all spurs
            auto nf = spurs_.smsigFreq().size();
            for(decltype(nf) i=0; i<nf; i++) {
                newSpurIndices.push_back(static_cast<int>(i));
            }
        } else {
            size_t cnt=0;
            for (const auto& v : params.outspur.val<ValueVector>()) {
                auto [ok, ndx] = spurs_.smsigFreqIndex(v);
                if (!ok) {
                    s.set(Status::BadArguments, "Output spur #"+std::to_string(cnt)+" not found.");
                    return false;
                }
                newSpurIndices.push_back(static_cast<int>(ndx));
                cnt++;
            }
        }
    } else {
        // Single spur: scalar real frequency or integer weight vector
        auto [ok, ndx] = spurs_.smsigFreqIndex(params.outspur);
        if (!ok) {
            s.set(Status::BadArguments, "Output spur not found.");
            return false;
        }
        newSpurIndices.push_back(static_cast<int>(ndx));
    }

    // No output spurs, error
    if (newSpurIndices.empty()) {
        s.set(Status::BadArguments, "No output spur given.");
        return false;
    }

    // Collect new spur signatures
    std::vector<std::vector<Int>> newSignatures;
    newSignatures.reserve(newSpurIndices.size());
    for (auto i : newSpurIndices) {
        auto w = spurs_.smsigFreqWeights(i);
        std::vector<Int> sig(w.n());
        for (size_t k = 0; k < w.n(); k++) {
            sig[k] = w[k];
        }
        newSignatures.push_back(std::move(sig));
    }

    // Check for change
    if (spurIndices.size()!=0) {
        if (newSignatures.size()!=spurSignatures.size()) {
            s.set(Status::BadArguments, "Output spurs are not allowed to change.");
            return false;
        }
        for(size_t i=0; i<spurSignatures.size(); i++) {
            if (newSignatures[i]!=spurSignatures[i]) {
                s.set(Status::BadArguments, "Output spurs are not allowed to change.");
                return false;
            }
        }
    }

    // Update spur indices and signatures
    spurIndices = std::move(newSpurIndices);
    spurSignatures = std::move(newSignatures);

    // Build suffixes
    constructSuffixes();

    // Jacobian spectral components
    if (!jacSpec.rebuild(circuit.sparsityMap(), circuit.unknownCount(), nf, 2, true)) {
        setError(HBACError::MatrixError);
        return false;
    }

    // AC analysis matrix
    if (!acMatrix.rebuild(circuit.sparsityMap(), circuit.unknownCount(), nf, nf)) {
        setError(HBACError::MatrixError);
        return false;
    }
    
    return true;
}

bool HBACCore::collectExcitations() {
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
                // Extract spurs
                auto [spurs, mags, phases] = inst->spur();

                // Check vector lengths
                auto nSpurs = spurs.size();
                if (mags.size()>nSpurs) {
                    setError(HBACError::MagLength);
                    errorInst = inst;
                    return false;
                }
                if (phases.size()>nSpurs) {
                    setError(HBACError::PhaseLength);
                    errorInst = inst;
                    return false;
                }

                if (nSpurs==0) {
                    continue;
                }

                excitations.push_back(std::move(Excitation(inst, {}, {})));
                for(decltype(nSpurs) i=0; i<nSpurs; i++) {
                    auto [ok, ndx] = spurs_.smsigFreqIndex(spurs[i]);
                    if (!ok) {
                        setError(HBACError::SpurNotFound);
                        errorInst = inst;
                        errorSpur = i;
                        return false;
                    }
                    auto mag = (mags.size()>i) ? mags[i] : 0.0;
                    auto ph = (phases.size()>i) ? phases[i] : 0.0;
                    
                    double re = mag*std::cos(ph*std::numbers::pi/180);
                    double im = mag*std::sin(ph*std::numbers::pi/180);
                    
                    excitations.back().spur.push_back(ndx);
                    excitations.back().value.push_back(Complex(re, im));
                }
            }
        }
    }

    return true;
}

CoreCoroutine HBACCore::coroutine(bool continuePrevious) {
    acMatrix.setAccounting(circuit.tables().accounting());
    
    clearError();

    auto& options = circuit.simulatorOptions().core();
    Int debug = options.smsig_debug;
    
    auto n = circuit.unknownCount(); 
    auto& stencil = spurs_.mixingStencil();
    auto nf = stencil.nRows();

    // Colect excitations
    if (!collectExcitations()) {
        co_yield CoreState::Aborted;
    }

    // Make sure structures are large enough
    // One bucket for each spur
    acSolution.resize((n+1)*nf);

    // Frequencies near spurs
    omega.resize(nf);
    
    // Compute HB solution
    errorFreq = -1;
    if (params.hbParams.solve) {
        // Solve HB
        auto hbOk = hbCore_.run(continuePrevious);
        if (!hbOk) {
            setError(HBACError::HBError);
            co_yield CoreState::Aborted;
        }
    } else {
        // Evaluate HB at nodeset
        if (!hbCore_.evaluateAtNodeset()) {
            setError(HBACError::HBError);
            co_yield CoreState::Aborted;
        }
    }

    // Collect frequency-domain Jacobians
    hbCore_.getFrequencyDomainJacobians(jacSpec);

    // Check if the Jacobians are finite
    if (options.matrixcheck && !jacSpec.isFinite(true, true)) {
        setError(HBACError::MatrixError);
        if (debug>0) {
            Simulator::dbg() << "A frequency-domain Jacobian matrix entry is not finite.\n";
        }
        co_yield CoreState::Aborted;
    }

    if (debug>0) {
        Simulator::dbg() << "Starting HBAC small-signal analysis.\n";
    }
    
    // Create sweeper
    ScalarSweep sweeper;
    if (!sweeper.setup(params, errorStatus)) {
        setError(HBACError::Sweeper);
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
            setError(HBACError::SweepCompute);
            error = true;
            break;
        }

        // The value, however, must be convertible to real
        if (!v.convertInPlace(Value::Type::Real, errorStatus)) {
            setError(HBACError::BadFrequency);
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
        fillMatrix();

        // Fill RHS
        zero(acSolution);
        // Go through sources, go through spurs
        for (auto& exc : excitations) {
            // Collect source excitation unknowns
            auto [pe, ne] = exc.source->sourceExcitation(circuit);

            // Unity excitation accounting for $mfactor
            auto unity = exc.source->scaledUnityExcitation();

            for (size_t i = 0; i < exc.spur.size(); i++) {
                // Get spur complex magnitude
                auto spurIndex = exc.spur[i];
                auto mag = unity*exc.value[i];
                acSolution[pe*nf+spurIndex] += mag;
                acSolution[ne*nf+spurIndex] -= mag;
            }
        }

        if (debug>=100) {
            Simulator::dbg() << "Linear system at frequency " << frequency << "\n";
            acMatrix.dump(Simulator::dbg(), dataWithoutBucket(acSolution, nf)); 
            Simulator::dbg() << "\n";
        }

        // Check if matrix entries are finite, no need to check RHS 
        // since we loaded it without any computation (i.e. we only used mag and phase)
        if (options.matrixcheck && !acMatrix.isFinite(true, true)) {
            setError(HBACError::MatrixError);
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
                setError(HBACError::MatrixError);
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
                setError(HBACError::MatrixError);
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
                setError(HBACError::SingularMatrix);
                error = true;
                break;
            }
        }

        // Solve
        if (!acMatrix.solve(dataWithoutBucket(acSolution, nf))) {
            setError(HBACError::MatrixError);
            if (debug>2) {
                Simulator::dbg() << "Failed to solve factored system.\n";
            }
            error = true;
            break;
        }
        // Set bucket to 0
        for(decltype(nf) i=0; i<nf; i++) {
            acSolution[i] = 0.0;
        }

        // Print solution
        // for(decltype(n) i=1; i<=n; i++) {
        //     auto rn = circuit.reprNode(i);
        //     for(decltype(nf) j=0; j<nf; j++) {
        //         std::cout << rn->name() << ";" << (j) << " : " << acSolution[i*nf+j] << "\n";
        //     }
        // }

        if (options.solutioncheck && !acMatrix.isFinite(dataWithoutBucket(acSolution, nf), true, true)) {
            setError(HBACError::SolutionError);
            if (options.smsig_debug) {
                Simulator::dbg() << "A solution entry is not finite. Solver failed.\n";
            }
            error = true;
            break;
        }
        
        // Dump solution point
        if (params.write && !Simulator::noOutput() && outfile) {
            outfile->addPoint();
        }
        
        finished = sweeper.advance();
        
        setProgress(sweeper.at(), frequency);
    } while (!finished && !error);
    
    if (debug>0) {
        Simulator::dbg() << "HBAC frequency sweep " << (finished ? "completed" : "exited prematurely") << ".\n";
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

bool HBACCore::run(bool continuePrevious) {
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

bool HBACCore::formatError(Status& s) const {
    auto nr = HBACUnknownNameResolver(circuit, spurs_.smsigFreq().size());
    std::stringstream ss;
    ss << std::scientific << std::setprecision(4);
    
    // First, handle AnalysisCore errors
    if (lastError!=Error::OK) {
        AnalysisCore::formatError(s);
        return false;
    }
    
    // Then handle ACCore errors
    switch (lastHBACError) {
        case HBACError::Sweeper:
        case HBACError::SweepCompute:
            s.set(errorStatus);
            break;
        case HBACError::MatrixError:
            acMatrix.formatError(s, &nr);
            break;
        case HBACError::SolutionError:
            acMatrix.formatError(s, &nr);
            s.extend("Solution component is not finite.");
            break;
        case HBACError::HBError:
            hbCore_.formatError(s);
            break;
        case HBACError::SingularMatrix:
            s.set(Status::Analysis, "Matrix is close to singular.");
            break;
        case HBACError::BadFrequency:
            s.set(Status::Analysis, "Frequency value cannot be converted to real.");
            break;
        case HBACError::MagLength:
            s.set(Status::Analysis, "smag length exceeds spur length for instance '"+std::string(errorInst->name())+"'.");
            break;
        case HBACError::PhaseLength:
            s.set(Status::Analysis, "sphase length exceeds spur length for instance '"+std::string(errorInst->name())+"'.");
            break;
        case HBACError::SpurNotFound:
            s.set(Status::Analysis, "Spur #"+std::to_string(errorSpur)+" specified for instance '"+std::string(errorInst->name())+"' not found.");
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

void HBACCore::dump(std::ostream& os) const {
    AnalysisCore::dump(os);
    os << "  Results\n";
    auto n = circuit.unknownCount();
    auto nf = spurs_.smsigFreq().size();
    for(decltype(n) i=1; i<=n; i++) {
        for(decltype(nf) j=0; j<nf; j++) {
            auto rn = circuit.reprNode(i);
            auto c = acSolution.data()[i];
            os << "    " << rn->name() << ", spur=" << spurs_.smsigFreq()[j] <<  " : " << c.real();
            if (c.imag()>=0) {
                os << "+";
            }
            os << c.imag();
            os << "i\n";
        }
    }
}

}
