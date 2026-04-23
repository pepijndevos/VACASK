#ifndef __TDNFLICKER_DEFINED
#define __TDNFLICKER_DEFINED

#include <unordered_map>
#include <vector>
#include "tdnblock.h"
#include "common.h"


namespace NAMESPACE {

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
template <std::uniform_random_bit_generator URBG> class TimeDomainZohFlickerNoise : public TimeDomainNoiseBlock<URBG> {
public:
    TimeDomainZohFlickerNoise() {};

    TimeDomainZohFlickerNoise           (const TimeDomainZohFlickerNoise&)  = delete;
    TimeDomainZohFlickerNoise           (      TimeDomainZohFlickerNoise&&) = default;
    TimeDomainZohFlickerNoise& operator=(const TimeDomainZohFlickerNoise&)  = delete;
    TimeDomainZohFlickerNoise& operator=(      TimeDomainZohFlickerNoise&&) = delete;

    void reset(double t0, double timeStep, size_t count, int rollbackDepth, int k);
    void reset(double t0, double timeStep, size_t count, int rollbackDepth, int k, URBG& gen);
    void resetOptimizer(double fs, double fmin, double fmax, int ptsPerDecade=10, int ni=100, int ns=5, double lr=0.1);
    void setDebug(int debug) { debug_ = debug; };

    // This one should be inlined
    ExponentStatus setExponent(size_t i, double e) {
        if (e<0.1 || e>1.9) {
            return ExponentStatus::OutOfRange;
        }
        bool init = coeffIndex[i]==SIZE_T_MAX;
        bool changed = false;
        bool compute = init || changed;
        if (compute) {
            auto [newIndex, ok] = getCoefficients(e);
            coeffIndex[i] = newIndex;
            exponent[i] = e;
            return init ? ExponentStatus::Initialized : ExponentStatus::Changed;
        }
        return ExponentStatus::Unchanged;
    };
    // template <std::uniform_random_bit_generator URBG> bool advance(double time, URBG& gen);
    // template <std::uniform_random_bit_generator URBG> bool revert(double time, URBG& gen);

private:
    // Optimize flicker coefficients for given frequency range
    bool optimizeCoefficients(size_t index, double alpha);

    // Looks up coefficients for exponent alpha, if not present, generates them
    // Return value: index, inserted
    std::tuple<size_t, bool> getCoefficients(double alpha);

    // Get coefficients with index i
    std::vector<double>& getCoefficients(size_t i) {
        return data[i];
    };

    // Optimizer helpers
    double err(const std::vector<double>& target, const std::vector<double>& psd);
    double computePsd(const std::vector<double>& wpsd, double f, double& zoh, std::vector<double>& rows);
    void computePsds(const std::vector<double>& wpsd, const std::vector<double>& freq, std::vector<double>& tmpRows, std::vector<double>& result);
    double computeGradient(const std::vector<double>& wpsd, const std::vector<double>& freq, const std::vector<double>& target, std::vector<double>& tmpRows, std::vector<double>& psd, std::vector<double>& gradient);
    
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
    // VMCoefficientsRepository& vmCoeffs;

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

    using TimeDomainNoiseBlock<URBG>::history;
};

} 

#endif
