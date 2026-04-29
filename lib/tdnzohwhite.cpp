#include "tdnzohwhite.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

template <std::uniform_random_bit_generator URBG> void TimeDomainZohWhiteNoise<URBG>::generate(URBG& gen) {
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

// Explicit instantiation of template class
template class TimeDomainZohWhiteNoise<std::mt19937_64>;

}
