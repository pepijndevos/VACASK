#include <numbers>
#include <complex>
#include "corehbac.h"
#include "spurs.h"
#include "common.h"

namespace NAMESPACE {


// TODO: make list of sources every time core is invoked
//       somebody might sweep cs* parameters of sources


void HBACCore::constructSuffixes() {
    auto& spurs = hbCore_.spurs();
    auto nf = spurs.smsigFreq().size();
    suffixes.resize(nf);
    for (size_t i = 0; i < nf; i++) {
        auto w = spurs.smsigFreqWeights(i);
        std::string s;
        for (size_t k = 0; k < w.n(); k++) {
            if (k > 0) s += ',';
            s += std::to_string(w[k]);
        }
        suffixes[i] = std::move(s);
    }
}


// Construct omega vector: omega[n] = 2*pi*(f + f_n)
// where f_n = smsigFreq[n] is the signed spur frequency from the HB operating point.
// f    - small-signal input frequency (Hz)
// omega - output vector, resized to nf (number of spurs)
void HBACCore::computeOmega(Vector<Real>& omega, Real f) {
    auto& smsigFreq = hbCore_.spurs().smsigFreq();
    auto nf = smsigFreq.size();
    for (size_t n = 0; n < nf; n++) {
        omega[n] = 2.0 * std::numbers::pi * (f + smsigFreq[n]);
    }
}

// Fill one (p,q) subblock of the conversion matrix H(omega).
//
// The (n,m) entry of the subblock is:
//   h_nm = G[k] + j*(omega+omega_n)*C[k]
// where k is the Jacobian harmonic index determined by the mixing stencil,
// and omega is the small-signal frequency for output row n.
//
// Parameters:
//   G    - Fourier coefficients [G_k]_pq of the resistive Jacobian,
//          indexed by 0-based Jacobian frequency index
//          only positive part of the spectrum
//   C    - Fourier coefficients [C_k]_pq of the reactive Jacobian,
//          indexed by 0-based Jacobian frequency index
//          only positive part of the spectrum
//   omega - small-signal frequencies 2 pi (f+f_n), one per output row n
//   block - (p,q) subblock of H(omega) to fill, size nf x nf
//           column-major assumed
void HBACCore::fillDenseBlock(
    const VectorView<Complex>& G,
    const VectorView<Complex>& C,
    const Vector<Real>& omega,
    DenseMatrixView<Complex>& block
) {
    auto& spurs = hbCore_.spurs();
    auto& stencil = spurs.mixingStencil();
    auto nf = stencil.nRows();

    // Outer loop over columns (assume column major matrix)
    auto* p = &block.at(0, 0);
    auto* jacIndex = &stencil.at(0, 0);
    for (size_t m = 0; m < nf; m++) {
        // Omega is common for the whole row
        auto* om = &omega.at(0);
        auto [start, end] = spurs.rowRange(m);
        auto p1 = p + start;
        for(size_t n = start; n < end; n++) {
            if (*jacIndex != Spurs::noJacIndex) {
                bool conjugated = (*jacIndex < 0);
                auto k = static_cast<size_t>(conjugated ? (-*jacIndex) : (*jacIndex - 1));
                Complex g = conjugated ? std::conj(G[k]) : G[k];
                Complex c = conjugated ? std::conj(C[k]) : C[k];
                *p1 = g + Complex(0.0, *om) * c;
                p1++;
                jacIndex++;
                om++;
            }
        }
        p += nf;
    }
}

}
