#include "tdnflicker.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

void VMCoefficientsRepository::reset(int k, double fs, int oversample, double fmin, double fmax, int ptsPerDecade, int ni, int ns, double lr) {
    k_ = k;
    fs_ = fs;
    oversample_ = oversample;
    data.clear();
    flickerMap.clear();
    fmin_ = fmin;
    fmax_ = fmax;
    ptsPerDecade_ = ptsPerDecade;
    ni_ = ni;
    ns_ = ns;
    lr_ = lr;
}

std::tuple<size_t, bool> VMCoefficientsRepository::get(double alpha) {
    auto it = flickerMap.find(alpha);
    if (it!=flickerMap.end()) {
        // Already in map
        return std::make_tuple(it->second, false);
    } 

    if (debug_) {
        Simulator::dbg() << "Computing flicker noise coefficients for alpha=" << alpha << "\n";
    }

    // Not in map, insert
    auto index = data.size();
    flickerMap.insert({alpha, index});
    
    // Build coefficients
    auto& coeffs = data.emplace_back(k_);

    // One-sided PSD of the sum of all weighted rows must be 1/f^alpha
    // Row 0 corresponds to update probability 1/2
    // PSD coefficient for row i: fs*2^(i*(alpha-1))
    // Signal coefficient: fs*2^(i*(alpha-1)/2)
    //
    // Two-sided asymptotic ZOH PSD: fs^(1+alpha)*pi*2^(1-alpha)/(ln(2)*sin(pi*alpha/2)*(2*pi)^alpha)) * 1/f^alpha * sinc(pi*f/fs)
    // One-sided asymptotic ZOH PSD: 2*fs^(1+alpha)*pi*2^(1-alpha)/(ln(2)*sin(pi*alpha/2)*(2*pi)^alpha)) * 1/f^alpha * sinc(pi*f/fs)
    //
    // We want one-sided ZOH PSD to be 1/f^alpha * sinc(pi*f/fs)
    // We need to divide PSD with: 2*fs^(1+alpha)*pi*2^(1-alpha)/(ln(2)*sin(pi*alpha/2)*(2*pi)^alpha))
    // We need to divide signal coefficients with:
    //   sqrt(2*fs^(1+alpha)*pi*2^(1-alpha)/(ln(2)*sin(pi*alpha/2)*(2*pi)^alpha)))
    const double pi = std::numbers::pi;
    double scaling = 2*std::pow(fs_, -1.0+alpha)*pi*std::pow(2.0, 1.0-alpha) / 
        (std::log(2)*std::sin(pi*alpha/2)*std::pow(2*pi, alpha));
    scaling = std::sqrt(scaling);
    for(int i=0; i<k_; i++) {
        coeffs[i] = std::pow(2.0, i*(alpha-1.0)/2.0) / scaling;
    }

    // fs**(-1+alpha)*np.pi*2**(1-alpha)/(np.log(2)*np.sin(np.pi*alpha/2))*1.0/(2*np.pi*f)**alpha
    // alpha=1, f=1
    // 1/(2*np.log(2))

    // Optimize coeffs
    auto ok = optimizeCoefficients(index, alpha);

    return std::make_tuple(index, true);
}

double VMCoefficientsRepository::err(const std::vector<double>& target, const std::vector<double>& psd) {
    auto n = target.size();
    double sum = 0;
    for(decltype(n) i=0; i<n; i++) {
        auto q = 10*std::log(psd[i]/target[i])/std::log(10);
        sum += q*q;
    }
    return std::sqrt(sum/n);
}

double VMCoefficientsRepository::computePsd(const std::vector<double>& wpsd, double f, double& zoh, std::vector<double>& rows) {
    // Return value is the ZOH scaled PSD
    // PSDs of rows assume rows generate scaled impulse trains
    const double pi = std::numbers::pi;
    auto n = wpsd.size();

    // Zero-order hold TF
    auto zohArg = pi*f/fs_;
    zoh = 1/fs_*std::sin(zohArg)/zohArg;

    // sin(pi f / fs)
    auto s = std::sin(pi*f/fs_);
    
    // Compute rows
    auto p = 0.5;
    double sum = 0;
    for(decltype(n) i=0; i<n; i++) {
        // Factor 2 comes from one-sided PSD
        auto row = 2*fs_*p*(2-p)/(p*p+4*(1-p)*s*s);
        rows[i] = row;
        sum += wpsd[i] * row;
        p /= 2;
    }
    
    return sum*zoh*zoh;
}

// wpsd are PSD coeffs, i.e. squared signal coeffs
void VMCoefficientsRepository::computePsds(const std::vector<double>& wpsd, const std::vector<double>& freq, std::vector<double>& tmpRows, std::vector<double>& result) {
    // result will hold the PSD of generator output
    const double pi = std::numbers::pi;
    auto nf = freq.size();
    auto n = wpsd.size();
    double sum = 0;
    double zoh;
    zero(result);
    for(decltype(nf) j=0; j<nf; j++) {
        auto f = freq[j];
        result[j] = computePsd(wpsd, f, zoh, tmpRows);
    }
}

double VMCoefficientsRepository::computeGradient(const std::vector<double>& wpsd, const std::vector<double>& freq, const std::vector<double>& target, std::vector<double>& tmpRows, std::vector<double>& psd, std::vector<double>& gradient) {
    // We are minimizing 
    //   F = sum_i (ln(psd_i/target_i))^2
    // where psd_i is the psd at ith frequency. 
    //   psd_i = sum_j w_j row_j(f_i) * zoh * zoh
    //
    // Weights (w) are PSD weights (squared signal weights). 
    // 
    // Logarithms of weights (wl = ln(w)) are the unknowns s.t. optimization, i.e. 
    //   w = exp(wl)
    //   wl = ln(w)
    //   dwl/dw = 1/w
    //
    // Compute dF/dwl
    // 
    // dF/dwl = sum_i ( 2 ln(psd_i/target_i) target_i/psd_i 1/target_i dpsd_i/dw dw/dwl )
    //        = sum_i ( 2 ln(psd_i/target_i) 1/psd_i dpsd_i/dw w )
    //
    // dpsd_i/dw = row_i * zoh * zoh
    //
    // dF/dwl = sum_i ( 2 ln(psd_i/target_i) 1/psd_i row_i w )
    // 
    // row corresponds to weight w. 
    const double pi = std::numbers::pi;

    auto nc = wpsd.size();
    auto nf = freq.size();

    // PSD at all frequencies
    computePsds(wpsd, freq, tmpRows, psd);

    // Start with zero gradient
    zero(gradient);

    // Go through all freq points
    for(decltype(nf) i=0; i<nf; i++) {
        auto f = freq[i];

        // Storage for ZOH TF
        double zoh;
        
        // Compute PSD at f and the values of rows
        auto psd_i = computePsd(wpsd, f, zoh, tmpRows);
        auto ldiff = std::log(psd_i/target[i]);
        auto fact = 2*ldiff/psd_i;
        psd[i] = psd_i;
        
        // Go through all weights
        for(decltype(nc) iw=0; iw<nc; iw++) {
            auto dF_dw = fact * (tmpRows[iw] * zoh * zoh) * wpsd[iw];
            gradient[iw] += dF_dw;
        }
    }
    
    return err(target, psd);
}

bool VMCoefficientsRepository::optimizeCoefficients(size_t index, double alpha) {
    // Build vector of frequency points and ideal PSD
    auto nint = int(std::ceil(std::log(fmax_/fmin_)/std::log(10)*ptsPerDecade_));
    auto step = std::log(fmax_/fmin_)/nint;
    std::vector<double> psd_target(nint+1);
    std::vector<double> freq(nint+1);
    for(int i=0; i<=nint; i++) {
        freq[i] = fmin_ * std::exp(i*step);
        psd_target[i] = 1.0/std::pow(freq[i], alpha);
    }

    // PSD weights (copy, compute squared coeffs)
    auto wpsd = data[index];
    for(auto& w : wpsd) {
        w = w*w;
    }
    auto n = wpsd.size();

    // Temporary rows storage
    std::vector<double> rows(n);

    // Compute initial psd and error
    std::vector<double> psd(nint+1);
    computePsds(wpsd, freq, rows, psd);
    auto initialError = err(psd_target, psd);

    // Gradient storage
    std::vector<double> gradient(n);

    // Best-yet weights and error
    auto bestw = wpsd;
    auto bestErr = initialError;
    
    // Candidate weights
    auto wcand = wpsd;

    // Candidate PSD
    std::vector<double> psdcand(nint+1);

    // Optimizer loop
    auto atLr = lr_;
    int failures = 0;
    for(int iter=0; iter<ni_; iter++) {
        // Compute gradient, psd, and error
        bestw = wpsd;
        bestErr = computeGradient(wpsd, freq, psd_target, rows, psd, gradient);

        // No search direction yet
        double direction = 0;

        // Line search
        bool improved = false;
        for(int j=0; j<ns_; j++) {
            if (direction) {
                atLr *= direction;
            }
            
            // Candidate weights
            for(decltype(n) k=0; k<n; k++) {
                wcand[k] = std::exp(std::log(wpsd[k]) - atLr * gradient[k]);
            }

            // Compute error at candidate
            computePsds(wcand, freq, rows, psdcand);
            auto canderr = err(psd_target, psdcand);

            if (canderr<bestErr) {
                if (!direction) {
                    direction = 2;
                }
                bestw = wcand;
                bestErr = canderr;
                // Found a better point, reset consecutive failure count
                failures = 0;
                improved = true;
                
            } else {
                if (!direction) {
                    direction = 0.5;
                }
                // If we were going forward and failed to find a better point, stop
                if (direction>1) {
                    break;
                }
            }
        }
        
        if (improved) {
            // Move origin to new better point
            wpsd = bestw;
        } else {
            // After 5 consecutive failures, give up
            failures++;
            if (failures>10) {
                break;
            }
        }
    }

    // Final error
    computePsds(wpsd, freq, rows, psd);
    auto finalError = err(psd_target, psd);

    // Check if error improved
    if (finalError>=initialError) {
        // Nothing to do
        if (debug_) {
            Simulator::dbg() << "  Coefficients tuning failed to improve PSD error.\n";
        }
        return false;
    }

    // Check if coeffs are finite
    for(auto& w : wpsd) {
        if (!std::isfinite(w)) {
            // Not finite, nothng to do
            if (debug_) {
                Simulator::dbg() << "  Coefficients tuning produced inf/nan.\n";
            }
            return false;
        }
    }

    if (debug_) {
        Simulator::dbg() << "  PSD RMS error [dB]: " << initialError << " -> " << finalError << "\n";
    }

    // Write coefficients
    for(decltype(n) i=0; i<n; i++) {
        data[index][i] = std::sqrt(wpsd[i]);
    }

    return true;
}

}
