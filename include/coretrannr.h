#ifndef __CORETRANNR_DEFINED
#define __CORETRANNR_DEFINED

#include "coreopnr.h"
#include "coretrancoef.h"
#include "tdnwhite.h"
#include "tdnflicker.h"
#include "common.h"


namespace NAMESPACE {

// Transient NR solver is almost identical to OP NR solver
// This one is used for all points, but the first one. 
class TranNRSolver : public OpNRSolver {
public:
    TranNRSolver(
        Circuit& circuit, CommonData& commons, KluRealMatrix& jac, 
        VectorRepository<double>& states, VectorRepository<double>& solution, 
        NRSettings& settings, IntegratorCoeffs& integCoeffs, 
        VMCoefficientsRepository& vmCoeffs
    ); 

    enum class TranNRSolverError {
        OK, 
        BadFlickerExponent, 
        FlickerExponentChanged, 
    };

    // Clear error
    void clearError() { OpNRSolver::clearError(); lastTranNRError = TranNRSolverError::OK; }; 

    // Format error, return false on error - this function is not cheap (works with strings)
    bool formatError(Status& s=Status::ignore, NameResolver* resolver=nullptr) const; 

    // Disable noise
    void disableNoise() { noiseEnabled = false; };

    // Called in the beginning of transient noise analysis
    void initializeNoise(double noiseStepLimit, std::mt19937_64& gen);

    // Build noise residual. We expose this for noise generator coefficient initalization. 
    // Return value: ok
    bool buildNoiseResidual(double* noiseResidualContribution);
    
    // Called on accepted timepoint
    // Return value: ok, sample index changed
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
    IntegratorCoeffs* integCoeffs;

    bool noiseEnabled;
    TimeDomainWhiteNoise<std::mt19937_64> whiteBlock;
    TimeDomainFlickerNoise<std::mt19937_64> flickerBlock;
    VectorRepository<double> noiseResidual;
    int reverted;
    size_t maxNsCount;

protected:
    TranNRSolverError lastTranNRError;
    Instance* errorInstance;
};

}

#endif
