#include "coretrannr.h"
#include "simulator.h"
#include "common.h"
#include <iomanip>

namespace NAMESPACE {

TranNRSolver::TranNRSolver(
    Circuit& circuit, CommonData& commons, KluRealMatrix& jac, 
    VectorRepository<double>& states, VectorRepository<double>& solution, 
    NRSettings& settings, IntegratorCoeffs& integCoeffs
) : OpNRSolver(circuit, commons, jac, states, solution, settings, 3), integCoeffs(&integCoeffs) {
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

void TranNRSolver::initializeNoise(double noiseStepLimit, std::mt19937_64& gen) {
    // Count white and flicker noise sources
    // Device/model/instance loops
    // Must traverse in exactly the same order each time at noise load. 
    size_t nWhite = 0;
    size_t nFlicker = 0;
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
                for(decltype(nsCount) ins=0; ins<nsCount; ins++) {
                    auto nstype = inst->noiseSourceType(ins);
                    switch (nstype) {
                        case NoiseType::White:
                            nWhite++;
                            break;
                        case NoiseType::Flicker:
                            nFlicker++;
                            break;
                    }
                }
            }
        }
    }

    // Resize noise generator blocks
    // New noise sample time is 2*noiseStepLimit
    whiteBlock.reset(0.0, 2*noiseStepLimit, nWhite, 1, gen);
}

std::tuple<bool, bool> TranNRSolver::advanceNoise(double time, std::mt19937_64& gen) {
    bool changed = false;
    if (whiteBlock.advance(time, gen)) {
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
        buildNoiseResidual(noiseResidual.data());
    }
    // It is sufficient to check step sanity with white noise block
    // flicker noise block would perform the same check. 
    if (!whiteBlock.stepSanityCheck(time)) {
        return std::make_tuple(false, changed);
    }
    return std::make_tuple(true, changed);
}

bool TranNRSolver::revertNoise(double time, std::mt19937_64& gen) {
    bool changed = false;
    if (whiteBlock.revert(time, gen)) {
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


void TranNRSolver::buildNoiseResidual(double* noiseResidualContribution) {
    size_t atWhite = 0;
    size_t atFlicker = 0;

    auto whiteSamples = whiteBlock.values();

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
                noisePower.resize(nsCount);
                noiseExponent.resize(nsCount);
                inst->loadNoiseParameters(circuit, noisePower.data(), noiseExponent.data());
                // Go through noise sources
                for(decltype(nsCount) ndx=0; ndx<nsCount; ndx++) {
                    // Get noise source type
                    auto nstype = inst->noiseSourceType(ndx);
                    double sample = 0;
                    switch (nstype) {
                        case NoiseType::White:
                            // Scale with sqrt(PSD) because this is a time-domain sample
                            sample = whiteSamples[atWhite] * std::sqrt(noisePower[ndx]);
                            atWhite++;
                            break;
                        case NoiseType::Flicker:
                            atFlicker++;
                            break;
                        default:
                            continue;
                    }
                    // Get noise source terminals, add to residuals
                    auto [e1, e2] = inst->noiseExcitation(circuit, ndx);
                    noiseResidualContribution[e1] += sample;
                    noiseResidualContribution[e2] -= sample;
                }
            }
        }
    }
}

std::tuple<bool, bool> TranNRSolver::buildSystem(bool continuePrevious) {
    // First call the OpNRSolver method
    auto [ok, preventConvergence] = OpNRSolver::buildSystem(continuePrevious);

    // Now load the tranisent noise residuals
    if (ok) {
        if (circuit.simulatorOptions().core().tran_laggednoise) {
            auto n = jac.nRow();
            // Skip ground node contribution
            auto nrvec = noiseResidual.data();
            for(decltype(n) i=1; i<=n; i++) {
                loadSetup_.resistiveResidual[i] += nrvec[i];
            }
        } else {
            // Fully coupled noise residual
            buildNoiseResidual(loadSetup_.resistiveResidual);
        }
    }

    return std::make_tuple(ok, preventConvergence);
}

}
