#include "freqgrid.h"
#include "value.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

bool FrequencyGrid::build(const std::vector<int>& nHarmonics, int maxImOrder, bool hybrid, int debug, Status& s) {
    auto n = fundamentals.size();

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
        double f = 0;
        Int nnz = 0;
        for(decltype(n) i=0; i<n; i++) {
            order += std::abs(cnt[i]);
            f += cnt[i]*fundamentals[i];
            if (cnt[i]!=0) {
                nnz++;
            }
        }
        // Need to remember this to correctly compute phase when assembling APFT
        auto negative = f<0;
        // Use positive frequencies only
        f = std::abs(f);
        
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
                .f = f,
                .negative = negative,  
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
            auto df = std::abs(freq[i].f - freq[j].f);
            auto tolref = std::max(freq[i].f, freq[j].f);
            if (df <= tolref*freqtol) {
                // Frequencies match, compare order
                if (freq[i].order < freq[j].order) {
                    // j has higher order, keep i, mark j as removed, continue
                    removed[j] = true;
                    if (debug>0) {
                        Simulator::out() << "Removing #" << j << " (higher order)\n";
                    }
                 } else if (freq[i].order > freq[j].order) {
                    // i has higher order, keep j, mark i as removed, exit inner loop
                    removed[i] = true;
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
                        if (debug>0) {
                            Simulator::out() << "Removing #" << j << " (not harmonic)\n";
                        }
                    } else if (!freq[i].isHarmonic && freq[j].isHarmonic) {
                        // j is harmonic, i is not, keep j, mark i as removed, exit inner loop
                        removed[i] = true;
                        if (debug>0) {
                            Simulator::out() << "Removing #" << i << " (not harmonic)\n";
                        }
                        break;
                    } else {
                        // Harmonic satus is the same, keep the one with lower index (i), mark j as removed
                        removed[j] = true;
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
            }
            dest++;
        }
    }
    freq.resize(dest);

    // Sort freq vector by frequency
    std::sort(
        freq.begin(), freq.end(), 
        [](const SpecFreq& a, const SpecFreq& b) { return a.f < b.f; }
    );

    // Build frequencies vector for the solver
    spectrum.resize(dest);
    for(decltype(dest) i=0; i<dest; i++) {
        spectrum[i] = freq[i].f;
    }

    if (debug>0) {
        Simulator::out() << "Spectrum, " << freq.size() << " frequencies\n";
        auto nn = grid.nRows();
        for(auto& fd : freq) {
            std::cout << "  #" << fd.gridIndex << " [";
            auto row = grid.row(fd.gridIndex);
            auto nel = row.n();
            auto sgn = fd.negative ? -1 : 1;
            for(decltype(nel) j=0; j<nel; j++) {
                std::cout << row.at(j)*sgn << " ";
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
    
    return true;
}

}
