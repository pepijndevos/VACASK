#include "tdnsdewhite.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

template <std::uniform_random_bit_generator URBG> void TimeDomainSdeWhiteNoise<URBG>::generate(URBG& gen) {
    std::normal_distribution<double> norm(0.0, 1.0);
    for(auto& s : randomNumbers) {
        // Generate random from N(0,1)
        s = norm(gen);
    }
}

template <std::uniform_random_bit_generator URBG> void TimeDomainSdeWhiteNoise<URBG>::construct(double h) {
    auto sqrthinv = 1.0/std::sqrt(h);
    auto n = randomNumbers.size();
    auto& hvec = history.at();
    for(decltype(n) i=0; i<n; i++) {
        hvec[i] = randomNumbers[i]*sqrthinv;
    }
}

template <std::uniform_random_bit_generator URBG> 
void TimeDomainSdeWhiteNoise<URBG>::reset(double t0, size_t count, int rollbackDepth, URBG& gen) {
    TimeDomainSdeNoiseBlock<URBG>::reset(t0, count, rollbackDepth);
    
    // Make space for random numbers
    randomNumbers.resize(count);

    // Initial sample is all zeros, advance() will produce the first random sample
};

// Explicit instantiation of template class
template class TimeDomainSdeWhiteNoise<std::mt19937_64>;

}
