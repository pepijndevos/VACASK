#ifndef __FREQGRID_DEFINED
#define __FREQGRID_DEFINED

#include <vector>
#include <tuple>
#include "densematrix.h"
#include "ansupport.h"
#include "status.h"
#include "value.h"
#include "common.h"

namespace NAMESPACE {

// Generation of harmonics and intermodulation products
// See Chapter 1.1 in: 
//   Kundert, White, Sangiovanni-Vincentelli: 
//   Steady-state methods for simulating analog and microwave circuits, 
//   Springer, 1990. 

class Spurs {
public:
    Spurs() = default;
    
    explicit Spurs  (const Spurs&)  = default; // Allow explicit copy constructor
    Spurs           (      Spurs&&) = default;
    Spurs& operator=(const Spurs&)  = delete;
    Spurs& operator=(      Spurs&&) = default;

    // Fundamental freqeuncies vector
    const Vector<Real>& fundamentals() const { return fundamentals_; };

    // Truncated grid point weights + weights of their negatives if mixing map is built
    const auto spurWeights(size_t i) { return spurWeights_.row(i); };
    
    // Absolute values of frequencies corresponding to truncated grid points
    const std::vector<double>& spectrum() const { return spectrum_; };

    // Frequencies corresponding to truncated grid points
    const std::vector<double>& signedSpectrum() const { return signedSpectrum_; };

    // Negatives and positives of truncated grid point absolute frequencies
    // Row and column indices in mixingStencil refer to frequencies in this vector
    const std::vector<double>& smsigFreq() const { return smsigFreq_; };

    // Weights for the i-th small-signal frequency (index into smsigFreq_)
    VectorView<Int> smsigFreqWeights(size_t i) const { return spurWeights_.row(smsigFreqWeightIndices_[i]); };

    // Mapping from (output, input) spur index pair to frequency-domain Jacobian component index
    // i=0 = no entry foir this pair
    // i>0 = entries corresponding to Jacobians at frequencies given by spectrum, 
    //       i.e. Jac[i-1]
    // i<0 = conjugate Jacobian components, i.e. conj(Jac[-i])
    const DenseMatrix<Int>& mixingStencil() const { return mixingStencil_; };

    // Returns the index of the small-signal frequency corresponding to f
    std::tuple<bool, size_t> smsigFreqIndex(double f, double tol=1e-14) const;
    
    // Decodes a spur into the corresponding small-signal frequency index
    std::tuple<bool, size_t> smsigFreqIndex(const Value& v) const;
    
    // Build grid and spectrum (for HB)
    bool build(const std::vector<double>& fundamentals, const std::vector<int>& nHarmonics, int maxImOrder=0, bool hybrid=false, Int debug=0, Status& s=Status::ignore);
   
    // Build mixing map for (quasi)perodic small-signal analyses
    bool buildMixingMap(int debug=0, Status& s=Status::ignore);

    // Range of fow indoces for column i where mixing entries are found
    std::tuple<size_t, size_t> rowRange(size_t i) const { return std::make_tuple(rowStartNonzero[i], rowEndNonzero[i]); };

    // This value in mixingMap indicated no entry at that position
    static constexpr Int noJacIndex = -1;

private:
    // Spur properties
    std::tuple<double, int, int> spurStats(VectorView<Int>& weights) const;

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
    Vector<Real> fundamentals_;

    // Spurs that are mapped to the spectrum, and their negatives if mixing map is created
    // This vector holds the signed frequencies of spurs. Spurs are sorted by absolute frequency. 
    // If mixing map is created the negatives of sorted spurs are added at the end in the same order
    // as they appear in the first half. 
    std::vector<double> spurs_;

    // Spur weights corresponding to spurs_. 
    // Rows are spurs_, columns are weights
    DenseMatrix<int> spurWeights_;

    // Map from spur weights to small-signal frequency value
    std::unordered_map<VectorView<int>, size_t, ArrayHasher, ArrayEqual> smsigFreqMap;

    // Index stencil - column-major matrix of Jacobian component indices
    // s<0  .. no contribution
    // s>=0 .. contribution from Jacobian spectral component with index s
    // Jacobian spectral components are in the same order as frequencies in smsigFreq_
    DenseMatrix<Int> mixingStencil_;

    // First nonzero row for each stencil column
    // Helps cut down the number of dense matrix loads for Toeplitz matrices with bandwidth<2n-1
    Vector<size_t> rowStartNonzero;

    // Last nonzero row for each stencil column
    // Helps cut down the number of dense matrix loads for Toeplitz matrices with bandwidth<2n-1
    Vector<size_t> rowEndNonzero;
    
    // Absolute spectral frequencies, sorted - used as scale for HB
    // These are the absolute values of the spur frequencies
    // They appear in the same order as those in spur_
    // The negatives added when mixingStencil is created are not listed here
    Vector<Real> spectrum_;

    // Spectral frequencies, signed, sorted by absolute value - used by HB at APFT construction
    // They appear in the same order as those in spur_
    // The negatives added when mixingStencil is created are not listed here
    Vector<Real> signedSpectrum_;

    // Index of DC component is smsigFreq_
    size_t dcIndex;
    
    // Small-signal frequencies, sorted - used by (quasi)periodic small signal analyses
    // These are absolute frequencies from spectrun_ and their negatives, 0 is always included
    // First come the negatives, followed by spectrum_
    Vector<Real> smsigFreq_;

    // Weight indices of sorted small signal frequencies
    Vector<size_t> smsigFreqWeightIndices_;

    // Flag that indicates two components in signedSpectrum_ were in conflict 
    // (resulted in same absolute frequency). One of them was removed. 
    bool conflict;
};

}

#endif
