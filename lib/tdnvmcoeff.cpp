#include "tdnblock.h"
#include "common.h"

namespace NAMESPACE {

void VMCoefficientsRepository::reset(int k, double fs) {
    k_ = k;
    fs_ = fs;
    data.clear();
    flickerMap
    
    .clear();
}

std::tuple<size_t, bool> VMCoefficientsRepository::get(double alpha) {
    auto it = flickerMap.find(alpha);
    if (it!=flickerMap.end()) {
        // Already in map
        return std::make_tuple(it->second, false);
    } 
    
    // Not in map, insert
    auto index = data.size();
    flickerMap.insert({alpha, index});
    
    // Build coefficients
    auto& coeffs = data.emplace_back(k_);

    // One-sided PSD of the sum of all weighted rows must be 1/f^alpha
    // Row 0 corresponds to update probability 1/2
    // PSD coefficient for row i: fs*2^(i*(alpha-1))
    // Signal coefficient: sqrt(fs*2^(i*(alpha-1)))
    //
    // Two-sided asymptotic ZOH PSD: fs^(1+alpha)*pi*2^(1-alpha)/(ln(2)*sin(pi*alpha/2)*(2*pi)^alpha)) * 1/fâlpha * sinc(pi*f/fs)
    // One-sided asymptotic ZOH PSD: 2*fs^(1+alpha)*pi*2^(1-alpha)/(ln(2)*sin(pi*alpha/2)*(2*pi)^alpha)) * 1/f^alpha * sinc(pi*f/fs)
    //
    // We want one-sided ZOH PSD to be 1/f^alpha * sinc(pi*f/fs)
    // We need to divide PSD with: 2*fs^(1+alpha)*pi*2^(1-alpha)/(ln(2)*sin(pi*alpha/2)*(2*pi)^alpha))
    // We need to divide signal coefficients with:
    //   sqrt(2*fs^(1+alpha)*pi*2^(1-alpha)/(ln(2)*sin(pi*alpha/2)*(2*pi)^alpha)))
    const double pi = std::numbers::pi;
    double scaling = 2*std::pow(fs_, 1+alpha)*pi*std::pow(2, 1-alpha) / 
        (std::log(2)*std::sin(pi*alpha/2)*std::pow(2*pi, alpha));
    scaling = std::sqrt(scaling);
    for(int i=0; i<k_; i++) {
        coeffs[i] = std::sqrt(fs_*std::pow(2, i*(alpha-1))) / scaling;
    }

    return std::make_tuple(index, true);
}

}
