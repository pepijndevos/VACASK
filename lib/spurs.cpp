#include <numeric>
#include "spurs.h"
#include "value.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

std::tuple<double, int, int> Spurs::spurStats(VectorView<Int>& weights) const {
    double f = 0;
    int ord = 0, nz = 0;
    for(size_t i=0; i<fundamentals_.size(); i++) {
        f += weights[i] * fundamentals_[i];
        ord += std::abs(weights[i]);
        if (weights[i]!=0) {
            nz++;
        }
    }
    return std::make_tuple(f, ord, nz);
}

void Spurs::buildSmsig() {
    auto nf = spectrum_.size();
    auto firstNegative = nf;

    // spectrum_, and smsigFreq_ are sorted by frequency
    // The lower part of smsigFreq_ (negatives of spectrum_)
    // can be constructed immediately.
    // Skip DC 
    smsigFreq_.clear();
    smsigFreqWeightIndices_.clear();
    for(decltype(nf) posIndex=nf-1; posIndex>0; posIndex--) {
        smsigFreq_.push_back(-spectrum_[posIndex]);
        // Weight indices at firstNegative correspond to the 
        // negative of lowest spectrum_ frequency that is >0
        smsigFreqWeightIndices_.push_back(firstNegative+posIndex-1);
    }

    // Index of DC
    dcIndex = smsigFreq_.size();

    // Append positive frequencies
    for(decltype(nf) i=0; i<nf; i++) {
        smsigFreq_.push_back(spectrum_[i]);
        smsigFreqWeightIndices_.push_back(i);
    }

    // Indices in unpruned smsigFreq_ vector
    fullSmsigFreqIndex_.resize(2*nf-1);
    for(decltype(nf) i=0; i<2*nf-1; i++) {
        fullSmsigFreqIndex_[i] = i;
    }

}

bool Spurs::build(const std::vector<double>& fundamentals, const std::vector<Int>& nHarmonics, int maxImOrder, bool hybrid, int debug, Status& s) {
    fundamentals_ = fundamentals;

    Vector<int> orderVec;
    Vector<int> nnzVec;
    
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
    spurs_.clear();
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
        
        // Compute spur properties
        auto vv = VectorView<int>(cnt);
        auto [f, order, nnz] = spurStats(vv);
        
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
            spurs_.push_back(f);
            orderVec.push_back(order);
            nnzVec.push_back(nnz);
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
            auto df = std::abs(std::abs(spurs_[i]) - std::abs(spurs_[j]));
            auto tolref = std::max(std::abs(spurs_[i]), std::abs(spurs_[j]));
            if (df <= tolref*freqtol) {
                // Frequencies match, compare order
                if (orderVec[i] < orderVec[j]) {
                    // j has higher order, keep i, mark j as removed, continue
                    removed[j] = true;
                    conflict = true;
                    if (debug>0) {
                        Simulator::out() << "Removing #" << j << " (higher order)\n";
                    }
                 } else if (orderVec[i] > orderVec[j]) {
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
                    if (nnzVec[i]<=1 && !nnzVec[j]<=1) {
                        // i is harmonic, j is not, keep i, mark j as removed, continue
                        removed[j] = true;
                        conflict = true;
                        if (debug>0) {
                            Simulator::out() << "Removing #" << j << " (not harmonic)\n";
                        }
                    } else if (!nnzVec[i]<=1 && nnzVec[j]<=1) {
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
                spurWeights_.row(dest) = spurWeights_.row(i);
            }
            dest++;
        }
    }
    spurs_.resize(dest);
    nf = spurs_.size();

    // Negate weights of negative frequencies, negate negative frequencies
    for(decltype(nf) i=0; i<nf; i++) {
        if (spurs_[i]<0) {
            spurs_[i] = -spurs_[i];
            for(decltype(n) j=0; j<n; j++) {
                spurWeights_.at(i, j) = -spurWeights_.at(i, j);
            }
        }
    }

    // Sort spurs_ and spurWeights_ 
    // Note that frequencies are all positive now
    {
        // Build permutation sorted by frequency
        std::vector<size_t> perm(nf);
        std::iota(perm.begin(), perm.end(), 0);
        std::sort(perm.begin(), perm.end(), [&](size_t a, size_t b) {
            return spurs_[a] < spurs_[b];
        });

        // Apply permutation to spurs_ and spurWeights_
        DenseMatrix<Int> weightsSorted;
        weightsSorted.resize(0, n);
        std::vector<double> spursSorted;
        spursSorted.reserve(nf);
        for(decltype(nf) i=0; i<nf; i++) {
            weightsSorted.addRow() = spurWeights_.row(perm[i]);
            auto sf = spurs_[perm[i]];
            spursSorted.push_back(sf);
        }
        spurs_ = std::move(spursSorted);
        spurWeights_ = std::move(weightsSorted);
    }
    
    // Build spectrum for the hb solver
    spectrum_.resize(dest);
    for(decltype(dest) i=0; i<nf; i++) {
        spectrum_[i] = spurs_[i];
    }

    // Make sure DC is index 0
    if (spurs_[0]!=0) {
        s.set(Status::CreationFailed, "Failed to create spectrum, hb spectrum component 0 must be DC.");
        return false;
    }
    
    // Make sure DC weights are all 0
    for(decltype(n) i=0; i<n; i++) {
        if (spurWeights_.at(0, i)!=0) {
            s.set(Status::CreationFailed, "Failed to create spectrum, DC weights are nonzero.");
            return false;
        }
    }

    // Add weights for negatives af spectrum_ ((quasi)periodic AC, XF, SP, STB, NOISE)
    // Add them to spurWeights_ and smsigFreq_, and spurs_ array
    // Skip DC
    for(decltype(nf) i=1; i<nf; i++) {
        auto row = spurWeights_.addRow();
        auto fromRow = spurWeights_.row(i);
        for(decltype(nf) j=0; j<n; j++) {
            row[j] = -fromRow[j];
        }
        auto [f, order, nnz] = spurStats(row);
        spurs_.push_back(f);
    }

    // Build unpruned small signal analysis spectrum (symmetric)
    buildSmsig();

    if (debug>0) {
        Simulator::out() << "Spurs, " << spurs_.size() << " frequencies\n";
        auto nn = spurWeights_.nRows();
        auto cnt = 0;
        for(decltype(nn) cnt=0; cnt<nn; cnt++) {
            auto fd = spurs_[cnt];
            std::cout << "  #" << cnt << " [";
            auto row = spurWeights_.row(cnt);
            auto nel = row.n();
            auto [f, order, nnz] = spurStats(row);
            for(decltype(nel) j=0; j<nel; j++) {
                std::cout << row.at(j) << " ";
            }
            std::cout << "]";
            std::cout << " f=" << fd;
            std::cout << " order=" << order;
            if (nnz<=1) {
                std::cout << " harmonic";
            }
            std::cout << "\n";
        }
    }

    return true;
}

// Go through nonnegative small-signal frequencies, prune frequencies 
// - exceeding maximal harmonic weight
// - exceeding maximal frequency
// Prunes smsigFreq_, smsigFreqWeightIndices_, and fullSmsigFreqIndex_. 
// Updates dcIndex. 
// buildMixingMap() takes into account only frequencies that were not pruned. 
void Spurs::prune(const Vector<Int>& maxHarm, double maxFreq) {
    auto n = fundamentals_.size();
    auto nf = smsigFreq_.size();
    std::vector<bool> pruneFlag(nf, false);
    // Positive frequencies only
    for(decltype(nf) i=dcIndex; i<nf; i++) {
        bool prune = false;
        auto wi = smsigFreqWeightIndices_[i];
        for(decltype(n) j=0; j<n; j++) {
            if (maxHarm[j]>=0 && std::abs(spurWeights_.at(wi, j)>maxHarm[j])) {
                prune = true;
                break;
            }
        }
        if (!prune && maxFreq>=0) {
            if (std::abs(smsigFreq_[i])>maxFreq) {
                prune = true;
            }
        }
        // Store prune flag, prune positive and negative frequency
        if (prune) {
            pruneFlag[i] = prune;
            pruneFlag[dcIndex-(i-dcIndex)] = prune;
        }
    }

    // Prune
    auto dest = 0;
    decltype(dcIndex) newDcIndex;
    for(decltype(nf) i=0; i<nf; i++) {
        if (!pruneFlag[i]) {
            // Copy
            smsigFreq_[dest] = smsigFreq_[i];
            smsigFreqWeightIndices_[dest] = smsigFreqWeightIndices_[i];
            fullSmsigFreqIndex_[dest] = fullSmsigFreqIndex_[i];
            if (i==dcIndex) {
                newDcIndex = dest;
            }
            dest++;
        }
        
    }
    // Fix dc Index
    dcIndex = newDcIndex;
}

bool Spurs::buildMixingMap(Int debug, Status& s) {
    auto n = spurWeights_.nCols();
    auto nf = smsigFreq_.size();
    
    if (conflict) {
        s.set(Status::CreationFailed, "Cannot create mixing stencil due to spur conflict.");
        return false;
    }
    
    // smsigFreq_ is sorted by increasing frequency
    // smsigFreqWeightIndices_ holds the corresponding weights indices (also indices in spurs_ vector)
    
    // Build a map from spur weights (key is VectorView<Int>) to indices in 
    // smsigFreq_
    smsigFreqMap.clear();
    for(decltype(nf) i=0; i<nf; i++) {
        smsigFreqMap[spurWeights_.row(smsigFreqWeightIndices_[i])] = i;
    }

    // Mapping from indices in smsigFreq_ to indices of weights is handled by 
    // smsigFreqWeightIndices_
    
    // Build index stencil
    // Traverse input and Jacobian spur weights
    // Compute output spur weights
    // Fill map (output freq index, input freq index) -> Jacobian freq index
    // Encoding of Jacoibian component index
    // - <0 = no entry (noJacIndex)
    // - 0 = absolute largest negative frequency
    // - ...
    // - dcIndex = DC
    // - ...
    // - nf-1 = abslute largest positive frequency
    
    // Temporary storage
    DenseMatrix<Int> mat(1, n, DenseMatrix<Int>::Major::Row);
    auto outW = mat.row(0);

    // Mixing stencil must be column-major because that is the way we traverse sparse matrices
    mixingStencil_.resize(nf, nf, DenseMatrix<Int>::Major::Column);
    
    // Default is no Jacobian index for (out, in)
    mixingStencil_.fill(noJacIndex);
    for(decltype(nf) inF=0; inF<nf; inF++) {
        auto inW = spurWeights_.row(smsigFreqWeightIndices_[inF]);
        for(decltype(nf) jacF=0; jacF<nf; jacF++) {
            // Compute output weigths
            auto jacW = spurWeights_.row(smsigFreqWeightIndices_[jacF]);
            for(decltype(n) k=0; k<n; k++) {
                outW[k] = inW[k] + jacW[k];
            }
            // Look up output spur index (index of frequency in smsigFreq_)
            auto it = smsigFreqMap.find(outW);
            if (it!=smsigFreqMap.end()) {
                // In map, get spur index
                auto outF = it->second;
                // Write mixing stencil
                mixingStencil_.at(outF, inF) = jacF;
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
            if (mixingStencil_.at(row, col)>=0) {
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

    if (debug) {
        Simulator::out() << "Mixing map (out, in) : jac\n";
        for(decltype(nf) iout=0; iout<nf; iout++) {
            for(decltype(nf) iin=0; iin<nf; iin++) {
                auto ijac = mixingStencil_.at(iout, iin);
                if (ijac>=0) {
                    auto fin = smsigFreq_[iin]; 
                    auto fout = smsigFreq_[iout];
                    auto fjac = smsigFreq_[ijac];
                    Simulator::out() << "  (" << fout << ", " << fin << ") : " << fjac << (ijac<0 ? " (conjugated)" : "") << "\n";
                }
            }
        }
    }

    return true;
}

std::tuple<bool, size_t> Spurs::smsigFreqIndex(double f, double tol) const {
    // smsigFreq_ is sorted by value; find the first element >= f
    auto it = std::lower_bound(smsigFreq_.begin(), smsigFreq_.end(), f);
    // Check the candidate and the element just before it
    for (auto jt : {it, std::prev(it)}) {
        if (jt < smsigFreq_.begin() || jt == smsigFreq_.end()) continue;
        auto ref = std::max(std::abs(f), std::abs(*jt));
        if (std::abs(f - *jt) <= ref * tol) {
            auto smsigIndex = static_cast<size_t>(jt - smsigFreq_.begin());
            return {true, smsigIndex};
        }
    }
    return {false, 0};
}

// Resolve a Value specifying a spur into smsigFreq_ index.
//
// Accepted formats (matching devvisrc.h spurs / corehbac.h outspur):
//   Real         - spur frequency; resolved via smsigFreqIndex()
//   IntVector    - tone weights; looked up in smsigFreqMap
//
// Returns {true, index} on success, {false, 0} if not found.
std::tuple<bool, size_t> Spurs::smsigFreqIndex(const Value& v) const {
    switch (v.type()) {
        case Value::Type::Real: {
            return smsigFreqIndex(v.val<const Real>());
        }
        case Value::Type::IntVec: {
            auto& w = v.val<const IntVector>();
            if (w.size() != fundamentals_.size()) {
                return {false, 0};
            }
            auto it = smsigFreqMap.find(VectorView<Int>(const_cast<Int*>(w.data()), w.size()));
            if (it == smsigFreqMap.end()) {
                return {false, 0};
            }
            return {true, it->second};
        }
        default:
            return {false, 0};
    }
}

}
