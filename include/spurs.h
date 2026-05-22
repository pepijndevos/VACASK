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

    // Fundamental frequencies vector
    const Vector<Real>& fundamentals() const { return fundamentals_; };

    // Frequencies corresponding to truncated grid points used by HB, no negatives included
    const std::vector<double>& spectrum() const { return spectrum_; };

    // Truncated grid point weights
    // Let n denote the number of frequencies in spectrum()
    // Index 0 is DC, indices 1..n-1 are positive spectrum components
    // indices n..2n-2 are the negatives on frequencies with indices 1..n-1
    const auto spurWeights(size_t i) { return spurWeights_.row(i); };
    
    // Negatives and positives of truncated grid point frequencies
    // Sorted in ascending order. 
    // If prune() was called this spectrum has fewer than 2n-1 components
    // Row and column indices in mixingStencil refer to frequencies in this vector
    const std::vector<double>& smsigFreq() const { return smsigFreq_; };

    // Weights for the i-th small-signal frequency (index into smsigFreq())
    VectorView<Int> smsigFreqWeights(size_t i) const { return spurWeights_.row(smsigFreqWeightIndices_[i]); };

    // Mapping from (output, input) spur index pair to frequency-domain Jacobian component index
    // Row/column indices match the optionally truncated smsigFreq() spectrum
    // i<0  .. no entry for this pair
    // i>=0 .. entries corresponding to Jacobians at frequencies given by spectrum, i.e. Jac[i]
    const DenseMatrix<Int>& mixingStencil() const { return mixingStencil_; };

    // Returns the index of the small-signal frequency in smsigFreq() corresponding to f
    // First return value is false if not found. 
    std::tuple<bool, size_t> smsigFreqIndex(double f, double tol=1e-14) const;
    
    // Decodes a spur given as frequency or weights vector into the corresponding small-signal frequency index
    // First return value is false if not found. 
    std::tuple<bool, size_t> smsigFreqIndex(const Value& v) const;
    
    // Build grid and spectrum (for HB). Also builds full smsigFreq() spectrum. 
    bool build(const std::vector<double>& fundamentals, const std::vector<int>& nHarmonics, int maxImOrder=0, bool hybrid=false, Int debug=0, Status& s=Status::ignore);

    // Prune spurs with absolute tone weights above maxHarm or absolute frequency above maxFreq
    // from smsigFreq() spectrum. 
    // Negative maxHarm component disables pruning by that tone weight. 
    // Negative maxFreq disables pruning by frequency. 
    bool prune(const Vector<Int>& maxHarm, double maxFreq=-1, Status& s=Status::ignore);
   
    // Build mixing map for (quasi)perodic small-signal analyses based on current smsigFreq() spectrum. 
    bool buildMixingMap(int debug=0, Status& s=Status::ignore);

    // Range of indices for column i of mixing map where mixing entries are found. 
    std::tuple<size_t, size_t> rowRange(size_t i) const { return std::make_tuple(rowStartNonzero[i], rowEndNonzero[i]); };

    // This value in mixingMap indicated no entry at that position
    static constexpr Int noJacIndex = -1;

private:
    // Spur properties
    std::tuple<double, int, int> spurStats(VectorView<Int>& weights) const;

    // Build unpruned small signal analysis spectrum and associated data
    void buildSmsig();

    // Build map from full small-signal spectrum to pruned small-signal spectrum
    void buildPrunedSmsigFreqIndex();

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

    // Spurs that are mapped to the spectrum, and its negatives
    // This vector holds the frequencies of spurs. Negative spurs are added at the end in the same order
    // as positive spurs. 
    std::vector<double> spurs_;

    // Spur weights corresponding to spurs_. Rows are spurs_, columns are weights
    DenseMatrix<int> spurWeights_;

    // Spectral frequencies, sorted - used as scale for HB
    // These are the frequencies of nonnegative spurs. 
    // They appear in the same order as those in spur_
    // The negatives are not listed here. 
    Vector<Real> spectrum_;

    // Index of DC component is smsigFreq_
    size_t dcIndex;
    
    // Small-signal frequencies, sorted - used by (quasi)periodic small signal analyses
    // These are frequencies from spectrun_ and their negatives, 0 is always included
    // First come the negatives, followed by spectrum_
    // This vector is sorted by increasing frequency. 
    Vector<Real> smsigFreq_;

    // Weight indices of sorted small signal frequencies
    Vector<size_t> smsigFreqWeightIndices_;

    // Full small-signal spectrum index (used for transcribing full spectrum to pruned spectrum)
    Vector<size_t> fullSmsigFreqIndex_;

    // Pruned small-signal spectrum index (for tranlating full small-signal spectrum index into pruned spectrum index)
    Vector<size_t> prunedSmsigFreqIndex_;

    // For prunedSmsigFreqIndex_, this value indicates that there is no corresponding 
    // pruned small signal frequency in smsigFreq_
    static constexpr size_t noPrunedSmsigFreqIndex = SIM_SIZE_T_MAX;

    // Flag that indicates two components in signedSpectrum_ were in conflict 
    // (resulted in same absolute frequency). One of them was removed. 
    bool conflict;

    // Map from spur weights to pruned small-signal frequency index
    std::unordered_map<VectorView<int>, size_t, ArrayHasher, ArrayEqual> smsigFreqMap;

    // Index stencil - column-major matrix of Jacobian component indices
    // s<0  .. no contribution
    // s>=0 .. contribution from Jacobian spectral component with pruned small-signal index s
    // Jacobian spectral components are in the same order as frequencies in smsigFreq_
    DenseMatrix<Int> mixingStencil_;

    // First nonzero row for each stencil column
    // Helps cut down the number of dense matrix loads for Toeplitz matrices with bandwidth<2n-1
    Vector<size_t> rowStartNonzero;

    // Last nonzero row for each stencil column
    // Helps cut down the number of dense matrix loads for Toeplitz matrices with bandwidth<2n-1
    Vector<size_t> rowEndNonzero;
};

}

#endif
