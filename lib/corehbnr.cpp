#include "corehbnr.h"
#include "simulator.h"
#include "densematrix.h"
#include "common.h"

namespace NAMESPACE {

HBNRSolver::HBNRSolver(
        Circuit& circuit, 
        CommonData& commons, 
        KluBlockSparseRealMatrix& jacColoc, 
        KluBlockSparseRealMatrix& bsjac, 
        VectorRepository<double>& solution, 
        Vector<Complex>& solutionFD, 
        const Vector<double>& timepoints, 
        const Spurs& spurs,
        DenseMatrix<double>& Gamma, 
        DenseMatrix<double>& GammaInv, 
        DenseMatrix<Real>& OmegaGamma, 
        DenseMatrix<Real>& GammaInvColumnMajor, 
        NRSettings& settings
) : circuit(circuit), commons(commons), jacColoc(jacColoc), bsjac(bsjac), solutionFD(solutionFD), 
    timepoints(timepoints), spurs_(spurs), 
    Gamma(Gamma), GammaInv(GammaInv), OmegaGamma(OmegaGamma), GammaInvColumnMajor(GammaInvColumnMajor), 
    NRSolver(circuit.tables().accounting(), bsjac, solution, settings, 0) {
    // Bucket size is 0
    // Slot 0 is for sweep continuation and homotopy (set via CoreStateStorage object)
    // Slot 1 is for nodesets that are read from stored results. 
    resizeForces(2);

    // For constructing the linearized system in NR loop
    evalSetup_ = EvalSetup {
        // Inputs
        .solution = &oldSolutionAtTk, 
        .dummyStates = &dummyStates, 

        // Signal this is not a static DC analysis
        // Evaluation is in time domain so effectively 
        // we are doing the same thing as transient analysis. 
        .staticAnalysis = false, 
        .dcAnalysis = false, 
        .tranAnalysis = true, 

        // Evaluation
        // - no limiting
        // - resistive and reactive
        // - no output variables for now - maybe later we can collect their time-domain points
        //   and transform them to frequency domain before dumping them
        .enableLimiting = false, 
        .evaluateResistiveJacobian = true, 
        .evaluateReactiveJacobian = true, 
        .evaluateResistiveResidual = true, 
        .evaluateReactiveResidual = true, 
        .evaluateOutvars = true, 
    };

    loadSetup_ = LoadSetup {
        .states = nullptr, 
        .loadResistiveJacobian = true, 
        .loadReactiveJacobian = true, 
        // Used for loading with offset 0
        .reactiveJacobianFactor = 1.0, 
    };
}

bool HBNRSolver::setForces(Int ndx, const AnnotatedSolution& storedSolution, bool abortOnError) {
    // Get forces
    auto& f = forces(ndx);

    // Clear forced values, set number of forces to 0
    f.clear();
    
    // Number of unknowns
    auto n = circuit.unknownCount();

    // Number of components per unknown
    auto blockSize = timepoints.size(); // number of timepoints per unknown

    // No bucket
    // Make space for variable forces
    f.unknownValue_.resize(n*blockSize, 0.0);
    // By default turn off all forces
    f.unknownForced_.resize(n*blockSize, false);
    
    // Number of frequencies in solution and solver
    auto nfSolution = storedSolution.spurs().spectrum().size();
    auto nfSolver = spurs_.spectrum().size();

    // Prepare frequency translator between solution and solver
    // Translator stores the solver frequency index for each solution frequency index
    // Assume no frequency can be translated into solution frequency (negative index)
    std::vector<int> xlat(nfSolver, -1);
    
    // DC can be translated as new 0 -> old 0 (it is always present)
    if (nfSolver>0 && nfSolution>0) {
        xlat[0] = 0;
    }

    // Translate the rest
    decltype(nfSolver) ndxSolver = 0;
    decltype(nfSolution) ndxSolution = 0;
    for(; ndxSolver<nfSolver && ndxSolution<nfSolution;) {
        auto fSolver = spurs_.spectrum()[ndxSolver];
        auto fSolution = storedSolution.spurs().spectrum()[ndxSolution];
        if (std::abs(fSolver-fSolution)<=std::max(std::abs(fSolver), std::abs(fSolution))*1e-14) {
            // Frequencies are almost the same, store translator
            xlat[ndxSolver] = ndxSolution;
            // Advance both indices
            ndxSolver++;
            ndxSolution++;
        } else if (fSolver<fSolution) {
            // Solver frequency is lower, advance solver index
            ndxSolver++;
        } else {
            // Solution frequency is lower, advance solution index
            ndxSolution++;
        }
    }

    // Go through annotated solution. fill APFT spectrum 
    // Use resistive residual vector for APFT spectrum
    auto& forcesFD = resistiveResidual;
    // Resistive residual can hold n APFT spectra, we need only the first one
    // Resize it, just in case
    forcesFD.resize(n*blockSize);

    // Solution spectrum
    auto& solSpec = storedSolution.cxValues();
    auto& solNames = storedSolution.names();
    // In case we have no names, we must deduce the number of stored solution nodes (including ground). 
    // The vector does not contains entries for ground node. 
    // This number must match the length of solution names
    auto solNodes = solSpec.size()/nfSolution;

    // Check if we have solution name annotations
    bool checkNames;
    // Note that solNames includes ground node
    if (solNames.size()-1==solNodes) {
        // Yes, check names
        checkNames = true;
    } else if (solNames.size()==0 && solNodes==n) {
        // No annotations, solutions vector has correct length
        checkNames = false;
    } else {
        // Cannot apply stored solution, no names nor matching length vector
        lastHBNRError = HBNRSolverError::ForcesError;
        // Abort always regardless of abortOnError
        return false;
    }

    // Go through all unknowns, skip the unknown corresponding to the bucket 
    // If the stored solution has no names its solution vector length 
    // must match the current circuit's solution vector
    for(decltype(n) i=1; i<=solNodes; i++) {
        Node* node;
        if (checkNames) {
            // Stored solution has name annotations, get node by the name from the solution
            node = circuit.findNode(solNames[i]);
        } else {
            // Stored solution is coherent, simply take the representative node of i-th unknown
            node = circuit.reprNode(i);
        }
        if (!node) {
            // Node not found. No forces will be applied to this unknown. 
            // If abortOnError is set, abort 
            if (abortOnError) {
                lastHBNRError = HBNRSolverError::ForcesError;
                return false;
            }
            // Otherwise continue to next force
            continue;
        }
        // Copy spectrum for one node
        auto ui = node->unknownIndex();
        // Ground node, nothing to do
        if (ui==0) {
            continue;
        }
        // Spectrum origin index in complex spectrum vector (no bucket)
        auto srcOrigin = (i-1)*nfSolution;
        // Spectrum origin index in destination vector of TD values (no bucket)
        auto destOrigin = (ui-1)*blockSize;
        
        // Copy DC (one real value)
        f.unknownValue_[destOrigin] = solSpec[srcOrigin].real();
        f.unknownForced_[destOrigin] = true;
        // Scan all nonzero frequencies of solver's spectrum
        for(decltype(nfSolver) k=1; k<nfSolver; k++) {
            // Translate solver frequency into solution frequency
            auto xlf = xlat[k];
            // Index of real component (DC is stored as a single real number)
            auto ndx = 1+2*(k-1);
            if (xlf>=0) {
                // Translation exists, copy solution component (cos, -sin)
                // Solution is stored as all-complex vector
                // Solver's first component is real, the rest is complex
                f.unknownValue_[destOrigin+ndx] = solSpec[srcOrigin+xlf].real();
                f.unknownValue_[destOrigin+ndx+1] = solSpec[srcOrigin+xlf].imag();
                f.unknownForced_[destOrigin+ndx] = true;
                f.unknownForced_[destOrigin+ndx+1] = true;
            } else {
                // No translation, fill with zeros
                f.unknownValue_[destOrigin+ndx] = 0;
                f.unknownValue_[destOrigin+ndx+1] = 0;
            }
        }
    }

    // std::cout << "Set forces:\n";
    // f.dump(circuit, std::cout);
    
    return true; 
}

bool HBNRSolver::rebuild(size_t nSolComp) {
    // Call parent's rebuild
    if (!NRSolver::rebuild(nSolComp)) {
        // Assume parent has set the error flag
        return false;
    }

    // Old states at one timepoint (dummy vector of zeros because we do no limiting)
    dummyStates.resize(circuit.statesCount());
    zero(dummyStates);
    
    // Jacobian is sized in core or analysis.
    // solution is sized in core. 
    // delta is resized by NRSolver::rebuild() based on Jacobian size. 

    // Because vector lengths and Jacobian size may change 
    // due to different number of frequency components 
    // we size them in initialize() before first iteration. 
    // Analysis asks cores if they request a rebuild. 
    // HB core replies that it does if the set of frequencies changes. 

    // Do this only if sparse matrix is built
    // If we are using the nrSolver just for evaluation, 
    // matrix is not built and we will never load forces
    // so we do not need diagonal pointers. 
    if (bsjac.isBuilt()) {
        // Get diagonal pointers for forces
        auto n = circuit.unknownCount();
        auto nt = timepoints.size();
        diagPtrs.resize(n*nt);
        
        // Bind diagonal matrix elements, block indices are 1-based, 0 is the bucket
        // Needed for forcing unknown values
        for(decltype(n) i=0; i<n; i++) {
            for(decltype(nt) j=0; j<nt; j++) {
                // We know the matrix type so we can use the elementPtr() non-virtual function
                diagPtrs[i*nt+j] = bsjac.elementPtr(MatrixEntryPosition(i+1, i+1), Component::Real, MatrixEntryPosition(j, j));
            }
        }
    }

    return true;
}

bool HBNRSolver::initialize(bool continuePrevious) {
    // Clear HB NR solver error
    clearError();

    // Number fo frequency components and timepoints
    auto nt = timepoints.size();
    
    // Gamma and GammaInv are already set up

    // Number of nodes
    auto n = circuit.unknownCount();

    // Old solution and derivative wrt time at all timepoints
    resistiveResidual.resize(n*nt);
    reactiveResidual.resize(n*nt);

    // Old solution in time domain
    solutionTD.resize(n*nt); 

    // Temporary storage for Jacobian block, row major form
    blockTmp.resize(nt, nt);

    // Old solution and resistive residual at one timepoint
    // Includes ground node because it is used by evalAndLoad()
    oldSolutionAtTk.upsize(1, n+1);
    resistiveResidualAtTk.resize(n+1);
    reactiveResidualAtTk.resize(n+1);

    // Maximum residual contribution at single timepoint
    // Includes ground node because it is used by evalAndLoad()
    maxResidualContributionAtTk_.resize(n+1);

    // Set up loading
    // Resistive residual
    loadSetup_.resistiveResidual = resistiveResidualAtTk.data();
    // Reactive residual
    loadSetup_.reactiveResidual = reactiveResidualAtTk.data();

    // Maximal residual contribution computed by evalAndLoad()
    loadSetup_.maxResistiveResidualContribution = maxResidualContributionAtTk_.data();

    // Zero states
    zero(dummyStates);

    // Set up tolerance reference value for solution
    auto& options = circuit.simulatorOptions().core();
    
    return true;
}

bool HBNRSolver::preIteration(bool continuePrevious) {
    return true;
}

bool HBNRSolver::postSolve(bool continuePrevious) {
    // Nothing to do - we have no bypassing
    return true;
}

bool HBNRSolver::postConvergenceCheck(bool continuePrevious) {
    return NRSolver::postConvergenceCheck(continuePrevious);
}

bool HBNRSolver::postIteration(bool continuePrevious) {
    return true;
}

bool HBNRSolver::postRun(bool continuePrevious) {
    if (converged) {
        // If converged, convert solution from TD to FD, store as complex spectrum
        auto n = circuit.unknownCount();
        auto nf = spurs_.spectrum().size();
        auto nt = timepoints.size();
        solutionFD.resize(n*nf); // no bucket
        
        // Data
        for(decltype(n) i=0; i<n; i++) {
            auto srcOrigin = i*nt;
            auto destOrigin = i*nf;
            auto& data = solution.vector();
            solutionFD[destOrigin] = data[srcOrigin];
            for(decltype(nf) k=1; k<nf; k++) {
                auto base = srcOrigin + 1 + (k-1)*2;
                solutionFD[destOrigin+k] = Complex(data[base], data[base+1]);
            }
        }
    }
    return true;
}

bool HBNRSolver::evalAndLoadWrapper(EvalSetup& evalSetup, LoadSetup& loadSetup) {
    lastError = Error::OK;
    evalSetup.requestHighPrecision = highPrecision;
    if (!circuit.evalAndLoad(commons, &evalSetup, &loadSetup, nullptr)) {
        // Load error
        lastError = Error::EvalAndLoad;
        if (settings.debug>2) {
            Simulator::dbg() << "Evaluation error.\n";
        }
        return false;
    }

    // Store Abort, Finish, and Stop flag
    if (evalSetup_.requests.abort) {
        setFlags(Flags::Abort);
    }
    if (evalSetup_.requests.finish) {
        setFlags(Flags::Finish);
    }
    if (evalSetup_.requests.stop) {
        setFlags(Flags::Stop);
    }
    
    // Handle abort right now, finish and stop are handled outside NR loop
    if (checkFlags(Flags::Abort)) {
        if (settings.debug>2) {
            Simulator::dbg() << "Abort requested during evaluation.\n";
        }
        return false;
    }

    return true;
}

bool HBNRSolver::evaluate(bool continuePrevious) {
    // Jacobian values at colocation points are stored in jacColoc with dense 
    // blocks of size nt x 2, where nt is the number of colocation points. 
    // Resistive Jacobian is bound to 0-based subentry (0, 0) of each dense block. 
    // Reactive Jacobian is bound to 0-based subentry (0, 1) of each dense block. 
    // As Jacobian load offset goes from 0..nb-1 (nb=ntimepoints)
    // the first two columns of each block are loaded with Jacobian values, 
    // i.e. (k, 0) with resistive and (k, 1) with reactive Jacobian values 
    // at times coresponding to timepoints tk, k=0..nb-1 because KLU matrices are 
    // stored in column major order. 
    auto n = circuit.unknownCount();
    auto nb = timepoints.size();

    // Old frequency domain solution is in solution, transform to time domain
    for(decltype(n) i=0; i<n; i++) {
        auto src = VectorView(solution.vector(), i*nb, nb, 1);
        auto dest = VectorView(solutionTD, i*nb, nb, 1);
        GammaInv.multiply(src, dest);
    }
    
    // Clear Jacobian at colocation points
    jacColoc.zero();
    
    // Loop through timepoints 0..nb-1
    for(decltype(nb) k=0; k<nb; k++) {
        // We read old solution starting at index 1+k
        // (skip bucket, k-th unknown, first timepoint)
        // Vector length n, stride nb
        // We write to the vector of old solutions at timepoint t_k, 
        // start at index 1 (skip bucket), length n, stride 1
        VectorView(oldSolutionAtTk.vector(), 1, n, 1) = VectorView(solutionTD, k, n, nb);

        // Zero residual vectors where evalAndLoad() will load the residuals at t_k
        zero(resistiveResidualAtTk);
        zero(reactiveResidualAtTk);

        // Zero maximal residual contribution at timepoint
        zero(maxResidualContributionAtTk_);
        
        // Set time and offset
        evalSetup_.time = timepoints[k];
        loadSetup_.jacobianLoadOffset = k;

        // For k-th timepoint (t_k) load 
        // - resistive and reactive Jacobian at t_k with offset i, 
        // - resistive residuals for all equations at t_k
        // - reactive residuals for all equations at t_k
        // Values are stored in jacColoc. 
        auto ok = evalAndLoadWrapper(evalSetup_, loadSetup_);
        if (!ok) {
            return false;
        }

        // Put resistive residuals at t_k in the residuals vector
        VectorView(resistiveResidual, k, n, nb) = VectorView(resistiveResidualAtTk, 1, n, 1);
        VectorView(reactiveResidual, k, n, nb) = VectorView(reactiveResidualAtTk, 1, n, 1);
    }

    return true;
}

std::tuple<bool, bool> HBNRSolver::buildSystem(bool continuePrevious) {
    // Let Jr_ijk and Jc_ijk denote the resistive and reactive Jacobian value 
    // from block with 1-based position (i+1, j+1) at timepoint with index k. 
    // i, j and k are all 0-based. 
    // After nt evalAndLoad() calls the two columns of each block in jacColoc 
    // are filled with Jr_ijk and Jc_ijk. 
    // 
    // The unknowns and the equations are in frequency domain, i.e. we formulate 
    // HB in frequency domain. Let Gij denote the diagonal nb x nb matrix holding 
    // the resistive Jacobian values for the block at position (i+1, j+1). 
    // Similarly Cij is the diagonal nb x nb matrix holding the reactive 
    // Jacobian values for the block. 
    
    // The HB Jacobian block can then be expressed as
    //   Gamma Gij GammaInv + Omega Gamma Cij GammaInv
    // where Gamma and GammaInv are the forward and the inverse Fourier transform, 
    // while Omega is the time-derivative operator in frequency domain. 
    // Matrix OmegaGamma holds Omega Gamma. 
    // Matrix GammaInvColumnMajor is GammaInv in column-major order (better for caching), 
    // 
    // Gamma Gij GammaInv is computed by scaling columns of Gamma with 
    // valuef of G at colocation points, followed by multiplying with GammaInvColumnMajor. 
    // 
    // Omega Gamma Cij GammaInv is computed by scaling columns of OmegaGamma with
    // values of C at colocation points followed by  multiplying with GammaInvColumnMajor. 
    
    // Residual is computed as 
    // Gamma f + OmegaGamma c
    // where f and c are the resistive and the reactive residuals at colocation points. 
    
    // Get sizes
    auto n = circuit.unknownCount();
    auto nb = timepoints.size();

    // Remove forces originating from nodesets after nsiter iterations
    auto nsiter = circuit.simulatorOptions().core().hb_nsiter;
    // Do this only at nsiter+1 (first iteration has index 1)
    if (iteration==nsiter+1) {
        // Continuation nodesets
        enableForces(0, false);
        // User-specified nodesets
        enableForces(1, false);
    }

    // Evaluate at colocation points
    if (!evaluate(continuePrevious)) {
        return std::make_tuple(false, false); ;
    }

    // For each block (ordered in column major order)
    for(auto& pos : circuit.sparsityMap().positions()) {
        // Get block position (for debugging), make position 0-based
        auto [i, j] = pos;
        i--;
        j--;

        // Get dense block with Jacobian values at colocation points
        auto [colocBlock, found1] = jacColoc.block(pos);

        // Get HB Jacobian dense block
        auto [block, found2] = bsjac.block(pos);

        // Get g_ijk and c_ijk columns from block (column elements are indexed by k)
        auto gCol = colocBlock.column(0);
        auto cCol = colocBlock.column(1);

        // blockTmp = Gamma Jrdiag Gamma^-1
        // Scale columns of Gamma with resistive Jacobian at colocation points
        blockTmp.scaledColumns(Gamma, gCol);
        
        // blockTmp += Omega Gamma Jcdiag Gamma^-1
        // Scale columns of Omega Gamma with reactive Jacobian at colocation points
        blockTmp.addScaledColumns(OmegaGamma, cCol);
        
        // blockTmp Gamma^-1 -> HB Jacobian block
        blockTmp.multiply(GammaInvColumnMajor, block);
    }

    // Now handle residuals
    // delta is zeroed at the beginning of each iteration by NRSolver
    // Gamma f(x) + Omega Gamma q(x)
    for(decltype(n) i=0; i<n; i++) {
        auto g = VectorView(resistiveResidual, i*nb, nb, 1);
        auto q = VectorView(reactiveResidual, i*nb, nb, 1);
        auto dest = VectorView(delta, i*nb, nb, 1);
        Gamma.multiply(g, dest);
        OmegaGamma.multiplyAdd(q, dest);
    }

    // Add forced values to the system
    if (haveForces() && !loadForces(true)) {
        if (settings.debug) {
            Simulator::dbg() << "Failed to load forced values at iteration " << iteration << "\n";
        }
        lastHBNRError = HBNRSolverError::LoadForces;
        errorIteration = iteration;
        return std::make_tuple(false, evalSetup_.limitingApplied);
    }
    
    // OK, do not prevent convergence
    return std::make_tuple(true, false); 
}

bool HBNRSolver::loadForces(bool loadJacobian) {
    // Are any forces enabled? 
    auto nForces = forcesList.size();
    
    // Get row norms
    jac.rowMaxNorm(dataWithoutBucket(rowNorm, bucketSize_));

    // Load forces
    auto n = jac.nRow();
    double* xprev = solution.data();
    for(decltype(nForces) iForce=0; iForce<nForces; iForce++) {
        // Skip disabled force lists
        if (!forcesEnabled[iForce]) {
            continue;
        }
        double ff = forcesFactor[iForce];

        // First, handle forced unknowns
        auto& enabled = forcesList[iForce].unknownForced_;
        auto& force = forcesList[iForce].unknownValue_;
        auto nForceEquations = force.size();
        // Load only if the number of forced unknowns matches 
        // the number of equations
        if (nForceEquations==n) {
            for(decltype(nForceEquations) i=0; i<nForceEquations; i++) {
                if (enabled[i]) {
                    double factor = rowNorm[i]*ff;
                    if (factor==0.0) {
                        factor = 1.0;
                    }
                    // Jacobian entry: factor
                    // Residual: factor * x_i - factor * nodeset_i
                    auto ptr = diagPtrs[i];
                    if (ptr) {
                        // Negative diagonal element, change sign of factor
                        if (*ptr<0) {
                            factor = -factor;
                        }
                        // Jacobian
                        if (loadJacobian) {
                            *ptr += factor;
                        }
                        // Residual
                        delta[i] += factor * xprev[i] - factor * force[i];
                    }
                }
            }
        }
    }

    return true;
}

// No residual checking when HB is formulated in frequency domain
// because maximal residual contribution is a time domain quantity
std::tuple<bool, bool> HBNRSolver::checkResidual() {
    return std::make_tuple(true, true); 
}

std::tuple<bool, bool> HBNRSolver::checkDelta() {
    // Options
    auto& options = circuit.simulatorOptions().core();

    // Compute norms only in debug mode
    bool computeNorms = settings.debug;

    // In delta we have the solution change
    // Check it for convergence
    
    // Number of unknowns (vector length includes a bucket at index 0)
    auto n = circuit.unknownCount();

    // Number of timepoints and frequencies
    auto nt = timepoints.size();
    auto nf = spurs_.spectrum().size();

    maxDelta = 0.0;
    maxNormDelta = 0.0;
    maxDeltaNode = nullptr;
    maxDeltaFreqIndex = 0;
    
    // Check convergence (see if delta is small enough), 
    // but only if this is iteration 2 or later
    // In iteration 1 assume we did not converge
    
    // Assume we converged
    deltaWithinTol = true;
    
    auto xold = solution.data();
    auto xdelta = delta.data();
    // Scan unknowns
    for(decltype(n) i=1; i<=n; i++) {
        // Find maximal magnitude across frequencies to use as tolerance reference
        double tolRef = std::abs(xold[(i-1)*nt]);
        for(decltype(nt) j=1; j<nf; j++) {
            auto baseI = (i-1)*nt+(j-1)*2+1;
            double mag = std::sqrt(xold[baseI]*xold[baseI] + xold[baseI+1]*xold[baseI+1]);
            if (mag>tolRef) {
                tolRef = mag;
            }
        }

        // Scan frequencies
        for(decltype(nt) j=0; j<nf; j++) {
            // Index of component, tolerance reference, absolute delta
            size_t baseI;
            double deltaAbs;
            if (j==0) {
                // Handle DC (real)
                baseI = (i-1)*nt;
                // tolref = std::abs(xold[baseI]);
                deltaAbs = std::abs(xdelta[baseI]);
            } else {
                // Handle the rest (complex)
                baseI = (i-1)*nt+(j-1)*2+1;
                // tolref = std::sqrt(xold[baseI]*xold[baseI] + xold[baseI+1]*xold[baseI+1]);
                deltaAbs = std::sqrt(xdelta[baseI]*xdelta[baseI] + xdelta[baseI+1]*xdelta[baseI+1]);
            }
            
            // Compute tolerance
            double tol = std::max(std::fabs(tolRef*options.reltol), commons.unknown_abstol[i]);

            if (computeNorms) {
                double normDelta = deltaAbs/tol;
                if ((i==1 && j==0) || normDelta>maxNormDelta) {
                    maxDelta = deltaAbs;
                    maxNormDelta = normDelta;
                    maxDeltaNode = circuit.reprNode(i);
                    maxDeltaFreqIndex = j;
                }
            }

            // Check tolerance
            if (deltaAbs>tol) {
                // Did not converge
                deltaWithinTol = false;
                
                // Can exit if not computing norms
                if (!computeNorms) {
                    return std::make_tuple(true, deltaWithinTol);
                }
            }
        }
    }
    
    return std::make_tuple(true, deltaWithinTol);
}

std::string HBNRSolver::formatConvergence() const {
    std::stringstream ss;
    ss << std::scientific << std::setprecision(2);
    std::string s = (preventedConvergence ? "convergence not allowed" : "");
    if (!preventedConvergence) {
        s += (iterationConverged ? "converged" : "");
        if (iteration>1) {
            ss.str(""); ss << maxDelta;
            if (s.length()>0) {
                s +=", ";
            }
            s += "worst delta=";
            s += ss.str(); 
            if (!deltaWithinTol) {
                s += " >TOL";
            }
            s += " @ ";
            s += (maxDeltaNode ? std::string(maxDeltaNode->name()) : "(unknown)");
            s += "~f";
            s += std::to_string(maxDeltaFreqIndex);
            s += "=";
            s += std::to_string(spurs_.spectrum()[maxDeltaFreqIndex]);
        }
    }

    return s;
}

bool HBNRSolver::formatError(Status& s, NameResolver* resolver) const {
    // Error in NRSolver
    if (lastError!=NRSolver::Error::OK) {
        NRSolver::formatError(s, resolver);
        return false;
    }

    switch (lastHBNRError) {
        case HBNRSolverError::ForcesError:
            s.set(Status::Force, "Failed to apply forces.");
            return false;            
        case HBNRSolverError::LoadForces:
            s.set(Status::Force, "Failed to load forces.");
            return false;
        default:
            return true;
    }
}

void HBNRSolver::dumpSolution(std::ostream& os, double* solution, const char* prefix) {
    auto n = circuit.unknownCount();
    auto nt = timepoints.size();
    auto nf = spurs_.spectrum().size();
    for(decltype(n) i=1; i<=n; i++) {
        auto rn = circuit.reprNode(i);
        auto base = (i-1)*nt;
        for(decltype(nf) k=0; k<nf; k++) {
            Complex x;    
            if (k==0) {
                x = solution[base];
            } else {
                auto ndx = base+1+(k-1)*2;
                x = Complex(solution[ndx], solution[ndx+1]);
            }
            os << prefix << rn->name() << "@f" << k << " : " << x << "\n";
        }
    }
}

}
