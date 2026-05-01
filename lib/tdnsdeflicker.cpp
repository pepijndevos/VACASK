#include "tdnsdeflicker.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

// We approximate PSD = f^(-alpha) as a sum of Lorentzians
//   PSD = sum_i w_i^2 / (1+(f/f_i)^2) for i=0..k-1
// with corner frequencies
//   f_i = 2^(-i) fmax
// Signal weights are
//   w_i^2 = 2 sin(pi alpha / 2) ln(2) / pi f_i^(-alpha)
//         = 2 sin(pi alpha / 2) ln(2) fmax^(-alpha) / pi 2^(alpha i)
void SdeFlickerCoeffs::analyticalCoefficients(double alpha, std::vector<double>& coeffs) {
    const double pi = std::numbers::pi;
    // Scaling that makes one-sided PSD 1/f^alpha
    double scaling = 2*std::log(2)*std::sin(pi*alpha/2)*std::pow(fmax_, -alpha)/pi;
    // Signal scaling is the square root of PSD scaling
    scaling = std::sqrt(scaling);
    // Signal coefficients
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

template <std::uniform_random_bit_generator URBG> 
ShapeSetStatus TimeDomainSdeFlickerNoise<URBG>::setShapeParameters(size_t i, double p) {
    auto e = p;
    if (e<0.1 || e>1.9) {
        return ShapeSetStatus::OutOfRange;
    }
    bool init = coeffIndex[i]==SIZE_T_MAX;
    bool changed = false;
    bool compute = init || changed;
    if (compute) {
        auto [newIndex, ok] = getCoefficients(e);
        coeffIndex[i] = newIndex;
        exponent[i] = e;
        return init ? ShapeSetStatus::Initialized : ShapeSetStatus::Changed;
    }
    return ShapeSetStatus::Unchanged;
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
    
    // A single Lorentzian from analyticalCoefficients()
    //   PSD = w_i^2 / (1+(f/f_i)^2)
    //
    // Match it to the one-sided PSD of x obtained from 
    //   dx = - omega_i x dt + sigma dW   where W is a Wiener process
    // 
    //   w_i^2 / (1 + (f/f_i)^2) = 2 sigma^2 / ((2 pi f_i)^2 + (2 pi f)^2)
    // 
    // We get
    //   sigma = sqrt(2) pi f_i w_i
    //
    // x(h) can be expressed with x(0) as
    //   x(h) = a x(0) + b N(0,1)
    //
    //   a = exp(-omega_i h) = exp(-2 pi f_i h)
    //   b = sigma sqrt((1-a^2)/(2 omega_i))
    //     = sqrt((1-a^2)/(2 pi f_i)) pi f_i w_i 
    //     = sqrt(pi f_i (1-a^2)/2) w_i 
    //     = sqrt(2 pi f_i (1-a^2)/4) w_i 
    //     = sqrt(omega_i (1-a^2))/2 w_i 

    // Go through all Lorentzians, compute a and b for each Lorentzian
    for(int i=0; i<k_; i++) {
        a[i] = std::exp(-omega[i]*h);
        // b without the weight
        b[i] = std::sqrt((1-a[i]*a[i])*omega[i])/2;
    }

    // It makes more sense to go through all generators in outer loop (easier on cache)
    auto count = newSample.size();
    for(decltype(count) i=0; i<count; i++) {
        // Get coefficients
        auto& coeffs = getCoefficients(coeffIndex[i]);

        // New sample, start with 0
        double sam = 0;
        // Loop through Lorentzians
        for(int j=0; j<k_; j++) {
            // Compute old Lorentzian sample contribution
            auto eta = a[j] * (*oldLor);
            // Add random number contribution, multiply with weight
            eta += coeffs[j] * b[j] * (*randoms);
            // Store new Lorentzian sample
            *newLor = eta;
            // Add to new flicker sample
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

    // Exponents for generators
    exponent.resize(count);
    
    // Coefficints index in coefficients repository
    coeffIndex.resize(count);

    // Coeff index set to SIZE_T_MAX initially so that we can detect initalization
    std::fill(coeffIndex.begin(), coeffIndex.end(), SIZE_T_MAX);
    
    // Make space for random numbers
    randomNumbers.resize(count*k_);

    // Make space for Lorentzian history
    lorentzianHistory.upsize(2, count*k_);

    // Generated values
    history.upsize(rollbackDepth+1, count);

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
