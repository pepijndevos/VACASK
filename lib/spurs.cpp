#include <numeric>
#include "spurs.h"
#include "value.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

double Spurs::toFreq(VectorView<Int> weights) {
    double f = 0;
    for(size_t i=0; i<weights.n(); i++) {
        f += weights[i] * fundamentals_[i];
    }
    return f;
}

bool Spurs::build(const std::vector<double>& fundamentals, const std::vector<Int>& nHarmonics, int maxImOrder, bool hybrid, int debug, Status& s) {
    fundamentals_ = fundamentals;
    
    auto n = fundamentals_.size();

    // Tone comparison tolerance
    auto freqtol = 1e-14;

    // Check nHarmonics, get maximum
    auto nHarmMax = 0;
    for(auto nh : nHarmonics) {
        if (nh>nHarmMax) {
            nHarmMax = nh;
        }
    }
    
    // Build spurs
    spurWeights_.resize(0, n); // Empty table
    std::vector<Int> cnt(n);
    std::vector<Int> end(n);
    Int lastChanged = 0;
    cnt[0] = 0;
    end[0] = nHarmonics[0]+1;

    // Compute immax
    auto immax = std::max(maxImOrder, nHarmMax);
    
    while (true) {
        // Do we need to build ranges
        if (lastChanged<n-1) {
            // Check if all previous coordinates are 0
            bool allZero = true;
            for(decltype(lastChanged) i=0; i<lastChanged+1; i++) {
                if (cnt[i] != 0) {
                    allZero = false;
                    break;
                }
            }
            // From lastChanged+1 to n-1, set up ranges
            for(decltype(lastChanged) i=lastChanged+1; i<n; i++) {
                if (allZero) {
                    cnt[i] = 0;
                } else {
                    cnt[i] = -nHarmonics[i];
                }
                end[i] = nHarmonics[i]+1;
            }
        }

        // Compute properties
        Int order = 0;
        Int nnz = 0;
        for(decltype(n) i=0; i<n; i++) {
            order += std::abs(cnt[i]);
            if (cnt[i]!=0) {
                nnz++;
            }
        }
        
        // Keep only those spurs that survive truncation
        // Check immax
        // Not optimal for diamond truncation because we traverse the whole box and 
        // leave out frequencies with order above immax. 
        // But then again, HB spends a lot more time solving the problem. 
        if (
            (maxImOrder==0) || // Box truncation, accept all
            (order<=immax) ||  // Diamond truncation (maxImOrder>0)
            (hybrid && nnz<=1) // Hybrid: Diamond + single tone harmonics from whole box
        ) {
            // Construct component
            auto row = spurWeights_.addRow();
            for(decltype(n) i=0; i<n; i++) {
                row.at(i) = cnt[i];
            }
            spurs_.push_back({
                .index = spurWeights_.nRows()-1, 
                .f = toFreq(row),
                .order = order, 
                .isHarmonic = nnz<=1, 
            });
        }

        // Advance, count up because size_t is unsigned
        for(decltype(n) i=0; i<n; i++) {
            lastChanged = n-1-i;
            cnt[lastChanged]++;
            if (cnt[lastChanged]<end[lastChanged]) {
                break;
            }
        }

        // Check if done
        if (cnt[0]>=end[0]) {
            // Done
            break;
        }
    }
    
    // Map to spectrum
    // Remove duplicate frequencies
    // Lower order im products are kept over higher order ones
    // Harmonics are kept over im products
    // Lower index is kept over higher index
    conflict = false;
    auto nf = spurs_.size();
    std::vector<bool> removed(nf, false);
    for(decltype(nf) i=0; i<nf-1; i++) {
        // Is i removed
        if (removed[i]) {
            // Go to next i
            continue;
        }
        for(decltype(nf) j=i+1; j<nf; j++) {
            // Is j removed
            if (removed[j]) {
                // Go to next j
                continue;
            }
            auto df = std::abs(std::abs(spurs_[i].f) - std::abs(spurs_[j].f));
            auto tolref = std::max(std::abs(spurs_[i].f), std::abs(spurs_[j].f));
            if (df <= tolref*freqtol) {
                // Frequencies match, compare order
                if (spurs_[i].order < spurs_[j].order) {
                    // j has higher order, keep i, mark j as removed, continue
                    removed[j] = true;
                    conflict = true;
                    if (debug>0) {
                        Simulator::out() << "Removing #" << j << " (higher order)\n";
                    }
                 } else if (spurs_[i].order > spurs_[j].order) {
                    // i has higher order, keep j, mark i as removed, exit inner loop
                    removed[i] = true;
                    conflict = true;
                    if (debug>0) {
                        Simulator::out() << "Removing #" << i << " (higher order)\n";
                    }
                    break;
                } else {
                    // Same order
                    // Check harmonic
                    if (spurs_[i].isHarmonic && !spurs_[j].isHarmonic) {
                        // i is harmonic, j is not, keep i, mark j as removed, continue
                        removed[j] = true;
                        conflict = true;
                        if (debug>0) {
                            Simulator::out() << "Removing #" << j << " (not harmonic)\n";
                        }
                    } else if (!spurs_[i].isHarmonic && spurs_[j].isHarmonic) {
                        // j is harmonic, i is not, keep j, mark i as removed, exit inner loop
                        removed[i] = true;
                        conflict = true;
                        if (debug>0) {
                            Simulator::out() << "Removing #" << i << " (not harmonic)\n";
                        }
                        break;
                    } else {
                        // Harmonic status is the same, keep the one with lower index (i), mark j as removed
                        removed[j] = true;
                        conflict = true;
                        if (debug>0) {
                            Simulator::out() << "Removing #" << j << " (higher index)\n";
                        }
                    }
                }
            }
        }
    }

    // Compact spurs and spurWeights
    decltype(nf) dest = 0;
    for(decltype(nf) i=0; i<nf; i++) {
        if (!removed[i]) {
            if (i!=dest) {
                spurs_[dest] = spurs_[i];
                spurs_[dest].index = dest;
                spurWeights_.row(dest) = spurWeights_.row(i);
            }
            dest++;
        }
    }
    spurs_.resize(dest);
    nf = spurs_.size();

    // Sort spurs_ and spurWeights_ together by frequency
    // After compaction spur[i].index == i
    {
        // Build permutation sorted by frequency
        std::vector<size_t> perm(nf);
        std::iota(perm.begin(), perm.end(), 0);
        std::sort(perm.begin(), perm.end(), [&](size_t a, size_t b) {
            return std::abs(spurs_[a].f) < std::abs(spurs_[b].f);
        });

        // Apply permutation to spurs and spurWeights
        DenseMatrix<Int> gridSorted;
        gridSorted.resize(0, n);
        std::vector<Spur> freqSorted;
        freqSorted.reserve(nf);
        for(decltype(nf) i=0; i<nf; i++) {
            gridSorted.addRow() = spurWeights_.row(perm[i]);
            auto sf = spurs_[perm[i]];
            sf.index = i;
            freqSorted.push_back(sf);
        }
        spurs_ = std::move(freqSorted);
        spurWeights_ = std::move(gridSorted);
    }
    
    // Build frequencies vector for the solver
    spectrum_.resize(dest);
    signedSpectrum_.resize(dest);
    for(decltype(dest) i=0; i<nf; i++) {
        spectrum_[i] = std::abs(spurs_[i].f);
        signedSpectrum_[i] = spurs_[i].f;
    }

    // Make sure DC is index 0
    if (spurs_[0].f!=0) {
        s.set(Status::CreationFailed, "Failed to create spectrum, component 0 must be DC.");
        return false;
    }

    if (debug>0) {
        Simulator::out() << "Spectrum, " << spurs_.size() << " frequencies\n";
        auto nn = spurWeights_.nRows();
        for(auto& fd : spurs_) {
            std::cout << "  #" << fd.index << " [";
            auto row = spurWeights_.row(fd.index);
            auto nel = row.n();
            for(decltype(nel) j=0; j<nel; j++) {
                std::cout << row.at(j) << " ";
            }
            std::cout << "]";
            
            std::cout << " f=" << fd.f;
            std::cout << " order=" << fd.order;
            if (fd.isHarmonic) {
                std::cout << " harmonic";
            }
            std::cout << "\n";
        }
    }

    return buildMixingMap(debug, s);

    return true;
}

bool Spurs::buildMixingMap(Int debug, Status& s) {
    auto n = spurWeights_.nCols();
    auto nf = spurWeights_.nRows();
    auto firstNegative = nf;

    smsigFreq_ = signedSpectrum_;

    if (conflict) {
        s.set(Status::CreationFailed, "Cannot create mixing stencil due to spur conflict.");
        return false;
    }

    // Add negatives for cyclostationary AC, SP, STB, NOISE
    // Add them to spurWeights_ and smsigFreq_, do not add them to spur array
    // Skip DC
    for(decltype(nf) i=1; i<nf; i++) {
        auto row = spurWeights_.addRow();
        auto fromRow = spurWeights_.row(i);
        for(decltype(nf) j=0; j<n; j++) {
            row[j] = -fromRow[j];
        }
        smsigFreq_.push_back(toFreq(row));
    }
    
    // From this point on the set of frequencies and spurs_ no longer changes
    // Build a map from spur weights (key is VectorView<Int>) to spur indices
    nf = spurWeights_.nRows();
    for(decltype(nf) i=0; i<nf; i++) {
        spurMap[spurWeights_.row(i)] = i;
    }

    // Build index stencil
    // Traverse input and Jacobian spur weights
    // Compute output spur weights
    // Fill map (output freq index, input freq index) -> Jacobian freq index
    DenseMatrix<Int> mat(1, n, DenseMatrix<Int>::Major::Row);
    auto outW = mat.row(0);
    mixingStencil_.resize(nf, nf);
    // Default is no Jacobian index for (out, in)
    mixingStencil_.fill(noJacIndex);
    for(decltype(nf) inF=0; inF<nf; inF++) {
        auto inW = spurWeights_.row(inF);
        for(decltype(nf) jacF=0; jacF<nf; jacF++) {
            auto jacW = spurWeights_.row(jacF);
            // Compute output weigths
            for(decltype(n) k=0; k<n; k++) {
                outW[k] = inW[k] + jacW[k];
            }
            // Look up spur index
            auto it = spurMap.find(outW);
            if (it!=spurMap.end()) {
                // In map, get spur index of Jacobian freq
                auto outF = it->second;
                // Is it negative
                Int jacIndex;
                if (jacF>=firstNegative) {
                    // Negative of a spur
                    jacIndex = -(jacF - firstNegative + 1);
                } else {
                    // Original spur
                    jacIndex = jacF+1;
                }
                mixingStencil_.at(outF, inF) = jacIndex;
            }
        }
    }

    // Find first and last nonzero element in each column
    rowStartNonzero.clear();
    rowEndNonzero.clear();
    for(decltype(nf) col=0; col<nf; col++) {
        decltype(nf) first = 0;
        decltype(nf) last = 0;
        bool haveFirst = false;
        bool haveNonzero = false;
        for(decltype(nf) row=0; row<nf; row++) {
            if (mixingStencil_.at(row, col)!=noJacIndex) {
                if (!haveFirst) {
                    first = row;
                    haveFirst = true;
                }
                last = row;
                haveNonzero = true;
            }
        }
        rowStartNonzero.push_back(first);
        rowEndNonzero.push_back(haveNonzero ? last+1 : 0);
    }

    // Build sorted index for spurIndex() binary search
    smsigFreqSorted_.resize(smsigFreq_.size());
    for (size_t i = 0; i < smsigFreq_.size(); i++) {
        smsigFreqSorted_[i] = {smsigFreq_[i], i};
    }
    std::sort(smsigFreqSorted_.begin(), smsigFreqSorted_.end());

    if (debug) {
        Simulator::out() << "Mixing map (out, in) : jac\n";
        for(decltype(nf) iout=0; iout<nf; iout++) {
            for(decltype(nf) iin=0; iin<nf; iin++) {
                auto ijac = mixingStencil_.at(iout, iin);
                if (ijac!=noJacIndex) {
                    auto fin = smsigFreq_[iin]; 
                    auto fout = smsigFreq_[iout];
                    auto fjac = ijac<0 ? spurs_[-ijac].f : spurs_[ijac-1].f;
                    Simulator::out() << "  (" << fout << ", " << fin << ") : " << fjac << (ijac<0 ? " (conjugated)" : "") << "\n";
                }
            }
        }
    }
    
    return true;
}

std::tuple<size_t, bool> Spurs::spurIndex(double f, double tol) const {
    // Lower_bound finds the first element with .first >= f
    auto it = std::lower_bound(smsigFreqSorted_.begin(), smsigFreqSorted_.end(), std::pair(f, size_t(0)));
    // The closest value must be either at it or the element just before it
    for (auto jt : {it, std::prev(it)}) {
        if (jt < smsigFreqSorted_.begin() || jt == smsigFreqSorted_.end()) continue;
        auto ref = std::max(std::abs(f), std::abs(jt->first));
        if (std::abs(f - jt->first) <= ref * tol) {
            return {jt->second, true};
        }
    }
    return {0, false};
}

// Resolve a Value specifying one or more spurs into a vector of smsigFreq_ indices.
//
// Accepted formats (matching devvisrc.h csmixprod / corehbac.h outspur):
//   Real         - spur frequency; resolved via spurIndex()
//   Int          - harmonic number (1-tone HB only); equivalent to IntVector{n}
//   IntVector    - tone weights; looked up in spurMap
//   ValueVec     - list of any of the above, one entry per spur
//   empty IntVector or empty ValueVec - if emptyIsAll, fill with all spur indices
//
// Returns false and sets s on any resolution failure.
bool Spurs::spurIndexVector(const Value& v, std::vector<size_t>& spurIndices, bool emptyIsAll, Status& s) const {
    spurIndices.clear();

    // Helper: resolve one scalar/vector item to a spur index
    auto resolveOne = [&](const Value& item) -> std::tuple<size_t, bool> {
        switch (item.type()) {
            case Value::Type::Real: {
                auto [idx, found] = spurIndex(item.val<const Real>());
                if (!found) {
                    s.set(Status::NotFound, "Spur frequency not found in spectrum.");
                }
                return {idx, found};
            }
            case Value::Type::Int: {
                if (fundamentals_.size() != 1) {
                    s.set(Status::BadArguments, "Integer spur specification is only valid for single-tone analyses.");
                    return {0, false};
                }
                IntVector w = {static_cast<Int>(item.val<const Int>())};
                auto it = spurMap.find(VectorView<Int>(w));
                if (it == spurMap.end()) {
                    s.set(Status::NotFound, "Spur harmonic index not found in spectrum.");
                    return {0, false};
                }
                return {it->second, true};
            }
            case Value::Type::IntVec: {
                auto& w = item.val<const IntVector>();
                if (w.size() != fundamentals_.size()) {
                    s.set(Status::BadArguments, "Spur weight vector length does not match number of fundamentals.");
                    return {0, false};
                }
                auto it = spurMap.find(VectorView<Int>(const_cast<Int*>(w.data()), w.size()));
                if (it == spurMap.end()) {
                    s.set(Status::NotFound, "Spur tone weights not found in spectrum.");
                    return {0, false};
                }
                return {it->second, true};
            }
            default:
                s.set(Status::BadArguments, "Spur must be a frequency (real), harmonic index (integer), or tone weights (integer vector).");
                return {0, false};
        }
    };

    // Empty IntVector or ValueVec: all spurs or nothing
    if (v.isVector() && v.size()==0) {
        if (emptyIsAll) {
            spurIndices.resize(smsigFreq_.size());
            std::iota(spurIndices.begin(), spurIndices.end(), 0);
        }
        return true;
    }

    // ValueVec: list of multiple spur specs
    if (v.type() == Value::Type::ValueVec) {
        size_t i=0;
        for (auto& item : v.val<const ValueVector>()) {
            auto [idx, found] = resolveOne(item);
            if (!found) {
                s.extend(std::string("Check position ")+std::to_string(i)+" in spurs array.");
                return false;
            }
            spurIndices.push_back(idx);
            i++;
        }
        return true;
    }

    // Scalar or single IntVector: one spur
    auto [idx, found] = resolveOne(v);
    if (!found) {
        return false;
    }
    spurIndices.push_back(idx);
    return true;
}

}
