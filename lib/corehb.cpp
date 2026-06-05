#include <vector>
#include <algorithm>
#include <numbers>
#include "core.h"
#include "corehb.h"
#include "hmtpsrc.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

Id HBCore::truncateBox = Id::createStatic("box");
Id HBCore::truncateDiamond = Id::createStatic("diamond");
Id HBCore::truncateHybrid = Id::createStatic("hybrid");

Id HBCore::sampleUniform = Id::createStatic("uniform");
Id HBCore::sampleRandom = Id::createStatic("random");
Id HBCore::sampleMixed = Id::createStatic("mixed");

HBParameters::HBParameters() {
    truncate = HBCore::truncateHybrid;
    sample = HBCore::sampleUniform;
}

template<> int Introspection<HBParameters>::setup() {
    registerMember(freq);
    registerMember(nharm);
    registerMember(immax);
    registerMember(truncate);
    registerMember(samplefac);
    registerMember(tstart);
    registerMember(nper);
    registerMember(sample);
    registerMember(shift);
    registerMember(write);
    registerMember(nodeset);
    registerMember(store);
    
    return 0;
}
instantiateIntrospection(HBParameters);

class HbUnknownNameResolver : public NameResolver {
public:
    HbUnknownNameResolver(Circuit& circuit, size_t nb) : circuit(circuit), nb(nb) {};

    virtual Id operator()(MatrixEntryIndex u) {
        return circuit.reprNode(u/nb+1)->name();
    };

private:
    Circuit& circuit;
    size_t nb;
};

HBCore::HBCore(
    OutputDescriptorResolver& parentResolver, HBParameters& params, Circuit& circuit, CommonData& commons,
    KluBlockSparseRealMatrix& jacColoc, KluBlockSparseRealMatrix& jacobian, VectorRepository<double>& solution
) : AnalysisCore(parentResolver, circuit, commons),
    lastHbError(HBError::OK),
    homotopySteps(0),
    jacColoc(jacColoc),
    bsjac(jacobian),
    solution(solution),
    continueState(nullptr),
    outfile(nullptr),
    converged_(false),
    firstBuild(true),
    params(params),
    nrSolver(circuit, commons, jacColoc, jacobian, solution, solutionFD, 
             timepoints, spurs_, 
             APFT, IAPFT, OmegaGamma, GammaInvColumnMajor, nrSettings) {
};

HBCore::~HBCore() {
    delete outfile;
}

bool HBCore::addCoreOutputDescriptors(Status& s) {
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

bool HBCore::addDefaultOutputDescriptors(Status& s) {
    // If output is suppressed, skip all this work
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    if (savesCount==0) {
        return addAllUnknowns(PTSave("default", Id(), Id()), s);
    }
    return true;
}

bool HBCore::resolveOutputDescriptors(bool strict, Status& s) {
    // Clear output sources
    outputSources.clear();
    // Resolve output descriptors
    bool ok = true;
    for (auto it = outputDescriptors.cbegin(); it != outputDescriptors.cend(); ++it) {
        Node *node;
        Instance *inst;
        // TODO: handle output variables someday
        switch (it->type) {
        case OutdSolComponent:
            ok = addComplexVarOutputSource(strict, it->id, outputPhasors, 1, 0, it->name, s); 
            break;
        case OutdFrequency:
            outputSources.emplace_back(&outputFreq, it->name);
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

bool HBCore::initializeOutputs(const std::string& name, Status& s) {
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
        outfile->setPlotname("Harmonic Balance Analysis");
    }
    outfile->prologue();

    return true;
}

bool HBCore::finalizeOutputs(Status& s) {
    if (outfile) {
        outfile->epilogue();
        delete outfile;
        outfile = nullptr;
    }

    // Write solution to repository if analysis is OK
    if (converged_ && params.store.length()>0) {
        auto sol = circuit.newStoredSolution("hb", params.store);
        sol->setNames(circuit);
        sol->setCxValues(solutionFD);
        sol->setHBAuxData(spurs_, timepoints);
    }
    return true;
}

bool HBCore::deleteOutputs(Id name, Status& s) {
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

bool HBCore::storeState(size_t ndx, bool storeDetails) {
    auto& repo = coreStates.at(ndx);
    // Store current solution as annotated solution
    if (storeDetails) {
        repo.solution.setNames(circuit);
    } else {
        repo.solution.clearNames();
    }
    
    // Store solution in frequency domain (complex spectrum)
    repo.solution.setCxValues(solutionFD);
    
    // Store frequencies
    repo.solution.setHBAuxData(spurs_, timepoints);
    
    // Stored state is coherent and valid
    repo.coherent = true;
    repo.valid = true;
    return true;
}

bool HBCore::restoreState(size_t ndx) {
    auto& state = coreStates.at(ndx);
    if (state.valid) {
        // State is valid
        continueState = &state;
        return true;
    } else { 
        // Nothing to restore, do not use continuation mode
        return false;
    }
}

// Analysis asks cores if they request a rebuild. 
// HB core replies that it does 
// - if this is the first build
// - TODO: if previously it was used in evaluate mode
// - if the set of frequencies changes. 
// Along with changed set of frequencies this function recomputes
// - colocation points
// - transform matrices
std::tuple<bool, bool> HBCore::requestsRebuild(Status& s) {
    // First build, nothing to compare to
    if (firstBuild) {
        oldParams = params;
        return std::make_tuple(true, true);
    }

    // Did parameters that affect the set of frequencies and the colocation timepoints change
    bool needsRebuild = 
        oldParams.freq != params.freq ||
        oldParams.nharm != params.nharm ||
        oldParams.immax != params.immax ||
        oldParams.truncate != params.truncate || 
        oldParams.samplefac != params.samplefac ||
        oldParams.nper != params.nper || 
        oldParams.sample != params.sample ||
        oldParams.shift != params.shift ||
        oldParams.solve != params.solve;
    oldParams = params;
    return std::make_tuple(true, needsRebuild);
}

bool HBCore::buildGrid(Status& s) {
    auto n = params.freq.size();

    auto& options = circuit.simulatorOptions().core();
    auto debug = options.hb_debug>2;
    
    if (params.freq.size()<1) {
        s.set(Status::BadArguments, "freq must have at least one component.");
        return false;
    }
    
    // Check freq
    for(decltype(n) i=0; i<n; i++) {
        if (params.freq[i]==0.0) {
            s.set(Status::BadArguments, "Zero frequency should not be specified explicitly.");
            return false;
        }
    }

    // Harmonics count
    std::vector<Int> nHarmonics;

    // If nharm is a scalar, construct a vector
    if (params.nharm.type()==ValueType::Int) {
        auto nScalar = params.nharm.val<Int>();
        if (nScalar<=0) {
            s.set(Status::BadArguments, "nharm must be >0.");
            return false;
        }
        nHarmonics.resize(n, nScalar);
    } else if (params.nharm.type()==ValueType::IntVec) {
        auto& nVector = params.nharm.val<IntVector>();
        for(auto nh : nVector) {
            if (nh<=0) {
                s.set(Status::BadArguments, "nharm components must be >0.");
                return false;
            }
        }
        auto nharmCount = nVector.size();
        if (nharmCount!=n) {
            s.set(Status::BadArguments, "Number of nharm components must match number of freq components.");
            return false;
        }
        nHarmonics = nVector;
    } else {
        s.set(Status::BadArguments, "nharm must be an integer or an integer vector.");
        return false;
    }

    if (!(
        params.truncate==HBCore::truncateBox || 
        params.truncate==HBCore::truncateDiamond ||
        params.truncate==HBCore::truncateHybrid
    )) {
        s.set(Status::BadArguments, "Unknown spectrum truncation method.");
        return false;
    }

    if (!spurs_.build(params.freq, nHarmonics, params.immax, params.truncate==HBCore::truncateHybrid, debug, s)) {
        return false;
    }

    if (spurs_.spectrum().size()<2) {
        s.set(Status::BadArguments, "Too few frequencies in spectrum.");
        return false;
    }

    return true;
}

// Called after build
bool HBCore::evaluateAtNodeset() {
    clearError();

    auto& options = circuit.simulatorOptions().core();
    nrSettings = NRSettings {
        .debug = options.nr_debug, 
        .matrixCheck = bool(options.matrixcheck), 
    };

    // Copy from forces slot 1 to solution vector
    auto n = circuit.unknownCount();
    auto nt = timepoints.size();
    solution.upsize(2, n*nt);
    solution.vector() = nrSolver.forces(1).unknownValue_;

    // Disable forces
    nrSolver.enableForces(0, false);
    nrSolver.enableForces(1, false);
    
    // Rebuild NR solver structures
    if (!nrSolver.rebuild(n*nt)) {
        setError(HBError::SolverBuild);
        return false;
    }

    // Initialize NR solver (continue previous)
    if (!nrSolver.initialize(true)) {
        setError(HBError::SolverError);
        return false;
    }

    // Run evaluation (continue previous)
    if (!nrSolver.evaluate(true)) {
        setError(HBError::SolverError);
        return false;
    }

    return true;
}

bool HBCore::getFrequencyDomainJacobians(KluBlockSparseComplexMatrix& jacSpec, const Spurs& prunedSpurs) {
    // Assumes evaluation was performed, writes frequency domin jacobians to jacSpec
    auto nt = timepoints.size();
    auto nf = spurs_.smsigFreq().size();
    auto dcIndex = spurs_.dcIndex();
    auto nfp = nf-dcIndex; // Number of nonnegative frequencies

    // Pruned spurs
    auto& fullSmsigFreqIndex = prunedSpurs.fullSmsigFreqIndex();
    auto prunedDcIndex = prunedSpurs.dcIndex();

    // Scratchpad for frequency domain spectrum
    Vector<Complex> GFullJac(nfp);
    Vector<Complex> CFullJac(nfp);

    // Go through all dense blocks
    for(auto& pos : circuit.sparsityMap().positions()) {
        // Get block position (for debugging), make position 0-based
        auto [i, j] = pos;
        i--;
        j--;

        // Get dense block with Jacobian values at colocation points
        auto [colocBlock, found1] = jacColoc.block(pos);
        auto gCol = colocBlock.column(0);
        auto cCol = colocBlock.column(1);

        // Dump collocation points and time-domain Jacobian columns in numpy format
        // {
        //     std::cout << std::scientific << std::setprecision(15);
        //     std::cout << "# Block (" << pos.first << ", " << pos.second << ")\n";
        //     std::cout << "t = np.array([";
        //     for (size_t k = 0; k < nt; k++) {
        //         if (k > 0) std::cout << ", ";
        //         std::cout << timepoints[k];
        //     }
        //     std::cout << "])\n";
        //     std::cout << "gCol = np.array([";
        //     for (size_t k = 0; k < nt; k++) {
        //         if (k > 0) std::cout << ", ";
        //         std::cout << gCol.at(k);
        //     }
        //     std::cout << "])\n";
        //     std::cout << "cCol = np.array([";
        //     for (size_t k = 0; k < nt; k++) {
        //         if (k > 0) std::cout << ", ";
        //         std::cout << cCol.at(k);
        //     }
        //     std::cout << "])\n";
        // }
        
        // APFT on stored time-domain values
        // Start APFT result at imag part of first component
        // First component (DC) is stored in complex vector at dcIndex
        // APFT produces dc, f1real, f1imag, f2real, f2imag, ...
        // but we need   dc, 0, f1real, f1imag, f2real, f2imag, ...
        // We store APFT starting from imaginary part of dc component (after x): 
        //   x, dc, f1real, f1imag, f2real, f2imag, ...
        // them move dc: dc, 0, f1real, f1imag, f2real, f2imag, ...
        // auto gDest = VectorView(reinterpret_cast<double*>(&GCol.at(dcIndex))+1, nt, 1);
        // auto cDest = VectorView(reinterpret_cast<double*>(&CCol.at(dcIndex))+1, nt, 1);
        auto gDest = VectorView(reinterpret_cast<double*>(&GFullJac.at(0))+1, nt, 1);
        auto cDest = VectorView(reinterpret_cast<double*>(&CFullJac.at(0))+1, nt, 1);

        // Transform
        APFT.multiply(gCol, gDest);
        APFT.multiply(cCol, cDest);

        // Move DC from imag to real part, set imag part to 0
        // GCol.at(dcIndex) = GCol.at(dcIndex).imag();
        // CCol.at(dcIndex) = CCol.at(dcIndex).imag();
        GFullJac.at(0) = GFullJac.at(0).imag();
        CFullJac.at(0) = CFullJac.at(0).imag();

        // Divide by 2 all positive frequency components, except DC
        // Spectrum is two-sided, but HB computes a one-sided spectrum
        // for(decltype(nf) k=dcIndex+1; k<nf; k++) {
        for(decltype(nf) k=1; k<nfp; k++) {
            // GCol.at(k) /= 2;
            // CCol.at(k) /= 2;
            GFullJac.at(k) /= 2;
            CFullJac.at(k) /= 2;
        }

        // Get FD Jacobian dense block
        auto [fdBlock, found2] = jacSpec.block(pos);
        auto GCol = fdBlock.column(0);
        auto CCol = fdBlock.column(1);

        // Copy to frequency domain Jacobian block
        for(decltype(prunedDcIndex) k=0; k<fullSmsigFreqIndex.size(); k++) {
            auto fullIndex = fullSmsigFreqIndex[k];
            if (fullIndex>=dcIndex) {
                // Positive frequency, GfullJac and CFullJac contain only DC and positive frequencies
                GCol[k] = GFullJac[fullIndex-dcIndex];
                CCol[k] = CFullJac[fullIndex-dcIndex];
            } else {
                // Negative frequency, conjugate corresponding positive frequency
                GCol[k] = std::conj(GFullJac[dcIndex-fullIndex]);
                CCol[k] = std::conj(CFullJac[dcIndex-fullIndex]);
            }
        }
        
        // Conjugates for negative frequencies
        // for(decltype(dcIndex) k=1; k<=dcIndex; k++) {
        //     GCol.at(dcIndex-k) = std::conj(GCol.at(dcIndex+k));
        //     CCol.at(dcIndex-k) = std::conj(CCol.at(dcIndex+k));
        // }
        
        // Dump FD Jacobian columns in numpy format
        // {
        //     auto& freqs = spurs_.smsigFreq();
        //     std::cout << std::scientific << std::setprecision(15);
        //     std::cout << "# Block (" << pos.first << ", " << pos.second << ") FD jacobians\n";
        //     std::cout << "f = np.array([";
        //     for (size_t k = 0; k < nf; k++) {
        //         if (k > 0) std::cout << ", ";
        //         std::cout << freqs[k];
        //     }
        //     std::cout << "])\n";
        //     std::cout << "GCol = np.array([";
        //     for (size_t k = 0; k < nf; k++) {
        //         if (k > 0) std::cout << ", ";
        //         std::cout << GCol.at(k).real() << "+" << GCol.at(k).imag() << "j";
        //     }
        //     std::cout << "])\n";
        //     std::cout << "CCol = np.array([";
        //     for (size_t k = 0; k < nf; k++) {
        //         if (k > 0) std::cout << ", ";
        //         std::cout << CCol.at(k).real() << "+" << CCol.at(k).imag() << "j";
        //     }
        //     std::cout << "])\n";
        // }

    }

    return true;
}

bool HBCore::rebuild(Status& s) {
    clearError();

    // solve=0 ... evaluate, collect spurs and timepoints from stored solution, 
    //             set up solution from stored solution
    //             prepare for linearized circuit evaluation, do not build Jacobian

    // solve=1 ... prepare for solving the HB problem, build Jacobian

    auto& options = circuit.simulatorOptions().core();
    nrSettings = NRSettings {
        .debug = options.nr_debug, 
        .itlim = options.hb_itl, 
        .itlimCont = options.hb_itlcont, 
        .convIter = options.nr_conviter, 
        .residualCheck = bool(options.nr_residualcheck),  
        .dampingFactor = options.nr_damping, 
        .matrixCheck = bool(options.matrixcheck), 
        .rhsCheck = bool(options.rhscheck), 
        .solutionCheck = bool(options.solutioncheck)
    };
    nrSolver.setForcesFactor(0, options.nr_nsforce);
    nrSolver.setForcesFactor(1, options.nr_nsforce);

    // Get nodeset from repository
    AnnotatedSolution* solPtr = nullptr;
    String& solutionName = params.nodeset;
    if (solutionName.length()>0) {
        // Get solution from repository
        solPtr = circuit.storedSolution("hb", solutionName);
    }

    if (params.solve) {
        // Compute set of frequencies
        if (!buildGrid(s)) {
            return false;
        }

        // Compute colocation
        if (!buildColocation(s)) {
            return false;
        }
        
        // Recompute transforms
        if (!buildAPFT(s)) {
            return false;
        }
    } else {
        // Assume grid, colocation, and APFT are obtained from nodeset
        if (!solPtr) {
            s.set(Status::NotFound, "Nodeset not found.");
            return false;
        }

        // Copy spurs
        spurs_ = Spurs(solPtr->hbSpurs());

        // Copy colocation
        timepoints = solPtr->hbTimepoints();

        // Need APFT for transforming time-domain Jacobian to frequency-domain Jacobian
        if (!buildAPFT(s)) {
            return false;
        }
    }

    // Number of colocation points
    auto nt = timepoints.size();

    // Jacobian entries at colocation points, do not create structures for scalar access
    jacColoc.rebuild(circuit.sparsityMap(), circuit.unknownCount(), nt, 2, true);

    // Build these only if we want to solve the HB problem
    if (params.solve) {
        // HB Jacobian
        if (!bsjac.rebuild(circuit.sparsityMap(), circuit.unknownCount(), nt, nt)) {
            auto nb = timepoints.size();
            auto nr = HbUnknownNameResolver(circuit, nb);
            bsjac.formatError(s, &nr);
            return false;
        }
        // solutionFD - complex vector without bucket
        solutionFD.resize(spurs_.spectrum().size());
    }

    // Bind resistive residuals to 0-based subelement (0,0) 
    // Bind reactive residuals to 0-based subelement (0,1) 
    if (!circuit.bind(
        &jacColoc, Component::Real, MatrixEntryPosition(0, 0), 
        &jacColoc, Component::Real, MatrixEntryPosition(0, 1), 
        s
    )) {
        return false;
    }

    // Prepare nodesets
    auto strictforce = circuit.simulatorOptions().core().strictforce; 
    if (solutionName.length()>0) {
        // Solution from repository (obtained previously)
        if (!solPtr) {
            // No nodesets
            nrSolver.forces(1).clear();
            Simulator::wrn() << "Warning, solution '"+solutionName+"' not found. No user nodesets applied.\n";
        } else {
            // Nodesets from solution repository
            if (!nrSolver.setForces(1, *solPtr, strictforce)) {
                // Abort if strictforce is set
                if (strictforce) {
                    nrSolver.formatError(s);
                    return false;
                }
            }
        }
    } else {
        // No nodesets, clear slot
        nrSolver.forces(1).clear();
    }
    
    // Rebuild NR solver structures
    auto n = circuit.unknownCount();
    if (!nrSolver.rebuild(n*nt)) {
        s.set(Status::NonlinearSolver, "Failed to rebuild internal structures of nonlinear solver.");
        return false;
    }
    
    firstBuild = false;
    return true;
}

std::tuple<bool, bool> HBCore::runSolver(bool continuePrevious) {
    auto& options = circuit.simulatorOptions().core();
    // Assume no initial state given, start with standard initial point. 
    // Coherence information is set by an.cpp and homotopy. 
    // They are responsible for detecting topology changes/rebuilds. 
    // Homotopy sweeps parameters that do not cause topology changes/rebuilds. 
    bool runInContinueMode = false;
    // Handle continuation
    if (continuePrevious) {
        // Continue mode
        if (continueState &&
            continueState->valid && continueState->coherent &&
            continueState->solution.cxValues().size()==circuit.unknownCount()*timepoints.size() &&
            continueState->solution.hbSpurs().spectrum().size()==spurs_.spectrum().size()
        ) {
            // Continue a state
            // State is valid, coherent, and its lengths match those of the solver vectors
            // Restore current state
            solution.vector() = continueState->solution.values();
            runInContinueMode = true;
            // No forces applied
            nrSolver.enableForces(0, false);
            nrSolver.enableForces(1, false);
            if (options.hb_debug>1) {
                Simulator::dbg() << "HB using ordinary continue mode with stored analysis state.\n";
            }
            // Forced bypass is not allowed
            commons.requestForcedBypass = false;
        } else if (continueState && continueState->valid) {
            // Continue a state
            // Stored analysis state is valid, but not coherent with current circuit, 
            // its lengths may not match those of the solver vectors. 
            // Use forces to continue, but set no initial states vector nor initial solution. 
            // Ignore forces conflicts arising from stored solution. 
            // There should be no such conflicts as we are applying forces to nodes only, not node deltas. 
            nrSolver.setForces(0, continueState->solution, false);
            nrSolver.enableForces(0, true);
            // Disable user-specified forces (nodesets)
            nrSolver.enableForces(1, false);
            if (options.hb_debug>1) {
                Simulator::dbg() << "HB using forced continue mode with stored analysis state.\n";
            }
            // Forced bypass is not allowed
            commons.requestForcedBypass = false;
        } else {
            // Do not continue a state (either not provided or not valid)
            // Continue with whatever is in solution and states vector
            runInContinueMode = true;
            // No forces applied
            nrSolver.enableForces(0, false);
            nrSolver.enableForces(1, false);
            if (options.hb_debug>1) {
                Simulator::dbg() << "HB using ordinary continue mode with previous solution.\n";
            }
            // Forced bypass is not allowed
            commons.requestForcedBypass = false;
        }
        // Continue state is spent after first use
        continueState = nullptr;
    } else {
        // Continue mode not requested
        // Disable continuation forces in slot 0
        nrSolver.enableForces(0, false);

        // Apply forces specified by user in slot 1 (nodeset parameter)
        nrSolver.enableForces(1, true); 

        // Forced bypass is not allowed
        commons.requestForcedBypass = false;
        
        if (options.hb_debug>1) {
            Simulator::dbg() << "HB using standard initial solution with forced nodesets.\n";
        }
    }

    auto converged = nrSolver.run(runInContinueMode);
    auto abort = nrSolver.checkFlags(HBNRSolver::Flags::Abort);
    if (!converged || abort) {
        setError(HBError::SolverError);
    }

    return std::make_tuple(converged, abort);
}

Int HBCore::iterations() const {
    return nrSolver.iterations();
}

Int HBCore::iterationLimit(bool continuePrevious) const {
    return continuePrevious ? nrSettings.itlimCont : nrSettings.itlim;
}

CoreCoroutine HBCore::coroutine(bool continuePrevious) {
    initProgress(1, 0);

    clearError();
    
    auto& options = circuit.simulatorOptions().core();
    converged_ = false;
    bool leave = false;
    bool tried = false;
    auto debug = options.hb_debug;
    auto n = circuit.unknownCount(); 
    auto nb = timepoints.size();
    auto nf = spurs_.spectrum().size();

    // Make sure structures are large enough
    solution.upsize(2, n*nb);
    
    if (debug>0) {
        Simulator::dbg() << "Starting HB analysis.\n";
    }

    // Run solver (time domain formulation)
    // Initial plain HB
    auto skipinitial = options.hb_skipinitial;
    if (!skipinitial) {
        tried = true;
        std::tie(converged_, leave) = runSolver(continuePrevious);
        if (!converged_) {
            setError(HBError::InitialHB);
        }
        if (debug>0) {
            if (converged_) {
                Simulator::dbg() << "HB core algorithm converged in " << std::to_string(nrSolver.iterations()) << " NR iteration(s).\n";
            } else {
                Simulator::dbg() << "HB core algorithm failed to converge in " << std::to_string(nrSolver.iterations()) << " NR iteration(s).\n";
            }
        }
    }

    // Try homotopy
    homotopySteps = 0;
    if (!converged_ && !leave && options.hb_homotopy.size()>0) {
        Homotopy* homotopy;
        for(auto it : options.hb_homotopy) {
            if (it==Homotopy::src) {
                if (debug>0) {
                    Simulator::dbg() << "Trying source stepping.\n";
                }
                homotopy = new SourceStepping(circuit, *this);
            } else {
                if (debug>0) {
                    Simulator::dbg() << "Unknown homotopy '"+std::string(it)+"'.\n";
                }
                homotopy = nullptr;
            }
            if (!homotopy) {
                continue;
            }
            // Run
            tried = true;
            std::tie(converged_, leave) = homotopy->run();
            homotopySteps += homotopy->stepCount();
            if (debug>0) {
                if (converged_) {
                    Simulator::dbg() << "Homotopy converged in " << std::to_string(homotopy->stepCount()) << " step(s).\n";
                } else {
                    Simulator::dbg() << "Homotopy failed to converge in " << std::to_string(homotopy->stepCount()) << " step(s).\n";
                }
            }
            delete homotopy;
            if (leave || converged_) {
                break;
            }
        }
        if (!converged_) {
            setError(HBError::Homotopy);
        }
    }

    if (!leave) {
        // Did not leave early
        if (!tried) {
            // No algorithm tried
            setError(HBError::NoAlgorithm);
        } else if (converged_) {
            // Tried and converged, fill solutionFD and outvec, write results
            if (outfile && params.write) {
                // Collect results for one frequency, need a slot for ground
                outputPhasors.upsize(1, n+1);
                auto outvec = outputPhasors.data();
                // Set ground unknown to zero
                outvec[0] = 0.0;
                // Go through frequencies
                for(decltype(nf) k=0; k<nf; k++) {
                    outputFreq = spurs_.spectrum()[k];
                    // Go through unknowns, fill outvec entries
                    for(decltype(n) i=0; i<n; i++) {
                        outvec[i+1] = solutionFD[i*nf+k];
                    }                    
                    // Dump values at current frequency to output
                    outfile->addPoint();
                }
            }
        }
    } else {
        // Leaving early, did not converge
        // Add a status message one level higher
        converged_ = false;
    }
    
    setProgress(1);

    // std::cout << "spectrum\n";
    // int i=0;
    // for(auto it : solutionFD) {
    //     std::cout << "  " << i << " " << it << "\n";
    //     i++;
    // }

    // nrSolver.dumpSolution(std::cout, solution.data(), "  ");

    // HB analysis can only Abort or Finish
    if (converged_) {
        co_yield CoreState::Finished;
    } else {
        co_yield CoreState::Aborted;
    }
}

bool HBCore::run(bool continuePrevious) {
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


bool HBCore::formatError(Status& s) const {
    auto nb = timepoints.size();
    auto nr = HbUnknownNameResolver(circuit, nb);
    std::stringstream ss;
    ss << std::scientific << std::setprecision(4);

    // Delegate to NRSolver (which in turn delegates to KluMatrix)
    auto solverError = nrSolver.formatError(s, &nr);
    
    // First, handle AnalysisCore errors
    if (lastError!=Error::OK) {
        AnalysisCore::formatError(s);
        return false;
    }
    
    // Then handle HBCore errors
    switch (lastHbError) {
        case HBError::InitialHB:
            s.extend("Initial HB analysis failed.");
            return false;
        case HBError::Homotopy:
            s.set(Status::Analysis, "Homotopy failed, "+std::to_string(homotopySteps)+" step(s) tried.");
            return false;
        case HBError::NoAlgorithm:
            s.set(Status::Analysis, "No HB algorithm tried."); 
            return false;
        case HBError::NoNodeset:
            s.set(Status::Analysis, "Nodeset not found."); 
            return false;
        case HBError::SolverBuild:
            s.set(Status::NonlinearSolver, "Failed to rebuild internal structures of nonlinear solver.");
            return false;
        case HBError::SolverError:
            nrSolver.formatError(s, &nr);
            return false;
    }
    return true;
}

void HBCore::dump(std::ostream& os) const {
    AnalysisCore::dump(os);
    os << "  Results\n";
    auto n = circuit.unknownCount();
    auto nf = spurs_.spectrum().size();
    for(decltype(n) i=0; i<n; i++) {
        auto rn = circuit.reprNode(i);
        for(decltype(nf) k=0; k<nf; k++) {
            auto c = solutionFD[i*nf+k];
            os << "    " << rn->name() << "@" << spurs_.spectrum()[k] << "Hz : " << c.real();
            if (c.imag()>=0) {
                os << "+";
            }
            os << c.imag();
            os << "i\n";
        }
    }
}

bool HBCore::test() {
    Status s;
    bool ok = true;

    HBParameters p;
    p.freq = {1000, 100000};
    p.nharm = 4;
    p.truncate = "diamond";
    p.sample = "random";
    p.samplefac = 4;

    // Dummy strutures
    OutputDescriptorResolver dummyResolver;
    KluBlockSparseRealMatrix jacColoc;
    KluBlockSparseRealMatrix bsjac;
    VectorRepository<double> sol;
    ParserTables tab;
    Circuit dummyCircuit(tab);
    CommonData dummyCommons;

    HBCore hb(dummyResolver, p, dummyCircuit, dummyCommons, jacColoc, bsjac, sol);

    if (ok && !hb.buildGrid(s)) {
        ok = false;
        std::cout << "Failed to build grid: " << s.message() << "\n";
    }
    
    if (ok && !hb.buildColocation(s)) {
        ok = false;
        std::cout << "Failed to select colocation points: " << s.message() << "\n";
    } 
    
    if (!hb.buildAPFT(s)) {
        ok = false;
        std::cout << "Failed to build APFT: " << s.message() << "\n";
    }

    if (ok) {
        double delta;
        auto n = hb.timepoints.size();
        DenseMatrix<double> result(n, n);

        // APFT*IAPFT
        hb.APFT.multiply(hb.IAPFT, result);
        DenseMatrix<double> I(n, n);
        I.identity();
        result.subtract(I, result);
        delta = result.maxAbs();
        std::cout << "APFT * IAPFT - I :: delta = " << delta << "\n";
        std::cout << "\n";
        if (delta>1e-12) {
            ok = false;
            std::cout << "IAPFT inverse failed\n";
        }

        // APFT of first non zero frequency cosine
        std::vector<double> v(n, 0.0);
        std::vector<double> vres(n, 0.0);
        auto f = hb.spurs_.spectrum()[1];
        auto mag = 10;
        for(size_t i=0; i<n; i++) {
            auto t = hb.timepoints[i];
            v[i] = mag*std::cos(2*std::numbers::pi*f*t);
        }

        auto vv = VectorView<double>(v);
        auto vvres = VectorView<double>(vres);
        hb.APFT.multiply(vv, vvres);
        auto norm = vvres.maxAbs();
        std::cout << "APFT of cosine at f1\n";
        vvres.dump(std::cout);
        std::cout << "\n";
        for(size_t i=0; i<n; i++) {
            if (
                (i==1 && std::abs(vvres[i]-mag)/norm>1e-12) ||
                (i!=1 && std::abs(vvres[i])/norm>1e-12)
            ) {
                ok = false;
                std::cout << "APFT failed\n";
                break;
            }
        }
    }

    if (!ok) {
        std::cout << "HB core test failed.\n";
    }

    return ok;
}

}
