#include "coretrannr.h"
#include "simulator.h"
#include "common.h"
#include <iomanip>

namespace NAMESPACE {

TranNRSolver::TranNRSolver(
    Circuit& circuit, CommonData& commons, KluRealMatrix& jac, 
    VectorRepository<double>& states, VectorRepository<double>& solution, 
    NRSettings& settings, IntegratorCoeffs& integCoeffs, 
    TimeDomainZohWhiteNoise<std::mt19937_64>& whiteBlock,
    TimeDomainZohFlickerNoise<std::mt19937_64>& flickerBlock
) : OpNRSolver(circuit, commons, jac, states, solution, settings, 3), 
    integCoeffs(&integCoeffs), noiseEnabled(false), 
    whiteBlock(whiteBlock), flickerBlock(flickerBlock) {
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
    noiseResidual.upsize(2, jac.nRow()+1);
    reverted = 0;
    
    return true;
}  

void TranNRSolver::enableNoise(size_t maxNsCount) {
    // Count white and flicker noise sources
    // Device/model/instance loops
    // Must traverse in exactly the same order each time at noise load. 
    noiseEnabled = true;
    
    // Maximal number of noise sources per instance
    maxNsCount_ = maxNsCount;
    
    // Turn on noise evaluation in evalSetup_
    evalSetup_.evaluateNoise = true;
}

std::tuple<bool, bool> TranNRSolver::advanceNoise(double time, std::mt19937_64& gen) {
    bool changed = false;
    if (whiteBlock.advance(time, gen)) {
        changed = true;
    }
    if (flickerBlock.advance(time, gen)) {
        changed = true;
    }
    // If advancing the noise generator changed noise samnple index
    // rebuild lagged noise residual
    if (circuit.simulatorOptions().core().tran_laggednoise && changed) {
        noiseResidual.advance(1);
        if (reverted) {
            // Moving forward from reverted value
            reverted -= 1;
        }
        // Rebuild lagged noise residual
        noiseResidual.zero();
        auto ok = buildNoiseResidual(noiseResidual.data());
        if (!ok) {
            return std::make_tuple(false, changed);
        }
    }
    return std::make_tuple(true, changed);
}

bool TranNRSolver::revertNoise(double time, std::mt19937_64& gen) {
    bool changed = false;
    if (whiteBlock.revert(time, gen)) {
        changed = true;
    }
    if (flickerBlock.revert(time, gen)) {
        changed = true;
    }
    // If reverting the noise generator changed noise samnple index
    // rebuild lagged noise residual
    if (circuit.simulatorOptions().core().tran_laggednoise && changed) {
        if (reverted) {
            throw std::logic_error("Cannot revert noise by more than 1 step.");
        }
        noiseResidual.advance(-1);
        reverted += 1;
    }
    return changed;
}


bool TranNRSolver::buildNoiseResidual(double* noiseResidualContribution) {
    size_t atWhite = 0;
    size_t atFlicker = 0;
    
    // These two will hopefully be elided to stack
    // TODO: use a stack-based container
    // need to allocate here beacause in the future 
    // if we use OpenMP these will be thread-local variables. 
    // TODO: check all classes for persistent storage, mark it for replacement
    RealVector noisePower(maxNsCount_);
    RealVector noiseExponent(maxNsCount_);
    
    auto whiteSamples = whiteBlock.values();
    auto flickerSamples = flickerBlock.values();

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
                            sample = whiteSamples[atWhite] * sgn*std::sqrt(std::abs(pwr));
                            atWhite++;
                            break;
                        }
                        case NoiseType::Flicker: {
                            auto expStatus = flickerBlock.setExponent(atFlicker, noiseExponent[ndx]);
                            if (expStatus==ExponentStatus::Unchanged) {
                                // Do nothing
                            } else if (expStatus==ExponentStatus::OutOfRange) {
                                // Out of range
                                errorInstance = inst;
                                lastTranNRError = TranNRSolverError::BadFlickerExponent;
                                return false;
                            } else if (expStatus==ExponentStatus::Changed) {
                                // Changed, but should not
                                errorInstance = inst;
                                lastTranNRError = TranNRSolverError::FlickerExponentChanged;
                                return false;
                            }
                            // Scale sample with sqrt(PSD) because this is a time-domain sample
                            auto pwr = noisePower[ndx];
                            auto sgn = pwr>0 ? 1 : -1;
                            sample = flickerSamples[atFlicker] * sgn*std::sqrt(std::abs(pwr));
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
        if (circuit.simulatorOptions().core().tran_laggednoise) {
            // Lagged noise residual
            auto n = jac.nRow();
            // Skip ground node contribution
            auto nrvec = noiseResidual.data();
            for(decltype(n) i=1; i<=n; i++) {
                loadSetup_.resistiveResidual[i] += nrvec[i];
            }
        } else {
            // Fully coupled noise residual
            auto ok = buildNoiseResidual(loadSetup_.resistiveResidual);
            if (!ok) {
                return std::make_tuple(false, preventConvergence);
            }
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
