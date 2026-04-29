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
    virtual ~TimeDomainZohWhiteNoise() override = default; 

    TimeDomainZohWhiteNoise           (const TimeDomainZohWhiteNoise&)  = delete;
    TimeDomainZohWhiteNoise           (      TimeDomainZohWhiteNoise&&) = default;
    TimeDomainZohWhiteNoise& operator=(const TimeDomainZohWhiteNoise&)  = delete;
    TimeDomainZohWhiteNoise& operator=(      TimeDomainZohWhiteNoise&&) = delete;

    using TimeDomainZohNoiseBlock<URBG>::reset;
    
private:
    // Generate new sample
    void generate(URBG& gen) override;  
    using TimeDomainNoiseBlock<URBG>::history;
    using TimeDomainZohNoiseBlock<URBG>::timeStep_;
};

}

#endif
