#include "tdnzohflicker.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

void VmFlickerCoeffs::reset(int k, double fs, double fmin, double fmax, int ptsPerDecade, int ni, int ns, double lr) {
    TimeDomainNoiseCoeffs::reset(k, fmin, fmax, ptsPerDecade, ni, ns, lr);
    fs_ = fs;
}

// Randomized Voss-McCartney coefficients used for weighting outputs of rows in time domain. 
// Rows output ZOH signal. 
// Square these coeffs to get the PSD coefficients. 
// i=0    .. fastest row
// i=k_-1 .. slowest row
//
// fact = sqrt( 2 fs^(alpha-1) pi 2^(1-alpha) / (ln(2) sin(pi alpha / 2) (2 pi)^alpha) )
// w_i = 1/fact 2^(i(alpha-1)/2) 
//
// Produces one-sided PSD in midband:
// S = 1/fact^2 2 fs^(alpha-1) pi 2^(1-alpha) / (ln(2) sin(pi alpha / 2) (2 pi)^alpha) f^(-alpha) sinc(pi f / fs)^2
//   = f^(-alpha) sinc(pi f / fs)^2
void VmFlickerCoeffs::analyticalCoefficients(double alpha, std::vector<double>& coeffs) {
    const double pi = std::numbers::pi;
    // Scaling that makes one-sided PSD 1/f^alpha
    double scaling = 2*std::pow(fs_, -1.0+alpha)*pi*std::pow(2.0, 1.0-alpha) / 
        (std::log(2)*std::sin(pi*alpha/2)*std::pow(2*pi, alpha));
    // Signal scaling is the square root of PSD scaling
    scaling = std::sqrt(scaling);
    // Scaled signal coefficients
    for(int i=0; i<k_; i++) {
        coeffs[i] = std::pow(2.0, i*(alpha-1.0)/2.0) / scaling;
    }
}

// Target one-sided PSD does not include the sinc() term: S = f^(-alpha)
bool VmFlickerCoeffs::computeTargetPsd(double alpha, std::vector<double>& target, const std::vector<double>& freq) {
    auto n = freq.size();
    for(int i=0; i<n; i++) {
        target[i] = 1.0/std::pow(freq[i], alpha);
    }
    return true;
}

// Compute PSD for given PSD coeffs at given frequency
// wpsd are PSD weights, not signal weights
// Unweighted contributions of rows are stored in contributions. 
// Return PSD value
double VmFlickerCoeffs::computePsd(const std::vector<double>& wpsd, double f, std::vector<double>& contributions) {
    const double pi = std::numbers::pi;
    auto n = wpsd.size();

    // sin(x)/x argument
    auto zohArg = pi*f/fs_;
    
    // sin(pi f / fs)
    auto s = std::sin(zohArg);

    // Zero-order hold TF
    double zoh = zohArg!=0 ? 1/fs_*s/zohArg : 1/fs_;

    // Compute rows
    auto p = 0.5;
    double sum = 0;
    for(decltype(n) i=0; i<n; i++) {
        // Factor 2 comes from one-sided PSD
        auto row = 2*fs_*p*(2-p)/(p*p+4*(1-p)*s*s);
        contributions[i] = row*zoh*zoh;
        sum += wpsd[i] * contributions[i];
        p /= 2;
    }
    
    return sum;
}

template <std::uniform_random_bit_generator URBG> 
void TimeDomainZohFlickerNoise<URBG>::reset(double t0, double timeStep, size_t count, int rollbackDepth, int k) {
    // k_ rows, for count generators
    k_ = k;
    rows.upsize(k, count);
    // Exponents for generators
    exponent.resize(count);
    // Coefficints index in coefficients repository
    coeffIndex.resize(count);
    // Coeff index set to SIM_SIZE_T_MAX initially so that we can detect initalization
    std::fill(coeffIndex.begin(), coeffIndex.end(), SIM_SIZE_T_MAX);
    // Generated values
    history.upsize(rollbackDepth+1, count);
    // Zero all exponents
    zero(exponent);

    TimeDomainZohNoiseBlock<URBG>::reset(t0, timeStep, count, rollbackDepth);
};

template <std::uniform_random_bit_generator URBG> void TimeDomainZohFlickerNoise<URBG>::generate(URBG& gen) {
    std::normal_distribution<double> norm(0.0, 1.0);
    // Uniformly distributed unsigned 64-bit integer
    std::uniform_int_distribution<uint64_t> unif(0, UINT64_MAX);
    
    // Previous generator value
    auto& prevValue = history.at(1);

    // New generator value
    auto& newValue = history.at();

    // Index of flicker noise generator
    for(size_t i = 0; i<prevValue.size(); i++) {
        // Generate uniformly distributed random integer
        auto rowSelector = unif(gen);
        
        // Determine which row to update
        // Probabilities are 1/2, 1/4, 1/8, ..., 1/2^k; no row 1/2^k
        // Trailing/leading zero count is fastest operation on AMD64
        //
        // Trailing zeros   Probability     Index of row with that probability
        // 0 (..1)          1/2             0
        // 1 (..10)         1/4             1
        // 2 (..100)        1/8             2
        // i-1              1/2^i           i-1
        // ...
        // 63 (100..0)      1/2^64          63
        // 64 (00..0)       1/2^64          63
        //
        // Maximum i is 64. There can be at most 64 rows, i.e. k_<=64. 

        // Get row index, make sure that it is <k_. If it is >=k_, no update is performed. 
        // The generated number is 64-bit so it can't have more than k_ trailing zeros
        // k_ trailing zeros correspond to probability 1/2^k_ - probability of updating no row
        auto rowIndex = std::countr_zero(rowSelector);
        
        // Do we need to update a row
        if (rowIndex < k_) {
            // Get coefficients for this generator
            auto& coeffs = getCoefficients(coeffIndex[i]);

            // Generate random number, scale with row coefficient
            auto newRowValue = norm(gen) * coeffs[rowIndex];

            // Get row
            auto& row = rows.at(rowIndex);

            // Update sample
            newValue[i] = prevValue[i] - row[i] + newRowValue;

            // Update row
            row[i] = newRowValue;
        }
    }
}

template <std::uniform_random_bit_generator URBG> 
ShapeSetStatus TimeDomainZohFlickerNoise<URBG>::setShapeParameters(size_t i, double p) {
    auto e = p;
    if (e<0.1 || e>1.9) {
        return ShapeSetStatus::OutOfRange;
    }
    bool init = coeffIndex[i]==SIM_SIZE_T_MAX;
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

// Explicit instantiation of template class
template class TimeDomainZohFlickerNoise<std::mt19937_64>;

}
