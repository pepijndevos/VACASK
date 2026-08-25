#ifndef __CIRDELAY_DEFINED
#define __CIRDELAY_DEFINED

#include <optional>
#include "ansupport.h"
#include "klumatrix.h"
#include "klubsmatrix.h"
#include "common.h"


namespace NAMESPACE {

// Per-slot (pointer to (out,in) element, pointer to (out,out) element) pair,
// filled in by bindToMatrix() below. Can also hold DenseMatrixViews. 
template<typename T> using DelayMatrixBindings = Vector<std::tuple<T, T>>;

class DelayLines {
public:
    // Initial per-slot history buffer size (first upsize, from empty) and
    // growth factor (every subsequent upsize) used by addSample().
    static const HistoryDepthIndex initialHistorySize = 32;
    static const HistoryDepthIndex historyGrowthFactor = 2;

    void scale(GlobalStorageIndex n) {
        inputUnknown_.resize(n);
        outputUnknown_.resize(n);
        maxDelay_.resize(n);
    };

    void scaleHistory(GlobalStorageIndex n) {
        history_.resize(n);
    };

    bool bindToUnknowns(GlobalStorageIndex slot, UnknownIndex inputUnknown, UnknownIndex outputUnknown, Status& s=Status::ignore) {
        inputUnknown_[slot] = inputUnknown;
        outputUnknown_[slot] = outputUnknown;
        return true;
    };

    // Matrix type (KluRealMatrix for T=double*, KluComplexMatrix for T=Complex*)
    // follows from the bindings type, so T alone is enough to select an
    // overload - e.g. bindToMatrix<double*>(matResist, mep, bindings, s).
    template<typename T> bool bindToMatrix(
        std::conditional_t<std::is_same_v<T, Complex*>, KluComplexMatrix, KluRealMatrix>& mat,
        const std::optional<MatrixEntryPosition>& mep,
        DelayMatrixBindings<T>& bindings,
        Status& s=Status::ignore
    );

    // Matrix type (KluBlockSparseRealMatrix for T=DenseMatrixView<double>,
    // KluBlockSparseComplexMatrix for T=DenseMatrixView<Complex>) follows
    // from the bindings type, same as bindToMatrix() above - e.g.
    // bindToMatrixBlock<DenseMatrixView<double>>(matResist, bindings, s).
    template<typename T> bool bindToMatrixBlock(
        std::conditional_t<std::is_same_v<T, DenseMatrixView<Complex>>, KluBlockSparseComplexMatrix, KluBlockSparseRealMatrix>* mat,
        DelayMatrixBindings<T>& bindings,
        Status& s=Status::ignore
    );

    void setDelay(GlobalStorageIndex slot, double delay) { delay_[slot] = delay; };
    void setMaxDelay(GlobalStorageIndex slot, double maxDelay) { maxDelay_[slot] = maxDelay; };

    // Record a new (time, value) sample for delay line slot, growing its
    // history buffer first if needed. timepointHistory is the (externally
    // owned/updated) shared buffer of past simulation times - read here
    // only, never written (that happens elsewhere, once per timestep).
    void addSample(GlobalStorageIndex slot, double time, double value, CircularBuffer<double>& timepointHistory) {
        auto& h = history_[slot];
        auto n = h.valueCount();
        if (n == h.size()) {
            // Buffer full - adding this sample would evict the current
            // oldest one. That's fine as long as what would become the new
            // oldest sample (today's second-oldest) is already further back
            // than maxDelay_ - nothing a future delay lookup could still
            // need would be lost. n<2 means there's no second-oldest to
            // check yet (covers the very first call, size()==0), so upsize
            // unconditionally in that case.
            bool needUpsize = true;
            if (n >= 2) {
                double secondOldestTime = timepointHistory.at(static_cast<CircularBuffer<double>::DepthIndexDelta>(n - 2));
                double distance = time - secondOldestTime;
                needUpsize = (distance <= maxDelay_[slot]);
            }
            if (needUpsize) {
                // Grow by historyGrowthFactor, except when starting out (or
                // otherwise below initialHistorySize), which jumps straight
                // to initialHistorySize instead.
                HistoryDepthIndex newSize = (h.size() < initialHistorySize) ? initialHistorySize : h.size() * historyGrowthFactor;
                h.upsize(newSize);
            }
        }
        h.add(value);
    };

    // Computes sample via linear interpolation.
    // Returns tuple holding
    // - sample value
    // - derivative of sample value wrt. input unknown value from currentUnknownValues vector
    // currentUnknownValues is the vector of unknowns at currentTime.
    // Clips delay above to maxDelay and below to 0.
    // timepointHistory pairs up with history_[slot]: timepointHistory.at(k)
    // is the time at which history_[slot].at(k) was recorded (same
    // depth-index convention as addSample() relies on).
    std::tuple<double, double> getSample(GlobalStorageIndex slot, double currentTime, Vector<double>& currentUnknownValues, double delay, CircularBuffer<double>& timepointHistory) {
        // Clip delay to [0, maxDelay_[slot]]
        double clippedDelay = delay;
        if (clippedDelay < 0.0) {
            clippedDelay = 0.0;
        } else if (clippedDelay > maxDelay_[slot]) {
            clippedDelay = maxDelay_[slot];
        }
        double targetTime = currentTime - clippedDelay;

        // Live value of the delay line's input at currentTime - not yet in
        // history_[slot] (that happens via a later addSample() call), but
        // it's the newest available data point and the only one the result
        // can actually depend on (everything in history_[slot] was already
        // fixed by a previous, converged timestep).
        double liveValue = currentUnknownValues[inputUnknown_[slot]];

        auto& h = history_[slot];
        auto n = h.valueCount();

        if (n == 0) {
            // No history yet - the live value is all there is.
            return std::make_tuple(liveValue, 1.0);
        }

        double newestHistoryTime = timepointHistory.at(0);
        if (targetTime >= newestHistoryTime) {
            // Target falls between "now" (live) and the newest stored
            // sample - only endpoint depending on currentUnknownValues is
            // the live one, so this is the only case with a nonzero
            // derivative.
            if (currentTime == newestHistoryTime) {
                // Degenerate (zero-width) interval - avoid a 0/0 divide.
                return std::make_tuple(liveValue, 1.0);
            }
            double frac = (currentTime - targetTime) / (currentTime - newestHistoryTime);
            double value = liveValue + (h.at(0) - liveValue) * frac;
            double deriv = 1.0 - frac;
            return std::make_tuple(value, deriv);
        }

        // Target is older than the newest stored sample (both interpolation
        // endpoints will be fixed history, so derivative is 0 from here on).
        // timepointHistory is sorted newest-to-oldest as depth-index
        // increases, so finding the consecutive pair straddling targetTime
        // is a classic monotonic-predicate search: binary search for the
        // largest index whose time is still >= targetTime, instead of
        // walking every sample (history_ can grow into the hundreds/
        // thousands via addSample()'s doubling).
        HistoryDepthIndex lo = 0, hi = n - 1;
        while (lo < hi) {
            HistoryDepthIndex mid = lo + (hi - lo + 1) / 2;
            if (timepointHistory.at(mid) >= targetTime) {
                lo = mid;
            } else {
                hi = mid - 1;
            }
        }
        if (lo == n - 1) {
            // Older than (or exactly at) the oldest stored sample - clamp to it.
            return std::make_tuple(h.at(n - 1), 0.0);
        }
        double tNewer = timepointHistory.at(lo);
        double tOlder = timepointHistory.at(lo + 1);
        if (tNewer == tOlder) {
            return std::make_tuple(h.at(lo), 0.0);
        }
        double frac = (tNewer - targetTime) / (tNewer - tOlder);
        double value = h.at(lo) + (h.at(lo + 1) - h.at(lo)) * frac;
        return std::make_tuple(value, 0.0);
    };

    UnknownIndex inputUnknown(GlobalStorageIndex slot) const { return inputUnknown_[slot]; };
    UnknownIndex outputUnknown(GlobalStorageIndex slot) const { return outputUnknown_[slot]; };

private:
    Vector<CircularBuffer<double>> history_;
    Vector<UnknownIndex> inputUnknown_;
    Vector<UnknownIndex> outputUnknown_;
    Vector<double> delay_;
    Vector<double> maxDelay_;
};

}

#endif
