#ifndef __FREQGRID_DEFINED
#define __FREQGRID_DEFINED

#include <vector>
#include "densematrix.h"
#include "status.h"
#include "common.h"

namespace NAMESPACE {

// Generation of harmonics and intermodulation products
// See Chapter 1.1 in: 
//   Kundert, White, Sangiovanni-Vincentelli: 
//   Steady-state methods for simulating analog and microwave circuits, 
//   Springer, 1990. 

class Spurs {
public:
    struct Spur {
        // Row index in spur weights matrix
        size_t index;
        // Signed frequency
        double f;
        // Intermodulation product order of this spur (L1 norm)
        // For harmonics this is the order of the harmonic. 
        int order;
        // Flag indicating that this spur is a harmonic 
        // (i.e. at most one weight is nonzero)
        bool isHarmonic;
    };
    
    Spurs() = default;
    
    Spurs           (const Spurs&)  = delete;
    Spurs           (      Spurs&&) = delete;
    Spurs& operator=(const Spurs&)  = delete;
    Spurs& operator=(      Spurs&&) = delete;

    const std::vector<Spur>& spurs() const { return spurs_; };
    const auto spurWeights(size_t i) { return spurWeights_.row(spurs_[i].index); };
    const std::vector<double>& spectrum() const { return spectrum_; };
    const std::vector<double>& signedSpectrum() const { return signedSpectrum_; };
    
    bool build(const std::vector<double>& fundamentals, const std::vector<int>& nHarmonics, int maxImOrder=0, bool hybrid=false, int debug=0, Status& s=Status::ignore);
   
    bool buildMixingMap(int debug=0, Status& s=Status::ignore);

    static const int32_t noJacIndex = 0;

private:
    double toFreq(VectorView<int> weights);

    // Custom hasher based on a pointer to integer array
    struct ArrayHasher {
        size_t operator()(const VectorView<int>& k) const {
            size_t seed = 0;
            for (size_t i = 0; i < k.n(); i++)
                seed ^= std::hash<int>{}(k[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    // Custom equality comparator
    struct ArrayEqual {
        bool operator()(const VectorView<int>& a, const VectorView<int>& b) const {
            for(size_t i=0; i<a.n(); i++) {
                if (a[i]!=b[i]) {
                    return false;
                }
            }
            return true;
        }
    };

    // Fundamentals
    std::vector<double> fundamentals_;

    // Spurs that are mapped to the spectrum, just the ones that were obtained with truncation
    std::vector<Spur> spurs_;

    // Spur weights obtained with truncation and their negatives
    // Rows are spurs_, columns are weights
    DenseMatrix<int> spurWeights_;

    // Map from spur weights to spur index
    std::unordered_map<VectorView<int>, size_t, ArrayHasher, ArrayEqual> spurMap;

    // Index stencil - column-major matrix of Jacobian component indices
    // s=0 .. no contribution
    // s>0 .. cotnribution from component s-1
    // s<0 .. contribution from component abs(s), conjugated
    DenseMatrix<int32_t> indexStencil;

    // Spectral frequencies (absolute), sorted
    std::vector<double> spectrum_;

    // Spectral frequencies, signed, ordered as in spectrum_
    std::vector<double> signedSpectrum_;

    // Small-signal frequencies in order of appearance
    std::vector<double> smsigFreq_;
    
    // Flag that indicated two spurs_ conflict (result in same spectrum frequency)
    bool conflict;
};

}

#endif
