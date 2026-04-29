#include "tdnsdeflicker.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

// Sum of Lorentzians resulting in a single-sided PSD of the form 1/f^alpha
// Each contribution is a Lorentzian with corner frequency fc. 
// Corner frequencies are fmax, fmax/2, fmax/4, ... 
// Square these coeffs to get the PSD coefficients. 
// i=0    .. highest corner frequency
// i=k_-1 .. lowest corner frequency
//
// f_i = 2^(-i) fmax
//
// w_i = sqrt( 2 ln(2) sin(pi alpha / 2) / pi f_i^(-alpha) )
//     = sqrt( 2 ln(2) sin(pi alpha / 2) fmax^(-alpha) / pi ) 2^(alpha i / 2) 
//
// Produces one-sided PSD in midband:
// S = f^(-alpha)
void SdeFlickerCoeffs::analyticalCoefficients(double alpha, std::vector<double>& coeffs) {
    const double pi = std::numbers::pi;
    // Scaling that makes one-sided PSD 1/f^alpha
    double scaling = 2*std::log(2)*std::sin(pi*alpha/2)*std::pow(fmax_, -alpha)/pi;
    // Signal scaling is the square root of PSD scaling
    scaling = std::sqrt(scaling);
    // Scaled signal coefficients
    for(int i=0; i<k_; i++) {
        coeffs[i] = std::pow(2.0, i*alpha/2.0) * scaling;
    }
}

// Target PSD: S = f^(-alpha)
bool SdeFlickerCoeffs::computeTargetPsd(double alpha, std::vector<double>& target, const std::vector<double>& freq) {
    return false;
    auto n = freq.size();
    for(int i=0; i<n; i++) {
        target[i] = 1.0/std::pow(freq[i], alpha);
    }
    return true;
}

// Compute PSD for given PSD coeffs at given frequency
double SdeFlickerCoeffs::computePsd(const std::vector<double>& wpsd, double f, std::vector<double>& contributions) {
    return 0;
}

template <std::uniform_random_bit_generator URBG> bool TimeDomainSdeFlickerNoise<URBG>::advance(double time, double h, URBG& gen) {
    lorentzianHistory.advance();
    return TimeDomainSdeNoiseBlock<URBG>::advance(time, h, gen);
}

template <std::uniform_random_bit_generator URBG> void TimeDomainSdeFlickerNoise<URBG>::generate(URBG& gen) {
    std::normal_distribution<double> norm(0.0, 1.0);
    for(auto& s : randomNumbers) {
        s = norm(gen);
    }
}

template <std::uniform_random_bit_generator URBG> void TimeDomainSdeFlickerNoise<URBG>::construct(double h) {
    const double pi = std::numbers::pi;

    // Get history
    auto& newSample = history.at();
    auto& oldSample = history.at(1);

    // Get Lorentzian history
    auto* newLor = lorentzianHistory.data();
    auto* oldLor = lorentzianHistory.data(1);

    // Get random numbers
    auto randoms = randomNumbers.data();
    
    // Go through all Lorentzians, compute a and b (weights of old and new sample) for each Lorentzian
    for(int i=0; i<k_; i++) {
        a[i] = std::exp(-2*omega[i]*h);
        b[i] = std::sqrt((1-a[i]*a[i])/(2*omega[i]));
    }

    // It makes more sense to go through all generators in outer loop (easier on cache)
    auto count = newSample.size();
    for(decltype(count) i=0; i<count; i++) {
        // New sample, start with 0
        double sam = 0;
        // Loop through Lorentzians
        for(int j=0; j<k_; j++) {
            // Compute old Lorentzian sample contribution
            auto eta = a[j] * (*oldLor);
            // Add random number contribution
            eta += b[j] * (*randoms);
            // Store new Lorentzian sample
            *newLor = eta;
            // Add to new sample
            sam += eta;
            // Move on
            newLor++;
            oldLor++;
            randoms++;
        }
        // Store new sample
        newSample[i] = sam;
    }
}

template <std::uniform_random_bit_generator URBG> 
void TimeDomainSdeFlickerNoise<URBG>::reset(double t0, size_t count, int rollbackDepth, int k, double fmax) {
    k_ = k;
    fmax_ = fmax;

    // Make space for random numbers
    randomNumbers.resize(count*k_);

    // Make space for Lorentzian history
    lorentzianHistory.upsize(2, count*k_);

    // Make space for auxiliary data
    a.resize(k);
    b.resize(k);
    omega.resize(k);

    // Compute corner frequencies of Lorentzians
    const double pi = std::numbers::pi;
    auto tmp = 2*pi*fmax_;
    for(int i=0; i<k_; i++) {
        omega[i] = tmp;
        tmp /= 2;
    }

    TimeDomainSdeNoiseBlock<URBG>::reset(t0, count, rollbackDepth);
    
    // Initial sample is all zeros, advance() will produce the first random sample
};

// Explicit instantiation of template class
template class TimeDomainSdeFlickerNoise<std::mt19937_64>;

}
