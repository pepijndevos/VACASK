#ifndef __TDNBLOCK_DEFINED
#define __TDNBLOCK_DEFINED

#include <vector>
#include <random>
#include <string>
#include "ansupport.h"
#include "common.h"


namespace NAMESPACE {

// Common API for a block of noise generators
class TimeDomainNoiseBlock {
public:
    TimeDomainNoiseBlock(double t0=0, double timeStep=1);

    void reset(double t0, double timeStep);

    template <std::uniform_random_bit_generator URBG> bool advance(double time, URBG& gen) { return false; };
    bool revert(double time) { return false; };
    std::vector<double>& values() { throw std::logic_error("Undefined values() method."); };

    // Compute sample index
    size_t sampleIndex(double time) {
        return size_t(std::floor((time-t0_)/timeStep_));
    };

    // Check if timeStep_ is below tolerance
    bool stepSanityCheck(double time) {
        return timeStep_ > timeRelativeTolerance*std::max(t0_, time);
    }

protected:
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
};


// White noise generators (count specifies the number of generators)
// All generators should use the same random generator (gen). 
// It is the user's responsibility to seed the generator.
class TimeDomainWhiteNoise : public TimeDomainNoiseBlock {
public:
    TimeDomainWhiteNoise(double t0=0, double timeStep=1);

    template <std::uniform_random_bit_generator URBG> void reset(double t0, double timeStep, size_t count, int rollbackDepth, URBG& gen);
    template <std::uniform_random_bit_generator URBG> bool advance(double time, URBG& gen);
    template <std::uniform_random_bit_generator URBG> bool revert(double time, URBG& gen);

    std::vector<double>& values() { return history.at(atHistoric); };

private:
    // Generate new sample
    template <std::uniform_random_bit_generator URBG> void generate(URBG& gen);

    VectorRepository<double> history;
};


}

#endif
