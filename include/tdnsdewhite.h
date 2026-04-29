#ifndef __TDNSDEWHITE_DEFINED
#define __TDNSDEWHITE_DEFINED

#include "ansupport.h"
#include "tdnblock.h"
#include "common.h"

namespace NAMESPACE {

// Stochastice DE-based white noise generator block
template <std::uniform_random_bit_generator URBG> class TimeDomainSdeWhiteNoise : public TimeDomainSdeNoiseBlock<URBG> {
public:
    TimeDomainSdeWhiteNoise() = default; 

    TimeDomainSdeWhiteNoise           (const TimeDomainSdeWhiteNoise&)  = delete;
    TimeDomainSdeWhiteNoise           (      TimeDomainSdeWhiteNoise&&) = default;
    TimeDomainSdeWhiteNoise& operator=(const TimeDomainSdeWhiteNoise&)  = delete;
    TimeDomainSdeWhiteNoise& operator=(      TimeDomainSdeWhiteNoise&&) = delete;

    using TimeDomainSdeNoiseBlock<URBG>::reset;
    void reset(double t0, size_t count, int rollbackDepth, URBG& gen);

private:
    // Generate new sample
    void generate(URBG& gen) override;  
    void construct(double h) override;  
    
    RealVector randomNumbers;

    using TimeDomainNoiseBlock<URBG>::history;
};
}

#endif
