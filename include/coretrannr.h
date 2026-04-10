#ifndef __CORETRANNR_DEFINED
#define __CORETRANNR_DEFINED

#include "coreopnr.h"
#include "coretrancoef.h"
#include "tdnblock.h"
#include "common.h"


namespace NAMESPACE {

// Transient NR solver is almost identical to OP NR solver
// This one is used for all points, but the first one. 
class TranNRSolver : public OpNRSolver {
public:
    TranNRSolver(
        Circuit& circuit, CommonData& commons, KluRealMatrix& jac, 
        VectorRepository<double>& states, VectorRepository<double>& solution, 
        NRSettings& settings, IntegratorCoeffs& integCoeffs
    ); 

    // Called in the beginning of transient noise analysis
    void initializeNoise(double noiseStepLimit, std::mt19937_64& gen);
    
    // Called on accepted timepoint
    // Return value: step ok, sample index changed
    std::tuple<bool, bool> advanceNoise(double time, std::mt19937_64& gen);

    // Called on rejected timepoint
    bool revertNoise(double time, std::mt19937_64& gen);
    
    virtual bool initialize(bool continuePrevious);

    // No need to override buildSysten() and computeResidual() to set 
    // nodeset and ic flags to false because 
    // nodeset flag is off due to continue mode and 
    // ic flag is off due to forces slot 2 not being present. 
    // Override buildSystem() for loading trasient noise residuals. 
    virtual std::tuple<bool, bool> buildSystem(bool continuePrevious);
    
private:
    void buildNoiseResidual(double* noiseResidualContribution);
    
    IntegratorCoeffs* integCoeffs;

    // Transient noise
    RealVector noisePower;
    RealVector noiseExponent;
    TimeDomainWhiteNoise whiteBlock;
    VectorRepository<double> noiseResidual;
    int reverted;
};

}

#endif
