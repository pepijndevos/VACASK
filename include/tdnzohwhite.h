#ifndef __TDNWHITE_DEFINED
#define __TDNWHITE_DEFINED

#include "ansupport.h"
#include "tdnblock.h"
#include "common.h"


namespace NAMESPACE {

// White noise generators (count specifies the number of generators)
// All generators should use the same random generator (gen). 
// It is the user's responsibility to seed the generator.
template <std::uniform_random_bit_generator URBG> class TimeDomainZohWhiteNoise : public TimeDomainNoiseBlock<URBG> {
public:
    TimeDomainZohWhiteNoise() = default; 

    TimeDomainZohWhiteNoise           (const TimeDomainZohWhiteNoise&)  = delete;
    TimeDomainZohWhiteNoise           (      TimeDomainZohWhiteNoise&&) = default;
    TimeDomainZohWhiteNoise& operator=(const TimeDomainZohWhiteNoise&)  = delete;
    TimeDomainZohWhiteNoise& operator=(      TimeDomainZohWhiteNoise&&) = delete;

    using TimeDomainNoiseBlock<URBG>::reset;
    void reset(double t0, double timeStep, size_t count, int rollbackDepth, URBG& gen);
    // template <std::uniform_random_bit_generator URBG> bool advance(double time, URBG& gen);
    // template <std::uniform_random_bit_generator URBG> bool revert(double time, URBG& gen);

private:
    // Generate new sample
    void generate(URBG& gen) override;  
    using TimeDomainNoiseBlock<URBG>::history;
    using TimeDomainNoiseBlock<URBG>::timeStep_;
};

}

#endif
