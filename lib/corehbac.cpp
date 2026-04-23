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
}

template<> int Introspection<HBACParameters>::setup() {
    registerMember(from);
    registerMember(to);
    registerMember(step);
    registerMember(mode);
    registerMember(points);
    registerMember(values);
    registerMember(opsolve);
    registerMember(writehb);
    registerMember(outspur);
    registerMember(write);
    registerNamedMember(hbParams.freq, "freq");
    registerNamedMember(hbParams.nharm, "nharm");
    registerNamedMember(hbParams.immax, "immax");
    registerNamedMember(hbParams.truncate, "truncate");
    registerNamedMember(hbParams.samplefac, "samplefac");
    registerNamedMember(hbParams.nper, "nper");
    registerNamedMember(hbParams.sample, "sample");
    registerNamedMember(hbParams.shift, "shift");
    registerNamedMember(hbParams.nodeset, "nodeset");
    registerNamedMember(hbParams.store, "store");
    
    return 0;
}
instantiateIntrospection(HBACParameters);

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
    auto nSpurs = hbCore_.spurs().smsigFreq().size();

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
            name, outputDescriptors, outputSources,
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
    auto& spurs = hbCore_.spurs();
    auto nf = spurs.smsigFreq().size();
    suffixes.resize(nf);
    for (auto i : spurIndices) {
        auto w = spurs.smsigFreqWeights(i);
        std::string s;
        for (size_t k = 0; k < w.n(); k++) {
            if (k > 0) s += ',';
            s += std::to_string(w[k]);
        }
        suffixes[i] = std::move(s);
    }
}


// Construct omega vector: omega[n] = 2*pi*(f + f_n)
// where f_n = smsigFreq[n] is the signed spur frequency from the HB operating point.
// f    - small-signal input frequency (Hz)
// omega - output vector, resized to nf (number of spurs)
void HBACCore::computeOmega(Real f) {
    auto& smsigFreq = hbCore_.spurs().smsigFreq();
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
    auto& spurs = hbCore_.spurs();
    auto& stencil = spurs.mixingStencil();
    auto nf = stencil.nRows();

    // Outer loop over columns (assume column major matrix)
    auto* p = &block.at(0, 0);
    auto* jacIndex = &stencil.at(0, 0);
    for (size_t m = 0; m < nf; m++) {
        // Omega is common for the whole row
        auto* om = &omega.at(0);
        auto [start, end] = spurs.rowRange(m);
        auto p1 = p + start;
        for(size_t n = start; n < end; n++) {
            if (*jacIndex != Spurs::noJacIndex) {
                bool conjugated = (*jacIndex < 0);
                auto k = (conjugated ? (-*jacIndex) : *jacIndex) - 1;
                Complex g = conjugated ? std::conj(G[k]) : G[k];
                Complex c = conjugated ? std::conj(C[k]) : C[k];
                *p1 = g + Complex(0.0, *om) * c;
                p1++;
                jacIndex++;
                om++;
            }
        }
        p += nf;
    }
}

void HBACCore::fillMatrix() {
    acMatrix.zero();
    for(auto& pos : circuit.sparsityMap().positions()) {
        // Jacobian spectrum block, column 1 is G, column 2 is C
        auto [jacSpecBlock, found1] = jacSpec.block(pos);
        auto G = jacSpecBlock.column(0);
        auto C = jacSpecBlock.column(1);

        // Get AC matrix block
        auto [block, found2] = acMatrix.block(pos);
        
        // Fill block
        fillDenseBlock(G, C, omega, block);
    }
}

bool HBACCore::rebuild(Status& s) {
    clearError();

    auto& spurs = hbCore_.spurs();
    auto& stencil = spurs.mixingStencil();
    auto nf = stencil.nRows();

    // AC analysis matrix
    if (!acMatrix.rebuild(circuit.sparsityMap(), circuit.unknownCount(), nf, nf)) {
        setError(HBACError::MatrixError);
        return false;
    }
    
    return true;
}

// TODO: set solve parameter of hbCore_ to the opsolve parameter of hbac
// in analysis rebuild

// TODO: make list of sources every time core is invoked
//       somebody might sweep cs* parameters of sources

// TODO: if spurs change during sweep, error
//       if output spurs change, error

CoreCoroutine HBACCore::coroutine(bool continuePrevious) {
    acMatrix.setAccounting(circuit.tables().accounting());
    
    clearError();

    auto& options = circuit.simulatorOptions().core();
    Int debug = options.smsig_debug;
    
    auto n = circuit.unknownCount(); 
    auto& spurs = hbCore_.spurs();
    auto& stencil = spurs.mixingStencil();
    auto nf = stencil.nRows();

    // Make sure structures are large enough
    // One bucket for each spur
    acSolution.resize((n+1)*nf);
    
    // Compute HB solution
    errorFreq = -1;
    if (params.opsolve) {
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
        double omega = 2*std::numbers::pi*frequency;

        if (debug>0) {
            ss.str(""); ss << frequency;
            Simulator::dbg() << "frequency=" << ss.str() << "\n";
        }

        // Construct matrix
        computeOmega(frequency);
        fillMatrix();

        // Fill RHS
        // TODO

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

        // Solve, set bucket to 0.0
        if (!acMatrix.solve(dataWithoutBucket(acSolution, nf))) {
            setError(HBACError::MatrixError);
            if (debug>2) {
                Simulator::dbg() << "Failed to solve factored system.\n";
            }
            error = true;
            break;
        }
        acSolution[0] = 0.0;

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
    auto nr = UnknownNameResolver(circuit);
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
    auto& spurs = hbCore_.spurs();
    auto nf = spurs.smsigFreq().size();
    for(decltype(n) i=1; i<=n; i++) {
        for(decltype(nf) j=0; j<nf; j++) {
            auto rn = circuit.reprNode(i);
            auto c = acSolution.data()[i];
            os << "    " << rn->name() << ", spur=" << spurs.smsigFreq()[j] <<  " : " << c.real();
            if (c.imag()>=0) {
                os << "+";
            }
            os << c.imag();
            os << "i\n";
        }
    }
}

}
