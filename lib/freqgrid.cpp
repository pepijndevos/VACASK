#include <numeric>
#include "freqgrid.h"
#include "value.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

double FrequencyGrid::toFreq(VectorView<int> weights) {
    double f = 0;
    for(size_t i=0; i<weights.n(); i++) {
        f += weights[i] * fundamentals_[i];
    }
    return f;
}

bool FrequencyGrid::build(const std::vector<double>& fundamentals, const std::vector<int>& nHarmonics, int maxImOrder, bool hybrid, int debug, Status& s) {
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
    
    // Build grid
    grid.resize(0, n); // Empty table
    std::vector<Int> cnt(n);
    std::vector<Int> end(n);
    int lastChanged = 0;
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
        
        // Keep only thode grid entries that survive truncation
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
            auto row = grid.addRow();
            for(decltype(n) i=0; i<n; i++) {
                row.at(i) = cnt[i];
            }
            freq.push_back({
                .gridIndex = grid.nRows()-1, 
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
    auto nf = freq.size();
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
            auto df = std::abs(std::abs(freq[i].f) - std::abs(freq[j].f));
            auto tolref = std::max(std::abs(freq[i].f), std::abs(freq[j].f));
            if (df <= tolref*freqtol) {
                // Frequencies match, compare order
                if (freq[i].order < freq[j].order) {
                    // j has higher order, keep i, mark j as removed, continue
                    removed[j] = true;
                    conflict = true;
                    if (debug>0) {
                        Simulator::out() << "Removing #" << j << " (higher order)\n";
                    }
                 } else if (freq[i].order > freq[j].order) {
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
                    if (freq[i].isHarmonic && !freq[j].isHarmonic) {
                        // i is harmonic, j is not, keep i, mark j as removed, continue
                        removed[j] = true;
                        conflict = true;
                        if (debug>0) {
                            Simulator::out() << "Removing #" << j << " (not harmonic)\n";
                        }
                    } else if (!freq[i].isHarmonic && freq[j].isHarmonic) {
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

    // Compact freq and grid
    decltype(nf) dest = 0;
    for(decltype(nf) i=0; i<nf; i++) {
        if (!removed[i]) {
            if (i!=dest) {
                freq[dest] = freq[i];
                freq[dest].gridIndex = dest;
                grid.row(dest) = grid.row(i);
            }
            dest++;
        }
    }
    freq.resize(dest);
    nf = freq.size();

    // Sort freq and grid rows together by frequency
    // After compaction freq[i].gridIndex == i
    {
        // Build permutation sorted by frequency
        std::vector<size_t> perm(nf);
        std::iota(perm.begin(), perm.end(), 0);
        std::sort(perm.begin(), perm.end(), [&](size_t a, size_t b) {
            return std::abs(freq[a].f) < std::abs(freq[b].f);
        });

        // Apply permutation to freq and grid
        DenseMatrix<int> gridSorted;
        gridSorted.resize(0, n);
        std::vector<SpecFreq> freqSorted;
        freqSorted.reserve(nf);
        for(decltype(nf) i=0; i<nf; i++) {
            gridSorted.addRow() = grid.row(perm[i]);
            auto sf = freq[perm[i]];
            sf.gridIndex = i;
            freqSorted.push_back(sf);
        }
        freq = std::move(freqSorted);
        grid = std::move(gridSorted);
    }
    
    // Build frequencies vector for the solver
    spectrum_.resize(dest);
    signedSpectrum_.resize(dest);
    for(decltype(dest) i=0; i<nf; i++) {
        spectrum_[i] = std::abs(freq[i].f);
        signedSpectrum_[i] = freq[i].f;
    }

    // Make sure DC is index 0
    if (freq[0].f!=0) {
        s.set(Status::CreationFailed, "Failed to create spectrum, component 0 must be DC.");
        return false;
    }

    if (debug>0) {
        Simulator::out() << "Spectrum, " << freq.size() << " frequencies\n";
        auto nn = grid.nRows();
        for(auto& fd : freq) {
            std::cout << "  #" << fd.gridIndex << " [";
            auto row = grid.row(fd.gridIndex);
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

bool FrequencyGrid::buildMixingMap(int debug, Status& s) {
    auto n = grid.nCols();
    auto nf = grid.nRows();
    auto firstNegative = nf;

    smsigFreq_ = signedSpectrum_;

    if (conflict) {
        s.set(Status::CreationFailed, "Cannot create mixing stencil due to grid frequency conflict.");
        return false;
    }

    // Add negatives for cyclostationary AC, SP, STB, NOISE
    // Skip DC
    for(decltype(nf) i=1; i<nf; i++) {
        auto row = grid.addRow();
        auto fromRow = grid.row(i);
        for(decltype(nf) j=0; j<n; j++) {
            row[j] = -fromRow[j];
            smsigFreq_.push_back(toFreq(row));
        }
    }
    
    // From this point on the grid no longer changes
    // Build a map from fingerprints (key is VectorView<int>) to grid row indices
    nf = grid.nRows();
    for(decltype(nf) i=0; i<nf; i++) {
        freqMap[grid.row(i)] = i;
    }

    // Build index stencil
    // Traverse input and Jacobian frequency fingerprints
    // Compute output frequency fingerprint
    // Fill map (output freq index, input freq index) -> Jacobian freq index
    DenseMatrix<int> mat(1, n, DenseMatrix<int>::Major::Row);
    auto outComp = mat.row(0);
    indexStencil.resize(nf, nf);
    // Default is no Jacobian index for (out, in)
    indexStencil.fill(noJacIndex);
    for(decltype(nf) inF=0; inF<nf; inF++) {
        auto inComp = grid.row(inF);
        for(decltype(nf) jacF=0; jacF<nf; jacF++) {
            auto jacComp = grid.row(jacF);
            // Compute output weigths
            for(decltype(n) k=0; k<n; k++) {
                outComp[k] = inComp[k] + jacComp[k];
            }
            // Look up in grid
            auto it = freqMap.find(outComp);
            if (it!=freqMap.end()) {
                // In map, get grid index of Jacobian freq
                auto outF = it->second;
                // Is it negative
                int32_t jacIndex;
                if (jacF>=firstNegative) {
                    jacIndex = -(jacF - firstNegative + 1);
                } else {
                    jacIndex = jacF;
                }
                indexStencil.at(outF, inF) = jacIndex;
            }
        }
    }

    if (debug) {
        Simulator::out() << "Mixing map (out, in) : jac\n";
        for(decltype(nf) iout=0; iout<nf; iout++) {
            for(decltype(nf) iin=0; iin<nf; iin++) {
                auto ijac = indexStencil.at(iout, iin);
                if (ijac!=noJacIndex) {
                    auto fin = smsigFreq_[iin]; 
                    auto fout = smsigFreq_[iout];
                    auto fjac = freq[std::abs(ijac)].f;
                    Simulator::out() << "  (" << fout << ", " << fin << ") : " << fjac << (ijac<0 ? " (conjugated)" : "") << "\n";
                    int a=1;
                }
            }
        }
    }
    
    return true;
}

}
