#include "coretrannr.h"
#include "simulator.h"
#include "common.h"
#include "densematrix.h"
#include <iomanip>

namespace NAMESPACE {

TranNRSolver::TranNRSolver(
    Circuit& circuit, CommonData& commons, KluRealMatrix& jac,
    VectorRepository<double>& states, VectorRepository<double>& solution,
    DelayLines* delayLines, DelayMatrixBindings<double*>* delayBindings,
    const CircularBuffer<double>& timepointHistory_,
    NRSettings& settings, IntegratorCoeffs& integCoeffs
) : OpNRSolver(circuit, commons, jac, states, solution, delayLines, nullptr, settings, 3),
    // Pass delayLines (so OpNRSolver's loadSetup_ points loadCore() at it and
    // per-slot delay values get refreshed every NR iteration) but NOT the
    // bindings: the delay residual/Jacobian is loaded by our own buildSystem()
    // using the transient dynamics, so OpNRSolver must skip its static
    // passthrough stamp (it keys that off a non-null delayBindings_).
    integCoeffs(&integCoeffs), noiseEnabled(false),
    whiteBlock(nullptr), flickerBlock(nullptr),
    tranDelayLines_(delayLines), tranDelayBindings_(delayBindings), timepointHistory_(timepointHistory_) {
    // TranNRSolver has 2 force slots
    // 0 .. continuation nodesets for sweep and homotopy
    //      cannot contain branch forces
    // 1 .. forces explicitly specified via nodeset analysis parameter
    //      can contain branch forces
    // 2 .. UIC forces, never activated, but used for setting up UIC forces
    // Slots containing branch forces affect the circuit topology. 
    // They need to be set before rebuild() is called. 

    enableForces(2, false);

    // Set analysis type
    evalSetup_.staticAnalysis = false;
    evalSetup_.dcAnalysis = false;
    evalSetup_.tranAnalysis = true;
    
    // For constructing the linearized system in NR loop
    // Add reactive Jacobian and residual evaluation
    evalSetup_.evaluateReactiveJacobian = true;
    evalSetup_.evaluateReactiveResidual = true;
    evalSetup_.evaluateLinearizedReactiveRhsResidual = true;

    // Make sure reactive residual is stored in the state vector at evaluation
    evalSetup_.storeReactiveState = true;
    
    // Need this for evaluation of residual derivative
    evalSetup_.integCoeffs = &integCoeffs;

    // Breakpoints, timestep limiting
    evalSetup_.computeNextBreakpoint = true;
    evalSetup_.computeBoundStep = true;

    // Also check reactive residual and Jacobian convergence
    evalSetup_.checkReactiveConvergece = true;
    
    
    // Set up Jacobian loading
    loadSetup_.loadResistiveJacobian = false;
    loadSetup_.loadReactiveJacobian = false;
    loadSetup_.loadTransientJacobian = true;
    loadSetup_.integCoeffs = &integCoeffs;

    // Per-timestep loads: not the first timepoint. loadCore() keeps refreshing a
    // variable delay's td every iteration, but leaves maxDelay (and a
    // no-maxdelay delay's frozen td) at the value seeded at t=0. The t=0 seed
    // is done with firstTimepoint=true by the initial OP solve / lsInit in
    // TranCore::coroutine().
    loadSetup_.firstTimepoint = false;
}

void TranNRSolver::rebuildCheckResidualFlags() {
    // Build flags indicating residual can be checked for a node
    auto n = circuit.unknownCount();
    residualCheckable.assign(n+1, false);
    for(decltype(n) i=1; i<=n; i++) {
        residualCheckable[i] = circuit.checkResidual(i) &&
            !circuit.reprNode(i)->checkFlags(Node::Flags::InternalDeviceNode);
    }
}

bool TranNRSolver::initialize(bool continuePrevious, ErrorConsumer& errors) {
    // This method is called once on entering run()
    // This is the right place to set vectors

    // Call parent's initialize()
    if (!OpNRSolver::initialize(continuePrevious, errors)) {
        // parent has pushed the error
        return false;
    }

    // Set output vector for building linear system (reactive residual derivative)
    loadSetup_.reactiveResidualDerivative = delta.data();

    // Compute reactive residual derivative contribution
    // Update it in the max resistive residual contribution vector 
    loadSetup_.maxReactiveResidualDerivativeContribution = loadSetup_.maxResistiveResidualContribution; 

    // Resize lagged noise residual vector repository, 1 step of rollback
    noiseResidual.resize(jac.nRow()+1);
    
    return true;
}  

void TranNRSolver::enableNoise(
    TimeDomainNoiseBlock<std::mt19937_64>& white, 
    TimeDomainNoiseBlock<std::mt19937_64>& flicker, 
    size_t maxNsCount, 
    double noiseScale
) {
    // Store noise generators
    whiteBlock = &white;
    flickerBlock = &flicker;

    noiseScale_ = noiseScale;

    // Make space for scaling factors
    whiteScaling.resize(whiteBlock->values().size());
    flickerScaling.resize(flickerBlock->values().size());

    // Initial scaling is 0
    zero(whiteScaling);
    zero(flickerScaling);

    // Count white and flicker noise sources
    // Device/model/instance loops
    // Must traverse in exactly the same order each time at noise load. 
    noiseEnabled = true;

    // Preallocate temporary storage for noise power and exponent
    noisePower.resize(maxNsCount);
    noiseExponent.resize(maxNsCount);
    
    // Turn on noise evaluation in evalSetup_
    evalSetup_.evaluateNoise = true;
}

void TranNRSolver::disableNoise() { 
    whiteBlock = nullptr;
    flickerBlock = nullptr;
    noiseEnabled = false; 
    evalSetup_.evaluateNoise = true;
};

std::tuple<bool, bool> TranNRSolver::advanceNoise(double time, double h, std::mt19937_64& gen, ErrorConsumer& errors) {
    bool changed = false;

    // Collect noise scaling if lagged noise is used
    if (circuit.simulatorOptions().core().tran_laggednoise) {
        if (!collectNoiseScaling(errors)) {
            return std::make_tuple(false, changed);
        }
    }

    if (whiteBlock->advance(time, h, gen)) {
        changed = true;
    }
    if (flickerBlock->advance(time, h, gen)) {
        changed = true;
    }
    
    return std::make_tuple(true, changed);
}

bool TranNRSolver::revertNoise(double time, double h, std::mt19937_64& gen) {
    bool changed = false;
    if (whiteBlock->revert(time, h, gen)) {
        changed = true;
    }
    if (flickerBlock->revert(time, h, gen)) {
        changed = true;
    }
    
    return changed;
}

bool TranNRSolver::collectNoiseScaling(ErrorConsumer& errors) {
    size_t atWhite = 0;
    size_t atFlicker = 0;
    
    auto ndev = circuit.deviceCount();
    for(decltype(ndev) idev=0; idev<ndev; idev++) {
        auto dev = circuit.device(idev);
        auto nmod = dev->modelCount();
        for(decltype(nmod) imod=0; imod<nmod; imod++) {
            auto mod = dev->model(imod);
            auto ninst = mod->instanceCount();
            for(decltype(ninst) iinst=0; iinst<ninst; iinst++) {
                auto inst = mod->instance(iinst);
                // Noise source count
                auto nsCount = inst->noiseSourceCount();
                if (nsCount<=0) {
                    continue;
                }
                // Get noise source parameters
                inst->loadNoiseParameters(circuit, noisePower.data(), noiseExponent.data());
                // Go through noise sources
                for(decltype(nsCount) ndx=0; ndx<nsCount; ndx++) {
                    // Get noise source type
                    auto nstype = inst->noiseSourceType(ndx);
                    double sample = 0;
                    switch (nstype) {
                        case NoiseType::White: {
                            // Scale with sqrt(PSD) because this is a time-domain sample
                            auto pwr = noisePower[ndx];
                            auto sgn = pwr>0 ? 1 : -1;
                            whiteScaling[atWhite] = sgn*std::sqrt(std::abs(pwr))*noiseScale_;
                            atWhite++;
                            break;
                        }
                        case NoiseType::Flicker: {
                            // Noise exponent is lagged by one timepoint
                            // Currently exponent changes are not allowed
                            auto expStatus = flickerBlock->setShapeParameters(atFlicker, noiseExponent[ndx]);
                            if (expStatus==ShapeSetStatus::Unchanged) {
                                // Do nothing
                            } else if (expStatus==ShapeSetStatus::OutOfRange) {
                                // Out of range
                                errors.push(TranNrBadFlickerExponent{inst->name()});
                                return false;
                            } else if (expStatus==ShapeSetStatus::Changed) {
                                // Changed, but should not
                                errors.push(TranNrFlickerExponentChanged{inst->name()});
                                return false;
                            }
                            // Scale sample with sqrt(PSD) because this is a time-domain sample
                            auto pwr = noisePower[ndx];
                            auto sgn = pwr>0 ? 1 : -1;
                            flickerScaling[atFlicker] = sgn*std::sqrt(std::abs(pwr))*noiseScale_;
                            atFlicker++;
                            break;
                        }
                        default:
                            continue;
                    }
                }
            }
        }
    }
    return true;
}


bool TranNRSolver::buildNoiseResidual() {
    // Clear, get data
    zero(noiseResidual);
    auto noiseResidualContribution = noiseResidual.data();
    
    size_t atWhite = 0;
    size_t atFlicker = 0;
    
    auto whiteSamples = whiteBlock->values();
    auto flickerSamples = flickerBlock->values();

    auto ndev = circuit.deviceCount();
    for(decltype(ndev) idev=0; idev<ndev; idev++) {
        auto dev = circuit.device(idev);
        auto nmod = dev->modelCount();
        for(decltype(nmod) imod=0; imod<nmod; imod++) {
            auto mod = dev->model(imod);
            auto ninst = mod->instanceCount();
            for(decltype(ninst) iinst=0; iinst<ninst; iinst++) {
                auto inst = mod->instance(iinst);
                // Noise source count
                auto nsCount = inst->noiseSourceCount();
                if (nsCount<=0) {
                    continue;
                }
                // Go through noise sources
                for(decltype(nsCount) ndx=0; ndx<nsCount; ndx++) {
                    // Get noise source type
                    auto nstype = inst->noiseSourceType(ndx);
                    double sample = 0;
                    switch (nstype) {
                        case NoiseType::White: {
                            // Scale 
                            sample = whiteSamples[atWhite] * whiteScaling[atWhite];
                            atWhite++;
                            break;
                        }
                        case NoiseType::Flicker: {
                            // Scale
                            sample = flickerSamples[atFlicker] * flickerScaling[atFlicker];
                            atFlicker++;
                            break;
                        }
                        default:
                            continue;
                    }
                    // Get noise source terminals, add to residual
                    auto [e1, e2] = inst->noiseExcitation(circuit, ndx);
                    noiseResidualContribution[e1] += sample;
                    noiseResidualContribution[e2] -= sample;
                }
            }
        }
    }
    
    // Set bucket to 0
    noiseResidual[0] = 0;

    return true;
}

std::tuple<bool, bool> TranNRSolver::buildSystem(bool continuePrevious, ErrorConsumer& errors) {
    // First call the OpNRSolver method
    auto [ok, preventConvergence] = OpNRSolver::buildSystem(continuePrevious, errors);

    // Load delay line contributions
    auto nDelay = circuit.delayHistoryCount();
    if (ok && tranDelayLines_ && nDelay>0) {
        // Get old solution
        auto& oldSolution = solution.vector();
        for(decltype(nDelay) i=0; i<nDelay; i++) {
            // Get input and output unknowns
            auto inU = tranDelayLines_->inputUnknown(i);
            auto outU = tranDelayLines_->outputUnknown(i);

            // Equation -out + delay(in, td) = 0
            // Compute delayed input and its derivative wrt current sample
            // oldSolution is the previous NR step's solution
            auto [delayedSample, delayedSampleDerivative] = tranDelayLines_->getSample(i, evalSetup_.time, oldSolution, timepointHistory_);
            // Compute residual, store it
            auto delRes = -oldSolution[outU] + delayedSample;
            delta[outU] = delRes;
            // Get Jacobian Pointers
            auto [outIn, outOut] = (*tranDelayBindings_)[i];
            // Load Jacobian
            *outIn += delayedSampleDerivative;
            *outOut += -1;
        }
    }
    
    // Bucket is 0
    delta[0] = 0.0;

    // Now load the tranisent noise residuals
    if (ok && noiseEnabled) {
        if (!circuit.simulatorOptions().core().tran_laggednoise) {
            // Fully coupled noise scaling
            if (!collectNoiseScaling(errors)) {
                return std::make_tuple(false, preventConvergence);
            }
        }
        // Build noise residual
        auto ok = buildNoiseResidual();
        if (!ok) {
            return std::make_tuple(false, preventConvergence);
        }
        // Add to RHS, skip ground node contribution
        auto n = jac.nRow();
        VectorView resistiveResidualView(loadSetup_.resistiveResidual, 1, n, 1);
        VectorView noiseResidualView(noiseResidual, 1, n, 1);
        resistiveResidualView.add(noiseResidualView);
    }

    return std::make_tuple(ok, preventConvergence);
}

bool TranNRSolver::computeNoiseSolutionContribution(ErrorConsumer& errors) {
    // Solve with last factored Jacobian
    if (!jac.solve(dataWithoutBucket(noiseResidual, bucketSize_), errors)) {
        if (settings.debug) {
            Simulator::dbg() << "Failed to solve for noise contribution.\n";
        }
        return false;
    }
    return true;
}

std::tuple<bool, bool> TranNRSolver::checkResidual() {
    // Options
    auto& options = circuit.simulatorOptions().core();

    // Compute norms only in debug mode
    bool computeNorms = settings.debug;

    // In residual we have the residual at previous solution
    // We are going to check that residual
    
    // Number of unknowns (vector length includes a bucket at index 0)
    auto n = circuit.unknownCount();

    // Results
    maxResidual = 0.0;
    maxNormResidual = 0.0;
    l2normResidual2 = 0.0;
    maxResidualNode = nullptr;
    
    // Assume residual is OK
    residualWithinTol = true;
    
    // Get point maximum for each residual nature
    zero(pointMaxResidualContribution_); 
    for(decltype(n) i=1; i<=n; i++) {
        double c = std::fabs(maxResidualContribution_[i]);
        // Get residual nature index
        auto ndx = commons.residual_natureIndex[i];
        if (c>pointMaxResidualContribution_[ndx]) {
            pointMaxResidualContribution_[ndx] = c;
        }
    }
    
    // Go through all variables (except ground)
    for(decltype(n) i=1; i<=n; i++) {
        // Representative node, associated flow nature index
        auto rn = circuit.reprNode(i);
        // Skip this node if residual check is not allowed
        if (!residualCheckable[i]) {
            continue;
        }
        // Get residual nature index
        auto ndx = commons.residual_natureIndex[i];
        
        // Compute tolerance reference
        // Point local reference by default
        // Compute tolerance reference, start with previous value of the i-th unknown
        double tolref = std::fabs(maxResidualContribution_[i]);

        // Account for global and historic references
        if (historicResRef) {
            if (globalResRef) {
                // Historic global reference, ndx is the nature index
                tolref = std::max(tolref, globalMaxResidualContribution_[ndx]);
            } else {
                // Historic local reference, i is the index of unknown
                tolref = std::max(tolref, historicMaxResidualContribution_[i]);
            }
        } else if (globalResRef) {
            // Point global reference, ndx is the nature index
            tolref = std::max(tolref, pointMaxResidualContribution_[ndx]);
        }

        // For transient noise include noise residual in tolerance reference
        if (noiseEnabled) {
            tolref = std::max(tolref, std::abs(noiseResidual[i]));
        }

        // Residual tolerance (Designer's Guide to Spice and Spectre, chapter 2.2.2)
        auto tol = std::max(std::fabs(tolref*options.reltol), commons.residual_abstol[i]);

        // TODO: internal nodes when NR converges have a very low residual contribution
        //       because a single OSDI instance provides all the residual contribution to them 
        //       and the contribution is close to 0. 
        //       This means that absolute tolerances may be too low and prevent convergence forever. 
        //       This is somehow remedied if relrefres is set to pointglobal or global. 
        //       Possible solution: count number of connected devices to each node. 
        //       If that number is >1 use it in residual tolerance check. 

        // Residual component
        double rescomp = fabs(delta[i]);
    
        // Normalized residual component
        double normResidual = rescomp/tol;

        if (computeNorms) {
            l2normResidual2 += normResidual*normResidual;
            // Update largest normalized component
            if (i==1 || normResidual>maxNormResidual) {
                maxResidual = rescomp;
                maxNormResidual = normResidual;
                maxResidualNode = rn;
            }
        }

        // See if residual component exceeds tolerance
        if (rescomp>tol) {
            residualWithinTol = false;
            // Can exit if not computing norms
            if (!computeNorms) {
                return std::make_tuple(true, residualWithinTol); 
            }
        }
    }
    
    return std::make_tuple(true, residualWithinTol);
}

}
