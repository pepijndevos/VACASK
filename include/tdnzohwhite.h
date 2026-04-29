#ifndef __TDNWHITE_DEFINED
#define __TDNWHITE_DEFINED

#include "ansupport.h"
#include "tdnblock.h"
#include "common.h"


namespace NAMESPACE {

// Zero-order hold white noise generator block
template <std::uniform_random_bit_generator URBG> class TimeDomainZohWhiteNoise : public TimeDomainZohNoiseBlock<URBG> {
public:
    TimeDomainZohWhiteNoise() = default; 

    TimeDomainZohWhiteNoise           (const TimeDomainZohWhiteNoise&)  = delete;
    TimeDomainZohWhiteNoise           (      TimeDomainZohWhiteNoise&&) = default;
    TimeDomainZohWhiteNoise& operator=(const TimeDomainZohWhiteNoise&)  = delete;
    TimeDomainZohWhiteNoise& operator=(      TimeDomainZohWhiteNoise&&) = delete;

    using TimeDomainZohNoiseBlock<URBG>::reset;
    void reset(double t0, double timeStep, size_t count, int rollbackDepth, URBG& gen);

private:
    // Generate new sample
    void generate(URBG& gen) override;  
    using TimeDomainNoiseBlock<URBG>::history;
    using TimeDomainZohNoiseBlock<URBG>::timeStep_;
};

}

#endif
