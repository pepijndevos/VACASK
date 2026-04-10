// Time-domain noise generator block, White noise generation (PSD=1)

#include <stdexcept>
#include "tdnblock.h"
#include "common.h"

namespace NAMESPACE {


TimeDomainNoiseBlock::TimeDomainNoiseBlock(double t0, double timeStep) {
    reset(t0, timeStep);
}

void TimeDomainNoiseBlock::reset(double t0, double timeStep) {
    timeStep_ = timeStep; 
    atSample_ = 0; 
    atHistoric = 0;
}


TimeDomainWhiteNoise::TimeDomainWhiteNoise(double t0, double timeStep) 
    : TimeDomainNoiseBlock(t0, timeStep) {
}

template <std::uniform_random_bit_generator URBG> void TimeDomainWhiteNoise::generate(URBG& gen) {
    std::normal_distribution<double> norm(0.0, 1.0);
    for(auto& s : history.at(0)) {
        // Two-sided PSD of ZOH continuous-time signal at low frequencies is 
        //   sigma^2 / timeStep_
        // Then one-sided PSD is 
        //   2 * sigma^2 / timeStep_
        // We want PSD = 1, therefore we must multiply with
        //   timeStep / 2
        s = norm(gen) * timeStep_ / 2;
    }
}

template <std::uniform_random_bit_generator URBG> void TimeDomainWhiteNoise::reset(double t0, double timeStep, size_t count, int rollbackDepth, URBG& gen) {
    TimeDomainNoiseBlock::reset(t0, timeStep);
    history.upsize(rollbackDepth, count);
    generate(gen);
};

template <std::uniform_random_bit_generator URBG> bool TimeDomainWhiteNoise::advance(double time, URBG& gen) {
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

template <std::uniform_random_bit_generator URBG> bool TimeDomainWhiteNoise::revert(double time, URBG& gen) {
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

template void TimeDomainWhiteNoise::reset<std::mt19937_64>(double, double, unsigned long, int, std::mt19937_64&);
template bool TimeDomainWhiteNoise::advance<std::mt19937_64>(double time, std::mt19937_64& gen);
template bool TimeDomainWhiteNoise::revert<std::mt19937_64>(double time, std::mt19937_64& gen);
template void TimeDomainWhiteNoise::generate<std::mt19937_64>(std::mt19937_64& gen);

}
