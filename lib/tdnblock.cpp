#include "tdnblock.h"
#include "common.h"

namespace NAMESPACE {

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

// Explicit instantiation of template class
template class TimeDomainNoiseBlock<std::mt19937_64>;

}


