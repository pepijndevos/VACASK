#ifndef __TDNWHITE_DEFINED
#define __TDNWHITE_DEFINED

#include "ansupport.h"
#include "tdnblock.h"
#include "common.h"


namespace NAMESPACE {

// White noise generators (count specifies the number of generators)
// All generators should use the same random generator (gen). 
// It is the user's responsibility to seed the generator.
template <std::uniform_random_bit_generator URBG> class TimeDomainWhiteNoise : public TimeDomainNoiseBlock<URBG> {
public:
    TimeDomainWhiteNoise() = default; 

    TimeDomainWhiteNoise           (const TimeDomainWhiteNoise&)  = delete;
    TimeDomainWhiteNoise           (      TimeDomainWhiteNoise&&) = default;
    TimeDomainWhiteNoise& operator=(const TimeDomainWhiteNoise&)  = delete;
    TimeDomainWhiteNoise& operator=(      TimeDomainWhiteNoise&&) = delete;

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

template <std::uniform_random_bit_generator URBG> void TimeDomainWhiteNoise<URBG>::generate(URBG& gen) {
    std::normal_distribution<double> norm(0.0, 1.0);
    for(auto& s : history.at(0)) {
        // Two-sided PSD of ZOH continuous-time signal at low frequencies is 
        //   sigma^2 * timeStep_ * sinc(pi f / fs)^2
        // Then one-sided PSD is 
        //   2 * sigma^2 * timeStep_* sinc(pi f / fs)^2
        // We want PSD = 1 for low frequencies (when sinc() has no effect))
        // we must divide the PSD with
        //   timeStep * 2
        // which means that we multiply the signal with 
        //   sqrt(timeStep * 2)
        s = norm(gen) / std::sqrt(2 * timeStep_ );
    }
}

template <std::uniform_random_bit_generator URBG> 
void TimeDomainWhiteNoise<URBG>::reset(double t0, double timeStep, size_t count, int rollbackDepth, URBG& gen) {
    TimeDomainNoiseBlock<URBG>::reset(t0, timeStep, count, rollbackDepth);
    // Generate random sample
    generate(gen);
};


}

#endif
