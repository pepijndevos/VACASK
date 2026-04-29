#ifndef __TDNZOHFLICKER_DEFINED
#define __TDNZOHFLICKER_DEFINED

#include <unordered_map>
#include <vector>
#include "tdnblock.h"
#include "common.h"


namespace NAMESPACE {

// Voss-McCartney flicker noise coefficients
class VmFlickerCoeffs : public TimeDomainNoiseCoeffs {
public:
    VmFlickerCoeffs() {};
    virtual ~VmFlickerCoeffs() override = default;

    VmFlickerCoeffs           (const VmFlickerCoeffs&)  = delete;
    VmFlickerCoeffs           (      VmFlickerCoeffs&&) = default;
    VmFlickerCoeffs& operator=(const VmFlickerCoeffs&)  = delete;
    VmFlickerCoeffs& operator=(      VmFlickerCoeffs&&) = delete;

    void reset(int k, double fs, double fmin, double fmax, int ptsPerDecade=10, int ni=100, int ns=5, double lr=0.1);

protected:
    virtual void analyticalCoefficients(double alpha, std::vector<double>& coeffs) override;
    virtual bool computeTargetPsd(double alpha, std::vector<double>& target, const std::vector<double>& freq) override;
    virtual double computePsd(const std::vector<double>& wpsd, double f, std::vector<double>& contributions) override;

    double fs_;
};

// Zero-order hold flicker noise generator block
// All generators have the same number of Voss-McCartney rows
template <std::uniform_random_bit_generator URBG> 
class TimeDomainZohFlickerNoise : public TimeDomainZohNoiseBlock<URBG>, public VmFlickerCoeffs {
public:
    TimeDomainZohFlickerNoise() {};
    virtual ~TimeDomainZohFlickerNoise() override = default;

    TimeDomainZohFlickerNoise           (const TimeDomainZohFlickerNoise&)  = delete;
    TimeDomainZohFlickerNoise           (      TimeDomainZohFlickerNoise&&) = default;
    TimeDomainZohFlickerNoise& operator=(const TimeDomainZohFlickerNoise&)  = delete;
    TimeDomainZohFlickerNoise& operator=(      TimeDomainZohFlickerNoise&&) = delete;

    void reset(double t0, double timeStep, size_t count, int rollbackDepth, int k);
    void resetOptimizer(double fs, double fmin, double fmax, int ptsPerDecade=10, int ni=100, int ns=5, double lr=0.1) {
        VmFlickerCoeffs::reset(k_, fs, fmin, fmax, ptsPerDecade, ni, ns, lr);
    }

    void setDebug(int debug) { 
        TimeDomainNoiseBlock<URBG>::setDebug(debug);
        VmFlickerCoeffs::setDebug(debug);
    };
    
    // Set shape parameters for i-th generator (for now p is the exponent of flicker noise)
    virtual ShapeSetStatus setShapeParameters(size_t i, double p) override;

private:
    // Generate new sample
    void generate(URBG& gen) override;

    // Rows holding the state for each generator
    // Vector holds one row for all generators
    VectorRepository<double> rows;

    // Exponent value for each generator
    std::vector<double> exponent;

    // Coefficients index for each generator
    Vector<size_t> coeffIndex;

    // Number of rows, row 0 has update probability p=1/2, row k-1 has p=2^(-k)
    int k_;

    // Sampling frequency (i.e. update frequency for row with update probability p=1)
    double fs_;
    
    using TimeDomainNoiseBlock<URBG>::history;
    using TimeDomainNoiseBlock<URBG>::debug_;
};

} 

#endif
