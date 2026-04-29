#include "simulator.h"
#include "tdnblock.h"
#include "common.h"

namespace NAMESPACE {

template <std::uniform_random_bit_generator URBG> bool TimeDomainZohNoiseBlock<URBG>::advance(double time, double h, URBG& gen) {
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

template <std::uniform_random_bit_generator URBG> bool TimeDomainZohNoiseBlock<URBG>::revert(double time, double h, URBG& gen) {
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
            atHistoric += 1;
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


template <std::uniform_random_bit_generator URBG> bool TimeDomainSdeNoiseBlock<URBG>::advance(double time, double h, URBG& gen) {
    if (time>lastAcceptedTime) {
        // Advance history
        history.advance();

        // Regenerate random numbers
        generate(gen);

        // Rebuild sample
        construct(h);

        // Update last accepted time
        lastAcceptedTime = atTime;

        // Update at time
        atTime = time;
        
        return true;
    } else if (time<lastAcceptedTime) {
        // Panic - advancing before last accepted time
        throw std::logic_error("Attempt to advance noise generator before last accepted time.");
    } else {
        // Panic - advancing to last accepted time
        throw std::logic_error("Attempt to advance noise generator to last accepted time.");
    }
    return false;
}

template <std::uniform_random_bit_generator URBG> bool TimeDomainSdeNoiseBlock<URBG>::revert(double time, double h, URBG& gen) {
    if (time>lastAcceptedTime) {
        // Keep history, rebuild latest sample
        construct(h);

        // Update at time
        atTime = time;

        return true;
    } else if (time<lastAcceptedTime) {
        // Panic - reverting before last accepted time
        throw std::logic_error("Attempt to revert noise generator before last accepted time.");
    } else {
        // Panic - reverting to last accepted time
        throw std::logic_error("Attempt to advance noise generator to last accepted time.");
    }
    return false;
}


void TimeDomainNoiseCoeffs::reset(int k, double fmin, double fmax, int ptsPerDecade, int ni, int ns, double lr) {
    data.clear();
    coeffMap.clear();
    k_ = k;
    fmin_ = fmin;
    fmax_ = fmax;
    ptsPerDecade_ = ptsPerDecade;
    ni_ = ni;
    ns_ = ns;
    lr_ = lr;
}

std::tuple<size_t, bool> TimeDomainNoiseCoeffs::getCoefficients(double alpha) {
    auto it = coeffMap.find(alpha);
    if (it!=coeffMap.end()) {
        // Already in map
        return std::make_tuple(it->second, false);
    } 

    if (debug_) {
        Simulator::dbg() << "Computing coefficients for alpha=" << alpha << "\n";
    }

    // Not in map, insert
    auto index = data.size();
    coeffMap.insert({alpha, index});
    
    // Build coefficients
    auto& coeffs = data.emplace_back(k_);
    analyticalCoefficients(alpha, coeffs);

    // Optimize coeffs
    auto ok = optimizeCoefficients(alpha, coeffs);

    return std::make_tuple(index, true);
}

double TimeDomainNoiseCoeffs::err(const std::vector<double>& target, const std::vector<double>& psd) {
    auto n = target.size();
    double sum = 0;
    for(decltype(n) i=0; i<n; i++) {
        auto q = 10*std::log(psd[i]/target[i])/std::log(10);
        sum += q*q;
    }
    return std::sqrt(sum/n);
}

void TimeDomainNoiseCoeffs::computePsds(const std::vector<double>& wpsd, const std::vector<double>& freq, std::vector<double>& tmpContribs, std::vector<double>& result) {
    // result will hold the PSD of generator output
    const double pi = std::numbers::pi;
    auto nf = freq.size();
    auto n = wpsd.size();
    double sum = 0;
    zero(result);
    for(decltype(nf) j=0; j<nf; j++) {
        auto f = freq[j];
        result[j] = computePsd(wpsd, f, tmpContribs);
    }
}

// wpsd        .. wieghts (PSD weights, squared signal weights)
// freq        .. frequencies
// target      .. target PSD at frequencies
// tmpContribs .. temporary storage for k: contributions
// psd         .. computed PSD at frequencies
// gradient    .. gradient of sum_i (ln(psd_i/target_i))^2 wrt. logarithms of PSD weights
// 
// Returns err(target psd)
double TimeDomainNoiseCoeffs::computeGradient(const std::vector<double>& wpsd, const std::vector<double>& freq, const std::vector<double>& target, std::vector<double>& tmpContribs, std::vector<double>& psd, std::vector<double>& gradient) {
    // We are minimizing 
    //   F = sum_i (ln(psd_i/target_i))^2
    // where psd_i is the psd at ith frequency. 
    //   psd_i = sum_j w_j contribution_j(f_i)
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
    // dpsd_i/dw = contribs_i .. values of unweighted contributions at f_i
    //
    // dF/dwl = sum_i ( 2 ln(psd_i/target_i) 1/psd_i contribs_i w )
    const double pi = std::numbers::pi;

    auto nc = wpsd.size(); 
    auto nf = freq.size();

    // PSD at all frequencies
    computePsds(wpsd, freq, tmpContribs, psd);

    // Start with zero gradient
    zero(gradient);

    // Go through all freq points
    for(decltype(nf) i=0; i<nf; i++) {
        auto f = freq[i];

        // Compute PSD at f and the values of contribs
        auto psd_i = computePsd(wpsd, f, tmpContribs);
        auto ldiff = std::log(psd_i/target[i]);
        auto fact = 2*ldiff/psd_i;
        psd[i] = psd_i;
        
        // Go through all weights
        for(decltype(nc) iw=0; iw<nc; iw++) {
            auto dF_dw = fact * (tmpContribs[iw]) * wpsd[iw];
            gradient[iw] += dF_dw;
        }
    }
    
    return err(target, psd);
}

bool TimeDomainNoiseCoeffs::optimizeCoefficients(double alpha, std::vector<double>& coeffs) {
    // Build vector of frequency points and ideal PSD
    auto nint = int(std::ceil(std::log(fmax_/fmin_)/std::log(10)*ptsPerDecade_));
    auto step = std::log(fmax_/fmin_)/nint;
    std::vector<double> psd_target(nint+1);
    std::vector<double> freq(nint+1);
    for(int i=0; i<=nint; i++) {
        freq[i] = fmin_ * std::exp(i*step);
    }
    computeTargetPsd(alpha, psd_target, freq);

    // PSD weights (copy, compute squared coeffs because optimizer works with PSD coefficients)
    auto wpsd = coeffs;
    for(auto& w : wpsd) {
        w = w*w;
    }
    auto n = wpsd.size();

    // Temporary contributions storage
    std::vector<double> contribs(n);

    // Compute initial psd and error
    std::vector<double> psd(nint+1);
    computePsds(wpsd, freq, contribs, psd);
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
        bestErr = computeGradient(wpsd, freq, psd_target, contribs, psd, gradient);

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
            computePsds(wcand, freq, contribs, psdcand);
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
    computePsds(wpsd, freq, contribs, psd);
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
        coeffs[i] = std::sqrt(wpsd[i]);
    }

    return true;
}


// Explicit instantiation of template class
template class TimeDomainNoiseBlock<std::mt19937_64>;
template class TimeDomainZohNoiseBlock<std::mt19937_64>;
template class TimeDomainSdeNoiseBlock<std::mt19937_64>;

}


