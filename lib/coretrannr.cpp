#include "coretrannr.h"
#include "simulator.h"
#include "common.h"
#include <iomanip>

namespace NAMESPACE {

TranNRSolver::TranNRSolver(
    Circuit& circuit, CommonData& commons, KluRealMatrix& jac, 
    VectorRepository<double>& states, VectorRepository<double>& solution, 
    NRSettings& settings, IntegratorCoeffs& integCoeffs
) : OpNRSolver(circuit, commons, jac, states, solution, settings, 3), 
    integCoeffs(&integCoeffs), noiseEnabled(false), 
    whiteBlock(nullptr), flickerBlock(nullptr) {
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
}

bool TranNRSolver::initialize(bool continuePrevious) {
    // This method is called once on entering run()
    // This is the right place to set vectors
    
    // Call parent's initialize()
    if (!OpNRSolver::initialize(continuePrevious)) {
        // Assume parent has set lastError
        return false;
    }

    // Clear OP NR solver error
    clearError();

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

std::tuple<bool, bool> TranNRSolver::advanceNoise(double time, double h, std::mt19937_64& gen) {
    bool changed = false;

    // Collect noise scaling if lagged noise is used
    if (circuit.simulatorOptions().core().tran_laggednoise) {
        if (!collectNoiseScaling()) {
            std::make_tuple(false, changed);
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

bool TranNRSolver::collectNoiseScaling() {
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
                                errorInstance = inst;
                                lastTranNRError = TranNRSolverError::BadFlickerExponent;
                                return false;
                            } else if (expStatus==ShapeSetStatus::Changed) {
                                // Changed, but should not
                                errorInstance = inst;
                                lastTranNRError = TranNRSolverError::FlickerExponentChanged;
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
                    // Get noise source terminals, add to residuals
                    // Do this only if noiseResidualContribution is not nullptr
                    if (noiseResidualContribution) {
                        auto [e1, e2] = inst->noiseExcitation(circuit, ndx);
                        noiseResidualContribution[e1] += sample;
                        noiseResidualContribution[e2] -= sample;
                    }
                }
            }
        }
    }
    return true;
}

std::tuple<bool, bool> TranNRSolver::buildSystem(bool continuePrevious) {
    // First call the OpNRSolver method
    auto [ok, preventConvergence] = OpNRSolver::buildSystem(continuePrevious);

    // Now load the tranisent noise residuals
    if (ok && noiseEnabled) {
        if (!circuit.simulatorOptions().core().tran_laggednoise) {
            // Fully coupled noise scaling
            if (!collectNoiseScaling()) {
                return std::make_tuple(false, preventConvergence);
            }
        }
        // Build noise residual
        auto ok = buildNoiseResidual();
        if (!ok) {
            return std::make_tuple(false, preventConvergence);
        }
        // Add to RHS
        auto n = jac.nRow();
        // Skip ground node contribution
        auto nrvec = noiseResidual.data();
        for(decltype(n) i=1; i<=n; i++) {
            loadSetup_.resistiveResidual[i] += nrvec[i];
        }
    }

    return std::make_tuple(ok, preventConvergence);
}

bool TranNRSolver::formatError(Status& s, NameResolver* resolver) const {
    // Error in NRSolver
    if (lastError!=NRSolver::Error::OK) {
        NRSolver::formatError(s, resolver);
        return false;
    }

    if (lastError!=OpNRSolver::Error::OK) {
        OpNRSolver::formatError(s, resolver);
        return false;
    }

    switch (lastTranNRError) {
        case TranNRSolverError::BadFlickerExponent:
            s.set(Status::BadArguments, "Flicker noise exponent out of range for '"+std::string(errorInstance->name())+"'.");
            return false;
        case TranNRSolverError::FlickerExponentChanged:
            s.set(Status::BadArguments, "Flicker noise exponent is not constant for '"+std::string(errorInstance->name())+"'.");
            return false;
        default:
            return true;
    }
}

}
