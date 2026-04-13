#ifndef __TDNBLOCK_DEFINED
#define __TDNBLOCK_DEFINED

#include <unordered_map>
#include <vector>
#include <random>
#include <string>
#include "ansupport.h"
#include "common.h"


namespace NAMESPACE {

// Voss-McCartney coefficients repository
class VMCoefficientsRepository {
public:
    // Repository holds coefficients for randomized update VM 
    // with k rows and sampling frequency fs
    // The coefficients generate ZOH continuous time one-sided PSD of the form
    // 1/f^alpha * sinc(pi f / fs)^2
    VMCoefficientsRepository(int k, double fs) : k_(k), fs_(fs) {};

    VMCoefficientsRepository           (const VMCoefficientsRepository&)  = delete;
    VMCoefficientsRepository           (      VMCoefficientsRepository&&) = default;
    VMCoefficientsRepository& operator=(const VMCoefficientsRepository&)  = delete;
    VMCoefficientsRepository& operator=(      VMCoefficientsRepository&&) = delete;

    // Reset to new k and fs
    void reset(int k, double fs);

    // Return value: index, inserted
    std::tuple<size_t, bool> get(double alpha);

    // Optimize flicker coefficients for given frequency range
    bool optimizeCoefficients(size_t index, double fmin, double fmax, int ptsPerDecade);

private:
    int k_;
    double fs_;
    std::vector<std::vector<double>> data;
    std::unordered_map<double, size_t> flickerMap;
};


// Common API for a block of noise generators
class TimeDomainNoiseBlock {
public:
    TimeDomainNoiseBlock(double t0=0, double timeStep=1);

    TimeDomainNoiseBlock           (const TimeDomainNoiseBlock&)  = delete;
    TimeDomainNoiseBlock           (      TimeDomainNoiseBlock&&) = default;
    TimeDomainNoiseBlock& operator=(const TimeDomainNoiseBlock&)  = delete;
    TimeDomainNoiseBlock& operator=(      TimeDomainNoiseBlock&&) = delete;

    void reset(double t0, double timeStep);

    // Return value is true if sample index changes
    template <std::uniform_random_bit_generator URBG> bool advance(double time, URBG& gen) { return false; };

    // Return value is true if sample index changes
    bool revert(double time) { return false; };

    // Return the vector of noise source values
    std::vector<double>& values() { throw std::logic_error("Undefined values() method."); };

    // Compute sample index
    size_t sampleIndex(double time) {
        return size_t(std::floor((time-t0_)/timeStep_));
    };

    // Check if timeStep_ is below tolerance
    bool stepSanityCheck(double time) {
        return timeStep_ > timeRelativeTolerance*std::max(t0_, time);
    }

protected:
    // Index of sample we are curretly at
    size_t atSample_;
    // Time between two consecutive samples
    double timeStep_;
    // Initial time
    double t0_;
    // Which historic entry are we at.
    //   0 .. latest (n)
    //   1 .. previous (n-1)
    //   ---
    unsigned int atHistoric;
};


// White noise generators (count specifies the number of generators)
// All generators should use the same random generator (gen). 
// It is the user's responsibility to seed the generator.
class TimeDomainWhiteNoise : public TimeDomainNoiseBlock {
public:
    TimeDomainWhiteNoise(double t0=0, double timeStep=1);

    TimeDomainWhiteNoise           (const TimeDomainWhiteNoise&)  = delete;
    TimeDomainWhiteNoise           (      TimeDomainWhiteNoise&&) = default;
    TimeDomainWhiteNoise& operator=(const TimeDomainWhiteNoise&)  = delete;
    TimeDomainWhiteNoise& operator=(      TimeDomainWhiteNoise&&) = delete;

    template <std::uniform_random_bit_generator URBG> void reset(double t0, double timeStep, size_t count, int rollbackDepth, URBG& gen);
    template <std::uniform_random_bit_generator URBG> bool advance(double time, URBG& gen);
    template <std::uniform_random_bit_generator URBG> bool revert(double time, URBG& gen);

    std::vector<double>& values() { return history.at(atHistoric); };

private:
    // Generate new sample
    template <std::uniform_random_bit_generator URBG> void generate(URBG& gen);

    VectorRepository<double> history;
};


// Flicker noise generators (count specifies the number of generators)
// All generators should use the same random generator (gen). 
// It is the user's responsibility to seed the generator.
// All generators have the same number of Voss-McCartney rows
class TimeDomainFlickerNoise : public TimeDomainNoiseBlock {
public:
    TimeDomainFlickerNoise(double t0=0, double timeStep=1);

    TimeDomainFlickerNoise           (const TimeDomainFlickerNoise&)  = delete;
    TimeDomainFlickerNoise           (      TimeDomainFlickerNoise&&) = default;
    TimeDomainFlickerNoise& operator=(const TimeDomainFlickerNoise&)  = delete;
    TimeDomainFlickerNoise& operator=(      TimeDomainFlickerNoise&&) = delete;

    template <std::uniform_random_bit_generator URBG> void reset(double t0, double timeStep, size_t count, int rollbackDepth, URBG& gen);
    template <std::uniform_random_bit_generator URBG> bool advance(double time, URBG& gen);
    template <std::uniform_random_bit_generator URBG> bool revert(double time, URBG& gen);

    std::vector<double>& values() { return history.at(atHistoric); };

private:
    // Generate new sample
    template <std::uniform_random_bit_generator URBG> void generate(URBG& gen);

    VectorRepository<double> history;
};


}

#endif
