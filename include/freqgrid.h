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
// maps (Gridpoint::index excitation, Gridpoint::index response) tuple
// into Gridpoint::index Jacobian spectral component
// Needed by HBAC, PAC, QPAC
// For single-tone analyses this results in a Toeplitz matrix

// TODO: remove raw truncation scheme

class FrequencyGrid {
public:
    struct SpecFreq {
        // Row index in grid
        // Need this because we compact grid. 
        // We also sort SpecFreq array, bu leave the grid unchanged. 
        size_t gridIndex;
        // Abslute frequency
        double f;
        // If the grid entry results in a negative frequency, this is true
        bool negative;
        // Intermodulation product order
        // For harmonics this is the order of the harmonic. 
        int order;
        // Flag indicating that this frequency is a harmonic 
        // (i.e. all grid coordinates, but one, are 0)
        bool isHarmonic;
    };
    typedef std::vector<int> SpectralFingerprint;

    FrequencyGrid(const std::vector<double>& fundamentals);
    ~FrequencyGrid();

    FrequencyGrid           (const FrequencyGrid&)  = delete;
    FrequencyGrid           (      FrequencyGrid&&) = delete;
    FrequencyGrid& operator=(const FrequencyGrid&)  = delete;
    FrequencyGrid& operator=(      FrequencyGrid&&) = delete;

    bool build(const std::vector<int>& nHarmonics, int maxImOrder=0, bool hybrid=false, int debug=0, Status& s=Status::ignore);
    
    bool buildMixingMap();

private:
    // Tones
    std::vector<double> fundamentals;

    // Gridpoints that are mapped to the spectrum
    std::vector<SpecFreq> freq;

    // Gridpoint fingerprints
    // Rows are gridpoints, columns are tone factors
    DenseMatrix<int> grid;

    // Spectral frequencies (absolute), sorted
    std::vector<double> spectrum;
    
    // Spectrum -> grid mapping
    // Gridpoint indices for sorted spectral components
    // If two gridpoints map to the same spectral component
    // the one with the lowest hash is listed here
    std::vector<size_t> spectralIndices;  
};

}

#endif
