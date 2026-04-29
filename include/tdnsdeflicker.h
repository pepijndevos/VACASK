#ifndef __TDNSDEFLICKER_DEFINED
#define __TDNSDEFLICKER_DEFINED

#include "ansupport.h"
#include "tdnblock.h"
#include "common.h"

namespace NAMESPACE {

// SDE flicker noise coefficients
class SdeFlickerCoeffs : public TimeDomainNoiseCoeffs {
public:
    SdeFlickerCoeffs() {};
    virtual ~SdeFlickerCoeffs() override = default;

    SdeFlickerCoeffs           (const SdeFlickerCoeffs&)  = delete;
    SdeFlickerCoeffs           (      SdeFlickerCoeffs&&) = default;
    SdeFlickerCoeffs& operator=(const SdeFlickerCoeffs&)  = delete;
    SdeFlickerCoeffs& operator=(      SdeFlickerCoeffs&&) = delete;

    using TimeDomainNoiseCoeffs::reset;
    
protected:
    virtual void analyticalCoefficients(double alpha, std::vector<double>& coeffs) override;
    virtual bool computeTargetPsd(double alpha, std::vector<double>& target, const std::vector<double>& freq) override;
    virtual double computePsd(const std::vector<double>& wpsd, double f, std::vector<double>& contributions) override;
};

// Stochastice DE-based flicker noise generator block
template <std::uniform_random_bit_generator URBG> 
class TimeDomainSdeFlickerNoise : public TimeDomainSdeNoiseBlock<URBG>, public SdeFlickerCoeffs {
public:
    TimeDomainSdeFlickerNoise() = default; 
    virtual ~TimeDomainSdeFlickerNoise() override = default; 

    TimeDomainSdeFlickerNoise           (const TimeDomainSdeFlickerNoise&)  = delete;
    TimeDomainSdeFlickerNoise           (      TimeDomainSdeFlickerNoise&&) = default;
    TimeDomainSdeFlickerNoise& operator=(const TimeDomainSdeFlickerNoise&)  = delete;
    TimeDomainSdeFlickerNoise& operator=(      TimeDomainSdeFlickerNoise&&) = delete;

    using TimeDomainSdeNoiseBlock<URBG>::reset;
    void reset(double t0, size_t count, int rollbackDepth, int k, double fmax);
    void resetOptimizer(double fmin, double fmax, int ptsPerDecade=10, int ni=100, int ns=5, double lr=0.1) {
        SdeFlickerCoeffs::reset(k_, fmin, fmax, ptsPerDecade, ni, ns, lr);
    };

    void setDebug(int debug) { 
        TimeDomainNoiseBlock<URBG>::setDebug(debug);
        SdeFlickerCoeffs::setDebug(debug);
    };

    // Need to override because we must also advance lorentzianHistory
    virtual bool advance(double time, double h, URBG& gen) override;

private:
    // Generate new sample
    void generate(URBG& gen) override;  
    void construct(double h) override;  
    
    // A vector of random numbers, k_ for each source
    RealVector randomNumbers;

    int k;
    double fmax_;

    // Auxiliary data for constructing a sample
    RealVector a;
    RealVector b;
    RealVector omega;

    // For each Lorentzian we need the previous and the new value
    // Each source has k_ Lorentzians
    // Organize this as a repository with 2 vectors (previous, new), each vector holds the
    // - Lorentzians of first source
    // - Lorentzians of second source
    // ...
    VectorRepository<double> lorentzianHistory;

    // History of noise sources
    using TimeDomainNoiseBlock<URBG>::history;
};
}

#endif
