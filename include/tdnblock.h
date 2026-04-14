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

// Common API for a block of noise generators
template <std::uniform_random_bit_generator URBG> class TimeDomainNoiseBlock {
public:
    TimeDomainNoiseBlock() {};

    TimeDomainNoiseBlock           (const TimeDomainNoiseBlock&)  = delete;
    TimeDomainNoiseBlock           (      TimeDomainNoiseBlock&&) = default;
    TimeDomainNoiseBlock& operator=(const TimeDomainNoiseBlock&)  = delete;
    TimeDomainNoiseBlock& operator=(      TimeDomainNoiseBlock&&) = delete;

    void reset(double t0, double timeStep, size_t count, int rollbackDepth) {
        t0_ = t0;
        timeStep_ = timeStep; 
        atSample_ = 0; 
        atHistoric = 0;
        history.upsize(rollbackDepth+1, count);
        // By default the initial sample is all-zero
        zero(history.at());
    };

    // Return values corresponding to entry we are at
    std::vector<double>& values() { return history.at(atHistoric); };

    // Compute sample index
    size_t sampleIndex(double time) {
        return size_t(std::floor((time-t0_)/timeStep_));
    };

    bool advance(double time, URBG& gen);
    bool revert(double time, URBG& gen);

protected:
    // To be defined in derived classes
    virtual void generate(URBG&) = 0;  
    
    // Index of sample we are curretly at
    size_t atSample_;
    // Time between two consecutive samples
    double timeStep_;
    // Initial time
    double t0_;
    // Which historic entry are we at.
    //   0 .. latest (n)
    //   1 .. previous (n-1)
    //   ---
    unsigned int atHistoric;
    // History
    VectorRepository<double> history;
};

template <std::uniform_random_bit_generator URBG> bool TimeDomainNoiseBlock<URBG>::advance(double time, URBG& gen) {
    // Compute new sample number
    auto index = sampleIndex(time);

    // Is it beyond current sample number
    if (index == atSample_) {
        // Nothing to do
        return false;
    } else if (index < atSample_) {
        // Panic - advancing backward
        throw std::logic_error("Attempt to advance noise generator backward.");
    } else if (index == atSample_+1) {
        // Advancing by 1
        // Are we in past?
        if (atHistoric>0) {
            // Go forward, use history
            atHistoric -= 1;
        } else {
            // At latest generated sample, need new one
            history.advance();
            generate(gen);
        }
        atSample_ = index;
        return true;
    } else {
        // Panic - advancing by more than 1
        // This should never happen if the time step upper bound is set correctly
        throw std::logic_error("Attempt to advance noise generator by more than one sample.");
    }
    return false;
}

template <std::uniform_random_bit_generator URBG> bool TimeDomainNoiseBlock<URBG>::revert(double time, URBG& gen) {
    // Compute sample number to revert to
    auto index = sampleIndex(time);
    if (index == atSample_) {
        // Nothing to do
        return false;
    } else if (index > atSample_) {
        // Reverting forward, panic
        throw std::logic_error("Attempt to revert noise generator forward.");
    } else if (index == atSample_-1) {
        // Reverting by 1
        if (atHistoric>=history.size()-1) {
            // Panic, insufficient history
            throw std::logic_error("Insufficient history for reverting.");
        } else {
            // Go backward
            atHistoric -= 1;
        }
        atSample_ = index;
        return true;
    } else {
        // Panic - reverting by more than 1
        // This should never happen if the time step upper bound is set correctly
        throw std::logic_error("Attempt to revert noise generator by more than one sample.");
    }
    
    return false;
}

}

#endif
