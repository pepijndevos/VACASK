#ifndef __CORETRANNR_DEFINED
#define __CORETRANNR_DEFINED

#include "coreopnr.h"
#include "coretrancoef.h"
#include "tdnzohwhite.h"
#include "tdnzohflicker.h"
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

    enum class TranNRSolverError {
        OK, 
        BadFlickerExponent, 
        FlickerExponentChanged, 
    };

    // Clear error
    void clearError() { OpNRSolver::clearError(); lastTranNRError = TranNRSolverError::OK; }; 

    // Format error, return false on error - this function is not cheap (works with strings)
    bool formatError(Status& s=Status::ignore, NameResolver* resolver=nullptr) const; 

    // Called in the beginning of transient noise analysis
    // Takes ownership of noise blocks
    void enableNoise(
        TimeDomainNoiseBlock<std::mt19937_64>& white, 
        TimeDomainNoiseBlock<std::mt19937_64>& flicker, 
        size_t maxNsCount, 
        double noiseScale=1.0
    );

    // Disable noise
    void disableNoise();

    // Collect noise scaling
    bool collectNoiseScaling();

    // Build noise residual. We expose this for noise generator coefficient initalization. 
    // Return value: ok
    bool buildNoiseResidual();
    
    // Called on accepted timepoint
    // Return value: ok, sample index changed
    std::tuple<bool, bool> advanceNoise(double time, double h, std::mt19937_64& gen);

    // Called on rejected timepoint
    bool revertNoise(double time, double h, std::mt19937_64& gen);

    // Compute the negative of noise contribution to the solution, store it in noise residual. 
    // It is the linearized contribution computed with the last factored Jacobian. 
    // Can be called after the solver succeeds. Call it only once because it overwrites 
    // noiseResidual which is the RHS for the solver. 
    // Returns true on success
    bool computeNoiseSolutionContribution();

    // Returns RealVector (solution contribution of noise)
    const RealVector& noiseSolutionContribution() { return noiseResidual; };

    // Override method for deciding which residual to check
    virtual void rebuildCheckResidualFlags() override;

    virtual bool initialize(bool continuePrevious) override;

    // No need to override buildSysten() and computeResidual() to set 
    // nodeset and ic flags to false because 
    // nodeset flag is off due to continue mode and 
    // ic flag is off due to forces slot 2 not being present. 
    // Override buildSystem() for loading trasient noise residuals. 
    virtual std::tuple<bool, bool> buildSystem(bool continuePrevious) override;

    // Need to obverride this because in transient noise we include noise residual
    // in tolerance reference
    virtual std::tuple<bool, bool> checkResidual() override;
    
private:
    IntegratorCoeffs* integCoeffs;

    bool noiseEnabled;
    TimeDomainNoiseBlock<std::mt19937_64>* whiteBlock;
    TimeDomainNoiseBlock<std::mt19937_64>* flickerBlock;
    RealVector whiteScaling;
    RealVector flickerScaling;
    RealVector noiseResidual;
    double noiseScale_;
    
    // Shared mutable scratch for noise loading.
    // Pre-OpenMP gate: before enabling parallel evaluation, audit TranNRSolver for
    // shared mutable state and convert these (and any peers added since) to
    // thread-local storage. Today's single-threaded use is correct.
    RealVector noisePower;
    RealVector noiseExponent;

protected:
    TranNRSolverError lastTranNRError;
    Instance* errorInstance;
};

}

#endif
