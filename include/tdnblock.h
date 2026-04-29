#ifndef __TDNBLOCK_DEFINED
#define __TDNBLOCK_DEFINED

#include <unordered_map>
#include <vector>
#include <random>
#include <string>
#include "ansupport.h"
#include "value.h"
#include "common.h"


namespace NAMESPACE {

enum class ShapeSetStatus {
    Error, 
    Initialized, 
    Unchanged, 
    Changed, 
    OutOfRange
};

// Common API for a block of noise generators
template <std::uniform_random_bit_generator URBG> class TimeDomainNoiseBlock {
public:
    TimeDomainNoiseBlock() : debug_(0), atHistoric(0) {};
    virtual ~TimeDomainNoiseBlock() = default;

    TimeDomainNoiseBlock           (const TimeDomainNoiseBlock&)  = delete;
    TimeDomainNoiseBlock           (      TimeDomainNoiseBlock&&) = default;
    TimeDomainNoiseBlock& operator=(const TimeDomainNoiseBlock&)  = delete;
    TimeDomainNoiseBlock& operator=(      TimeDomainNoiseBlock&&) = delete;

    void reset(size_t count, int rollbackDepth) {
        history.upsize(rollbackDepth+1, count);
        // By default the initial sample is all-zero
        zero(history.at());
        atHistoric = 0;
    };

    // Return values corresponding to entry we are at
    std::vector<double>& values() { return history.at(atHistoric); };

    // Set shape parameters i-th generator (for now p is the exponent of flicker noise)
    // By default this method should never be called
    virtual ShapeSetStatus setShapeParameters(size_t i, double p) { return ShapeSetStatus::Error; };

    // time is the new time at which we should generate a sample
    // h is the timestep from last accepted point to the point with new time
    virtual bool advance(double time, double h, URBG& gen) = 0;
    virtual bool revert(double time, double h, URBG& gen) = 0;

    void setDebug(int debug) { debug_ = debug; };

protected:
    // Generate new random numbers
    // To be defined in derived classes
    virtual void generate(URBG&) = 0;  
    
    // Debug mode
    int debug_;
    
    // Which historic entry are we at.
    //   0 .. latest (n)
    //   1 .. previous (n-1)
    //   ---
    unsigned int atHistoric;
    // History
    VectorRepository<double> history;
};


// Noise generation at predetermined timepoints, zero-order hold
template <std::uniform_random_bit_generator URBG> class TimeDomainZohNoiseBlock : public TimeDomainNoiseBlock<URBG> {
public:
    TimeDomainZohNoiseBlock() {};
    virtual ~TimeDomainZohNoiseBlock() override = default;

    void reset(double t0, double timeStep, size_t count, int rollbackDepth) {
        TimeDomainNoiseBlock<URBG>::reset(count, rollbackDepth);
        t0_ = t0;
        timeStep_ = timeStep; 
        atSample_ = 0; 
    };

    // Compute sample index for ZOH generators
    size_t sampleIndex(double time) {
        return size_t(std::floor((time-t0_)/timeStep_));
    };

    virtual bool advance(double time, double h, URBG& gen) override;
    virtual bool revert(double time, double h, URBG& gen) override;

protected: 
    // Generate new random numbers, construct a sample
    // To be defined in derived classes
    virtual void generate(URBG&) = 0;  

    // ZOH generator support
    // Index of sample we are curretly at
    size_t atSample_;
    // Time between two consecutive samples
    double timeStep_;
    // Initial time
    double t0_;

    using TimeDomainNoiseBlock<URBG>::history;
    using TimeDomainNoiseBlock<URBG>::atHistoric;
};


// Noise generation at arbitrary timepoints, based on stochastic DEs
template <std::uniform_random_bit_generator URBG> class TimeDomainSdeNoiseBlock : public TimeDomainNoiseBlock<URBG> {
public:
    TimeDomainSdeNoiseBlock() {};
    virtual ~TimeDomainSdeNoiseBlock() override = default;

    void reset(double t0, size_t count, int rollbackDepth) {
        TimeDomainNoiseBlock<URBG>::reset(count, rollbackDepth);
        lastAcceptedTime = t0;
        atTime = t0;
    };

    virtual bool advance(double time, double h, URBG& gen) override;
    virtual bool revert(double time, double h, URBG& gen) override;

protected: 
    // Generate random numbers
    // To be defined in derived classes
    virtual void generate(URBG&) = 0;

    // Construct a sample
    // To be defined in derived classes
    virtual void construct(double h) = 0;

    // Random number storage is defined in derived classes
    // Sample is held in history. History is never rolled back. 
    // The newest sample is reconstructed from the previous one for each value of h. 

    // We remember the last accepted timepoint for sanity checking
    double lastAcceptedTime;

    // We also remember timepoint of current sample
    double atTime;

    using TimeDomainNoiseBlock<URBG>::history;
    using TimeDomainNoiseBlock<URBG>::atHistoric;
};


// Noise coefficients repository with coefficient tuning for given alpha
class TimeDomainNoiseCoeffs {
public:
    TimeDomainNoiseCoeffs() : debug_(0) {};

    TimeDomainNoiseCoeffs           (const TimeDomainNoiseCoeffs&)  = delete;
    TimeDomainNoiseCoeffs           (      TimeDomainNoiseCoeffs&&) = default;
    TimeDomainNoiseCoeffs& operator=(const TimeDomainNoiseCoeffs&)  = delete;
    TimeDomainNoiseCoeffs& operator=(      TimeDomainNoiseCoeffs&&) = delete;

    void reset(int k, double fmin, double fmax, int ptsPerDecade=10, int ni=100, int ns=5, double lr=0.1);
    
    void setDebug(int debug) { debug_ = debug; };

    // Looks up coefficients for exponent alpha, if not present, generates them with analytical formulae
    // Return value: index, inserted
    std::tuple<size_t, bool> getCoefficients(double alpha);

    // Get coefficients with index i
    std::vector<double>& getCoefficients(size_t i) {
        return data[i];
    };

protected:
    // Analytical coefficients
    virtual void analyticalCoefficients(double alpha, std::vector<double>& coeffs) = 0;

    // Target PSD for given frequency points
    virtual void computeTargetPsd(double alpha, std::vector<double>& target, const std::vector<double>& freq) = 0;

    // Optimize flicker coefficients for given frequency range
    bool optimizeCoefficients(double alpha, std::vector<double>& coeffs);

    // Optimizer helpers
    // Compute error root mean square dB
    double err(const std::vector<double>& target, const std::vector<double>& psd);

    // Compute PSD for k_ contributions at given f, using weights in wpsd. 
    // Store in contributions. Return sum of contributions. 
    virtual double computePsd(const std::vector<double>& wpsd, double f, std::vector<double>& contributions) = 0;
    
    // Compute PSDs for frequencies given in freq, use vector tmpContribs as temporary storage with k_ elements
    // Store PSDs in result. 
    void computePsds(const std::vector<double>& wpsd, const std::vector<double>& freq, std::vector<double>& tmpContribs, std::vector<double>& result);

    // Compute gradient of difference between actual and target PSD wrt log weights
    // Weights are given by wpsd, frequency points by freq. 
    // Store in gradient. Temporary storage for k_ contributions in vector tmpContribs. 
    // PSD for frequencies in freq stored in psd. 
    // Returns err() at wpsd. 
    double computeGradient(const std::vector<double>& wpsd, const std::vector<double>& freq, const std::vector<double>& target, std::vector<double>& tmpContribs, std::vector<double>& psd, std::vector<double>& gradient);

    // Number of coefficients
    int k_;
    // Coefficient data
    std::vector<std::vector<double>> data;
    // Map from flicker noise exponent into coefficients data
    std::unordered_map<double, size_t> coeffMap;

    // Optimizer settings
    // Frequency range to optimize
    double fmin_;
    double fmax_;
    // Point density used for computing error
    int ptsPerDecade_;
    // Gradient algorithm iterations
    int ni_;
    // Line search steps
    int ns_;
    // Step size
    double lr_;

    int debug_;
};



}

#endif
