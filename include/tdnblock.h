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
    TimeDomainNoiseBlock() {};
    virtual ~TimeDomainNoiseBlock() = default;

    TimeDomainNoiseBlock           (const TimeDomainNoiseBlock&)  = delete;
    TimeDomainNoiseBlock           (      TimeDomainNoiseBlock&&) = default;
    TimeDomainNoiseBlock& operator=(const TimeDomainNoiseBlock&)  = delete;
    TimeDomainNoiseBlock& operator=(      TimeDomainNoiseBlock&&) = delete;

    void reset(size_t count, int rollbackDepth) {
        history.upsize(rollbackDepth+1, count);
        // By default the initial sample is all-zero
        zero(history.at());
    };

    // Return values corresponding to entry we are at
    std::vector<double>& values() { return history.at(atHistoric); };

    // Set shape parameters i-th generator (for now p is the exponent of flicker noise)
    // By default this method should never be called
    virtual ShapeSetStatus setShapeParameters(size_t i, double p) { return ShapeSetStatus::Error; };

    // time is the new time at which we should generate a sample
    virtual bool advance(double time, URBG& gen) = 0;
    virtual bool revert(double time, URBG& gen) = 0;

    void setDebug(int debug) { debug_ = debug; };

protected:
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

    virtual bool advance(double time, URBG& gen) override;
    virtual bool revert(double time, URBG& gen) override;

protected: 
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


}

#endif
