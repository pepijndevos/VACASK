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

// GridHash - linear index built from gridpoint fingerprint
// 
// GridMap map GridHash -> gridpoint index
// lookup via GridHash and std::vector<int>&
// 
// MixingMap
// maps (frequency index of excitation, frequency index of response) tuple
// into frequency index of Jacobian spectral component for which 
//   response = J excitation
// Needed by HBAC, PAC, QPAC
// For single-tone analyses this map contains nonzeros in a Toeplitz matrix

class FrequencyGrid {
public:
    struct SpecFreq {
        // Row index in grid
        // Need this because we compact grid. 
        // We also sort SpecFreq array, bu leave the grid unchanged. 
        size_t gridIndex;
        // Signed frequency
        double f;
        // Intermodulation product order
        // For harmonics this is the order of the harmonic. 
        int order;
        // Flag indicating that this frequency is a harmonic 
        // (i.e. all grid coordinates, but one, are 0)
        bool isHarmonic;
    };
    
    FrequencyGrid() = default;
    
    FrequencyGrid           (const FrequencyGrid&)  = delete;
    FrequencyGrid           (      FrequencyGrid&&) = delete;
    FrequencyGrid& operator=(const FrequencyGrid&)  = delete;
    FrequencyGrid& operator=(      FrequencyGrid&&) = delete;

    const std::vector<SpecFreq>& frequencyData() const { return freq; };
    const std::vector<double>& spectrum() const { return spectrum_; };
    const std::vector<double>& signedSpectrum() const { return signedSpectrum_; };
    const auto weights(size_t i) { return grid.row(freq[i].gridIndex); };

    bool build(const std::vector<double>& fundamentals, const std::vector<int>& nHarmonics, int maxImOrder=0, bool hybrid=false, int debug=0, Status& s=Status::ignore);
   
    bool buildMixingMap(int debug=0, Status& s=Status::ignore);

    static const int32_t noJacIndex = INT32_MAX;

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

    // Tuple hasher
    struct TupleHasher {
        size_t operator()(const std::tuple<size_t, size_t>& t) const {
            size_t seed = std::hash<size_t>{}(std::get<0>(t));
            seed ^= std::hash<size_t>{}(std::get<1>(t)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    // Map from fingerprint to grid index
    std::unordered_map<VectorView<int>, size_t, ArrayHasher, ArrayEqual> freqMap;

    // Index stencil - column-major matrix of Jacobian component indices
    // Negative index corresponds to conjugate of component with that absolute index 
    // INT_32_MAX means the contribution at that position is 0
    DenseMatrix<int32_t> indexStencil;

    // Fundamentals
    std::vector<double> fundamentals_;

    // Gridpoints that are mapped to the spectrum, just the ones that were obtained with truncation
    std::vector<SpecFreq> freq;

    // Gridpoint fingerprints, all of them, also the negatives
    // Rows are gridpoints, columns are tone factors
    DenseMatrix<int> grid;

    // Spectral frequencies (absolute), sorted
    std::vector<double> spectrum_;

    // Spectral frequencies, signed, ordered as in spectrum_
    std::vector<double> signedSpectrum_;

    // Small-signal frequencies in order of appearance
    std::vector<double> smsigFreq_;
    
    // Spectrum -> grid mapping
    // Gridpoint indices for sorted spectral components
    // If two gridpoints map to the same spectral component
    // the one with the lowest hash is listed here
    std::vector<size_t> spectralIndices;  

    // Flag that indicated two gridpoints conflict (result in same spectrum frequency)
    bool conflict;
};

}

#endif
