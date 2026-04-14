#ifndef __TDNFLICKER_DEFINED
#define __TDNFLICKER_DEFINED

#include <unordered_map>
#include <vector>
#include "tdnblock.h"
#include "common.h"


namespace NAMESPACE {

// Voss-McCartney coefficients repository
class VMCoefficientsRepository {
public:
    // Repository holds coefficients for randomized update VM 
    // with k rows and sampling frequency fs
    // The coefficients generate ZOH continuous time one-sided PSD of the form
    // 1/f^alpha * sinc(pi f / fs)^2
    VMCoefficientsRepository() : debug_(0) {};

    VMCoefficientsRepository           (const VMCoefficientsRepository&)  = delete;
    VMCoefficientsRepository           (      VMCoefficientsRepository&&) = default;
    VMCoefficientsRepository& operator=(const VMCoefficientsRepository&)  = delete;
    VMCoefficientsRepository& operator=(      VMCoefficientsRepository&&) = delete;

    // Reset to new k and fs
    // fs .. noise sampling frequency - first row changes at rate fs/2
    void reset(int k, double fs, int oversample, double fmin, double fmax, int ptsPerDecade=10, int ni=100, int ns=5, double lr=0.1);
    void setDebug(int debug) { debug_ = debug; };

    // Get k
    int k() const { return k_; };
    
    // Looks up coefficients for exponent alpha, if not present, generates them
    // Return value: index, inserted
    std::tuple<size_t, bool> get(double alpha);

    // Get coefficients with index i
    std::vector<double>& get(size_t i) {
        return data[i];
    };

    // Optimize flicker coefficients for given frequency range
    bool optimizeCoefficients(size_t index, double alpha);

private:
    double err(const std::vector<double>& target, const std::vector<double>& psd);
    double computePsd(const std::vector<double>& wpsd, double f, double& zoh, std::vector<double>& rows);
    void computePsds(const std::vector<double>& wpsd, const std::vector<double>& freq, std::vector<double>& tmpRows, std::vector<double>& result);
    double computeGradient(const std::vector<double>& wpsd, const std::vector<double>& freq, const std::vector<double>& target, std::vector<double>& tmpRows, std::vector<double>& psd, std::vector<double>& gradient);
    
    // Debug mode
    int debug_;
    // Number of rows, row 0 has update probability p=1/2, row k-1 has p=2^(-k)
    int k_;
    // Sampling frequency (i.e. update frequency for row with update probability p=1)
    double fs_;
    // Oversampling factor (used in computing ZOH effect) - TODO: remove
    int oversample_;
    // Coefficient data
    std::vector<std::vector<double>> data;
    // Map from flicker noise exponent into coefficients data
    std::unordered_map<double, size_t> flickerMap;

    // Optimizer settings
    // Frequency range to optimize
    double fmin_;
    double fmax_;
    // Point density used for computing error
    int ptsPerDecade_;
    // Gradient algorithm iterations
    int ni_;
    // Line search steps
    int ns_;
    // Step size
    double lr_;
};

enum class ExponentStatus {
    Initialized, 
    Unchanged, 
    Changed, 
    OutOfRange
};

// Flicker noise generators (count specifies the number of generators)
// All generators should use the same random generator (gen). 
// It is the user's responsibility to seed the generator.
// All generators have the same number of Voss-McCartney rows
template <std::uniform_random_bit_generator URBG> class TimeDomainFlickerNoise : public TimeDomainNoiseBlock<URBG> {
public:
    TimeDomainFlickerNoise(VMCoefficientsRepository& vmCoeffs)
        : vmCoeffs(vmCoeffs) {};

    TimeDomainFlickerNoise           (const TimeDomainFlickerNoise&)  = delete;
    TimeDomainFlickerNoise           (      TimeDomainFlickerNoise&&) = default;
    TimeDomainFlickerNoise& operator=(const TimeDomainFlickerNoise&)  = delete;
    TimeDomainFlickerNoise& operator=(      TimeDomainFlickerNoise&&) = delete;

    void reset(double t0, double timeStep, size_t count, int rollbackDepth);
    void reset(double t0, double timeStep, size_t count, int rollbackDepth, URBG& gen);

    // This one should be inlined
    ExponentStatus setExponent(size_t i, double e) {
        if (e<0.1 || e>1.9) {
            return ExponentStatus::OutOfRange;
        }
        bool init = coeffIndex[i]==SIZE_T_MAX;
        bool changed = false;
        bool compute = init || changed;
        if (compute) {
            auto [newIndex, ok] = vmCoeffs.get(e);
            coeffIndex[i] = newIndex;
            exponent[i] = e;
            return init ? ExponentStatus::Initialized : ExponentStatus::Changed;
        }
        return ExponentStatus::Unchanged;
    };
    // template <std::uniform_random_bit_generator URBG> bool advance(double time, URBG& gen);
    // template <std::uniform_random_bit_generator URBG> bool revert(double time, URBG& gen);

private:
    // Information needed to revert generators to previous state
    typedef struct ReversionData {
        int row;
        double previousValue;
    } ReversionData;

    // Generate new sample
    void generate(URBG& gen) override;

    // Rows holding the generators state
    VectorRepository<double> rows;

    // Data needed to revert generators
    VectorRepository<ReversionData> reversionData;

    // Exponent value for all generators
    std::vector<double> exponent;

    // Coefficients index for each generator
    Vector<size_t> coeffIndex;

    // Coefficients repository
    VMCoefficientsRepository& vmCoeffs;

    // Needed for optimization
    double fmin_;
    double fmax_;
    int ptsPerDecade_;

    using TimeDomainNoiseBlock<URBG>::history;
};

template <std::uniform_random_bit_generator URBG> 
void TimeDomainFlickerNoise<URBG>::reset(double t0, double timeStep, size_t count, int rollbackDepth) {
    TimeDomainNoiseBlock<URBG>::reset(t0, timeStep, count, rollbackDepth);
    // k_ rows, for count generators
    rows.upsize(vmCoeffs.k(), count);
    // Reversion data for rollbackDepth steps, one per generator
    reversionData.upsize(rollbackDepth, count);
    // Exponents for generators
    exponent.resize(count);
    // Coefficints index in coefficients repository
    coeffIndex.resize(count);
    // Coeff index set to SIZE_T_MAX initially so that we can detect initalization
    std::fill(coeffIndex.begin(), coeffIndex.end(), SIZE_T_MAX);
    // Generated values
    history.upsize(rollbackDepth+1, count);
    // Zero all exponents
    zero(exponent);
};

template <std::uniform_random_bit_generator URBG> 
void TimeDomainFlickerNoise<URBG>::reset(double t0, double timeStep, size_t count, int rollbackDepth, URBG& gen) {
    TimeDomainFlickerNoise<URBG>::reset(t0, timeStep, count, rollbackDepth);
    // Generate random sample
    generate(gen);
};

template <std::uniform_random_bit_generator URBG> void TimeDomainFlickerNoise<URBG>::generate(URBG& gen) {
    std::normal_distribution<double> norm(0.0, 1.0);
    // Uniformly distributed unsigned 64-bit integer
    std::uniform_int_distribution<uint64_t> unif(0, UINT64_MAX);
    
    // Previous generator value
    auto& prevValue = history.at(1);

    // New generator value
    auto& newValue = history.at();

    auto k = vmCoeffs.k();

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
        if (rowIndex < k) {
            // Get coefficients for this generator
            auto& coeffs = vmCoeffs.get(coeffIndex[i]);

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

} 

#endif
