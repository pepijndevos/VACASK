#include <vector>
#include <algorithm>
#include <random>
#include <numbers>
#include <cmath>
#include "corehb.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

// Amplitude vector (2m-1 components), first component is DC, 
// the remaining components are real and imaginary parts of 
// the phasors corresponding to m-1 nonzero frequencies. 
// 
//   [A0 A1r A1i A2r A2i ... A(m-1)r A(m-1)i]

// Transformation of an amplitude vector to a vector of signal values 
// at n colocation points.
// n rows, 2m-1 columns
// Should satisfy n=2m-1, but during colocation points selection n>=2m-1.

// See chapter 2 in:
//   Kundert, White, Sangiovanni-Vincentelli: 
//   Steady-state methods for simulating analog and microwave circuits, 
//   Springer, 1990. 

// Notable difference: the book works with magnitudes of cosine and sine 
// contributions. We, however, work with real and imaginary part of a phasor. 
// Because 
//   Re(X exp(jwt)) = Xr cos(jwt) - Xi sin(jwt) 
// the sin(jwt)) contribution is replaced with -sin(jwt). 

// We use the same subtraction of multiples of 2 pi from the phase of base 
// frequencies as in the book. If trigonometric functions have a decent 
// implementation this should not be neccessary. 

bool HBCore::buildTransformMatrix(DenseMatrix<double>& XF, Status& s) {
    auto n = timepoints.size();
    auto m = spurs_.spectrum().size();
    auto ncoef = 2*m-1;
    
    XF.resize(n, ncoef, DenseMatrix<double>::Major::Column);
    
    // Storage of (2 pi f t) factors for base frequencies
    auto nBase = spurs_.fundamentals().size();
    std::vector<double> baseFac(nBase);

    // Precompute reduced phase at tstart for each base frequency.
    // Use two-product via fma to get frac(f*tstart) with full precision,
    // avoiding catastrophic cancellation when f*tstart is large.
    std::vector<double> basePhaseAtTstart(nBase);
    for(decltype(nBase) k=0; k<nBase; k++) {
        auto f = spurs_.fundamentals()[k];
        auto prod = f * params.tstart;
        auto lo = std::fma(f, params.tstart, -prod);
        auto intpart = std::trunc(prod);
        auto frac = (prod - intpart) + lo;
        if (frac<0) frac += 1.0;
        if (frac>=1.0) frac -= 1.0;
        basePhaseAtTstart[k] = 2 * std::numbers::pi * frac;
    }

    // Loop through timepoints
    for(decltype(n) i=0; i<n; i++) {
        auto row = XF.row(i);
        auto t = timepoints[i];

        // For base frequencies compute phase at t (2 pi f t)
        // Split as: 2*pi*frac(f*tstart) + 2*pi*f*(t-tstart)
        // The first term is precomputed with extended precision.
        // The second term is small (t-tstart spans a few periods) so it is precise.
        for(decltype(nBase) k=0; k<nBase; k++) {
            baseFac[k] = basePhaseAtTstart[k]
                       + 2 * std::numbers::pi * spurs_.fundamentals()[k] * (t - params.tstart);
        }

        // DC
        row.at(0) = 1.0;

        // Nonzero frequencies
        for(decltype(m) j=1; j<m; j++) {
            // Grid entry
            auto weights = spurs_.spurWeights(j); 
            // Assemble phase from base frequency contributions
            double phase = 0;
            for(decltype(nBase) k=0; k<nBase; k++) {
                phase += weights.at(k)*baseFac[k];
            }
            // Compute cosine and sine component
            row.at(j*2-1) =  std::cos(phase);
            row.at(j*2)   = -std::sin(phase);
        }
    }

    return true;
}

bool HBCore::buildAPFT(Status& s) {
    auto n = timepoints.size();
    auto m = spurs_.spectrum().size();
    auto ncoef = 2*m-1;

    if (!buildTransformMatrix(IAPFT, s)) {
        return false;
    }

    // Make a copy that will be destroyed during matrix inversion
    // First size it and make it column-major
    DenseMatrix<double> coeffs(ncoef, ncoef, DenseMatrix<double>::Major::Column);
    // Then use DenseMatrixView's operator=() to copy rows, 
    // use DenseMatrixView from DenseMatrix coeffs. 
    static_cast<DenseMatrixView<double>&>(coeffs) = IAPFT;
    
    // Invert to obtain APFT
    // Destination matdix must be column-major so LAPACK is used
    APFT.resize(n, n, DenseMatrix<double>::Major::Column);
    rowPerm_.resize(ncoef);
    VectorView<int> rowPermView(rowPerm_);
    if (!coeffs.factorAndInvert(APFT, &rowPermView)) {
        s.set(Status::Analysis, "Failed to compute forward transform matrix.");
        return false;
    }

    // Gamma = APFT
    // GammaInv = IAPFT
    
    // Omega .. derivative operator operating on frequency-domain vectors
    // Assumes the first component is DC magnitude and the remaining ones are (cosine, -sine) 
    // magnitudes, i,e, (Re, Im) parts of a phasor
    // First 6 rows and columns are
    //   0 0   0   0    0  .
    //   0 0  -w_1 0    0  .
    //   0 w_1 0   0    0  .
    //   0 0   0   0   -w_2 .
    //   0 0   0   w_2  0  .
    //   . .  .  .   .  .
    // where wi is (2 pi fi). 

    // Compute Omega Gamma as row-major matrix (default)
    // DC row is 0
    // cos row x omega   -> -sin row
    // -sin row x -omega -> cos row
    OmegaGamma.resize(n, n);
    // DC row
    auto destRow = OmegaGamma.row(0);
    for(decltype(n) i=0; i<n; i++) {
        destRow[i] = 0;
    }
    // cos and -sin row
    for(decltype(n) i=1; i<m; i++) {
        auto omega = 2*std::numbers::pi*spurs_.spectrum()[i];
        auto baseNdx = 1+2*(i-1);
        auto cosRow = APFT.row(baseNdx);
        auto negSinRow = APFT.row(baseNdx+1);

        auto destCosRow = OmegaGamma.row(baseNdx);
        auto destNegSinRow = OmegaGamma.row(baseNdx+1);

        destCosRow.writeScaled(negSinRow, -omega);
        destNegSinRow.writeScaled(cosRow, omega);
    }

    // Form GammaInv as column-major matrix
    GammaInvColumnMajor.resize(n, n, DenseMatrix<double>::Major::Column);
    for(decltype(n) i=0; i<n; i++) {
        GammaInvColumnMajor.row(i) = IAPFT.row(i);
    }

    return true;
}

}
