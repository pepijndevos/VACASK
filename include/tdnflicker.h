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
    VMCoefficientsRepository() = default;

    VMCoefficientsRepository           (const VMCoefficientsRepository&)  = delete;
    VMCoefficientsRepository           (      VMCoefficientsRepository&&) = default;
    VMCoefficientsRepository& operator=(const VMCoefficientsRepository&)  = delete;
    VMCoefficientsRepository& operator=(      VMCoefficientsRepository&&) = delete;

    // Reset to new k and fs
    void reset(int k, double fs);

    // Looks up coefficients for exponent alpha, if not present, generates them
    // Return value: index, inserted
    std::tuple<size_t, bool> get(double alpha);

    // Get coefficients with index i
    std::vector<double>& get(size_t i) {
        return data[i];
    };

    // Optimize flicker coefficients for given frequency range
    bool optimizeCoefficients(size_t index, double fmin, double fmax, int ptsPerDecade);

private:
    int k_;
    double fs_;
    std::vector<std::vector<double>> data;
    std::unordered_map<double, size_t> flickerMap;
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
    TimeDomainFlickerNoise(VMCoefficientsRepository& repo)
        : coeffsRepo(repo) {
    };

    TimeDomainFlickerNoise           (const TimeDomainFlickerNoise&)  = delete;
    TimeDomainFlickerNoise           (      TimeDomainFlickerNoise&&) = default;
    TimeDomainFlickerNoise& operator=(const TimeDomainFlickerNoise&)  = delete;
    TimeDomainFlickerNoise& operator=(      TimeDomainFlickerNoise&&) = delete;

    using TimeDomainNoiseBlock<URBG>::reset;
    void reset(double t0, double timeStep, size_t count, int rollbackDepth, int k, URBG& gen);
    
    // This one should be inlined
    ExponentStatus setExponent(size_t i, double e) {
        if (e<0.1 || e>1.9) {
            return ExponentStatus::OutOfRange;
        }
        bool init = coeffIndex[i]==SIZE_T_MAX;
        bool changed = false;
        bool compute = init || changed;
        if (compute) {
            auto [newIndex, ok] = coeffsRepo.get(e);
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

    // Values vector
    VectorRepository<double> history;

    // Number of rows
    int k_;

    // COefficients repository
    VMCoefficientsRepository& coeffsRepo;
};

template <std::uniform_random_bit_generator URBG> 
void TimeDomainFlickerNoise<URBG>::reset(double t0, double timeStep, size_t count, int rollbackDepth, int k, URBG& gen) {
    TimeDomainNoiseBlock<URBG>::reset(t0, timeStep, count, rollbackDepth);
    k_ = k;
    // k_ rows, for count generators
    rows.upsize(k, count);
    // Reversion data for rollbackDepth steps, one per generator
    reversionData.upsize(rollbackDepth, count);
    // Exponents for generators
    exponent.resize(count);
    // Coefficints index in coefficients repository
    coeffIndex.resize(count);
    // Generated values
    history.upsize(rollbackDepth+1, count);
    // Zero all exponents
    zero(exponent);
    // Coeff index set to SIZE_T_MAX initially
    std::fill(exponent.begin(), exponent.end(), SIZE_T_MAX);
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

    // Index of flicker noise generator
    for(size_t i = 0; i<prevValue.size(); i++) {
        // Get coefficients for this generator
        auto& coeffs = coeffsRepo.get(coeffIndex[i]);

        // Generate random number
        auto rowSelector = unif(gen);
        
        // Determine which row to update
        // Probabilities are 1/2, 1/4, 1/8, ..., 1/2^k; no row 1/2^k
        // Trailing/leading zero count is fastest operation on AMD64
        //
        // Trailing zeros   Probability     Row index with that probability
        // 0                1/2             0
        // 1                1/4             1
        // 2                1/8             2
        // k-1              1/2^k           k-1
        //
        // Maximum k is 64. 

        // Get row index, make sure that it is <=k_
        // The generated number is 64-bit so it can have more than k_ trailing zeros
        // k_ trailing zeros correspond to probability 1/2^(k+1) - probability of updating no row
        auto rowIndex = std::min(std::countr_zero(rowSelector), k_);

        // We need to update a row
        if (rowIndex < k_) {
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
