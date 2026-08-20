#ifndef __ELSETUP_DEFINED
#define __ELSETUP_DEFINED

#include <cmath>
#include <limits>
#include "ansupport.h"
#include "coretrancoef.h"
#include "common.h"


namespace NAMESPACE {

class Circuit;

class Model;
class Instance;

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

    bool bind(GlobalStorageIndex slot, UnknownIndex inputUnknown, UnknownIndex outputUnknown) {
        inputUnknown_[slot] = inputUnknown;
        outputUnknown_[slot] = outputUnknown;
        return true;
    };

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
    Vector<double> maxDelay_;
};

typedef struct DeviceRequests {
    // Return information on what happened during evaluation
    // Verilog-A abort/finish/stop
    bool abort {};
    bool finish {};
    bool stop {};

    // Methods
    void clear() {
        abort = false;
        finish = false;
        stop = false;
    };
} Requests;

typedef struct EvalSetup {
    // {} for default initialization
    // State and solution repository
    VectorRepository<double>* solution {};
    VectorRepository<double>::DepthIndexDelta oldSolutionSlot {0};
    VectorRepository<double>* states {}; 
    // For diverting new state output to a bucket (when not nullptr)
    Vector<double>* dummyStates {};

    // Data for instance bypass check (previous values)
    double* deviceStates {};
    
    // Delay history
    DelayLines* delayeLines {};
    CircularBuffer<double>* timpointHistory {};
    
    // What mode are we running in - information for evaluator
    bool staticAnalysis {};
    bool dcAnalysis {};
    bool acAnalysis {};
    bool tranAnalysis {};
    bool noiseAnalysis {};
    bool nodesetEnabled {};
    bool icEnabled {};

    // Limiting control
    bool enableLimiting {}; 
    bool initializeLimiting {};

    // Core evaluations
    bool evaluateResistiveJacobian {};
    bool evaluateReactiveJacobian {};
    bool evaluateResistiveResidual {};
    bool evaluateReactiveResidual {};
    bool evaluateLinearizedResistiveRhsResidual {};
    bool evaluateLinearizedReactiveRhsResidual {};
    bool evaluateNoise {};
    bool evaluateOutvars {};

    // Force bypass
    bool forceBypass {};
    
    // Allow bypassing core evaluation
    bool allowBypass {}; 

    // Request high precision (usually this means that bypass is not possible)
    bool requestHighPrecision;
    
    // Store reactive residual in states or dummyStates
    bool storeReactiveState {};

    // .. what to evaluate beside core 
    bool computeBoundStep {};
    bool computeNextBreakpoint {};
    bool computeMaxFreq {};

    // Numerical differentiation of residual contributions after core evaluations
    // Results are written to states or dummyStates only if not nullptr
    IntegratorCoeffs* integCoeffs {};
    
    // Return information on what happened during evaluation
    // Verilog-A abort/finish/stop
    struct DeviceRequests requests;
    
    // Limiting applied (i.e. $discontinuity(-1))
    bool limitingApplied {};
    
    // Discontinuity signalled
    // Negative when no discontinuity, set first by an instance that calls $discontinuity with 
    // a nonnegative argument, updated by subsequent instances that call $discontinuity 
    // with a lower nonnegative argument. 
    Int discontinuity;
    
    // For setting the upper bound on the timestep
    // Infinite initially, set first by an instance that calls $bound_step with 
    // an argument greater than 0, updated by subsequent instances that call 
    // $bound_step with a lower argument that is greater than 0. 
    double boundStep {};

    // Next breakpoint
    // Infinite initially, set first by an instance if the set value is greater than current
    // time, updated by subsequent instances that set it to a value greater than current time. 
    double nextBreakPoint;
    
    // For setting maximal source frequency
    // Zero initially, increased by instances that generate a signal. 
    double maxFreq {};

    // Counter of instances that are not converged, is reset by initialize()
    size_t bypassableInstances;
    size_t bypassOpportunuties;
    size_t bypassedInstances;

    // Former members of CommonData
    double time {0};

    // Convergence check
    // Check reactive residual and Jacobian for convergence
    bool checkReactiveConvergece {};
    // Counter of convergence checks, is reset by initialize()
    size_t instancesConvergenceChecks;
    // Counter of convergence checks that resulted in a converged instance, is reset by initialize()
    size_t convergedInstances;
    
    // 
    // Internals
    // 

    // Fast access pointers - do not set manually
    double* oldSolution; // with bucket
    double* oldStates; // states (current data)
    double* newStates; // can be either from states (future data) or dummyStates (current data)

    // Methods
    void clearFlags() {
        requests.clear();
        discontinuity = -1;
        limitingApplied = false;
    };

    bool initialize() {
        // DBGCHECK(states && states->size()<2, "States history must have at least two slots.");
        // DBGCHECK(solution && solution->size()<2, "Solution history must have at least two slots.");
        if (solution) {
            oldSolution = solution->data(oldSolutionSlot);
        }
        if (states) {
            oldStates = states->data();
        } else if (dummyStates) {
            // No states given, use dummyStates for oldStates (if dummyStates given)
            // Looks like OSDI devices in eval() still access states, even when 
            // ENABLE_LIM and INIT_LIM are not set. 
            // TODO: research this on MIR level. 
            oldStates = dummyStates->data();
        }
        if (dummyStates) {
            // Dummy states are given when we want to avoid tainting future states
            newStates = dummyStates->data();
        } else if (states) {
            newStates = states->futureData();
        }
        
        if (integCoeffs) {
            DBGCHECK(states->size()<integCoeffs->a().size()+1, "Integration method requires a state history with at least "+std::to_string(integCoeffs->a().size()+1)+" slots.");
            DBGCHECK(states->size()<integCoeffs->b().size()+1, "Integration method requires a state history with at least "+std::to_string(integCoeffs->b().size()+1)+" slots.");
        }

        nextBreakPoint = -1.0;
        boundStep = -1.0;
        maxFreq = 0.0;
        
        instancesConvergenceChecks = 0;
        convergedInstances = 0;

        bypassableInstances = 0;
        bypassOpportunuties = 0;
        bypassedInstances = 0;

        return true;
    };

    void clearBounds() { 
        boundStep=std::numeric_limits<double>::infinity(); 
        nextBreakPoint=std::numeric_limits<double>::infinity(); 
        discontinuity=-1; 
        maxFreq=0.0; 
    };
    void setBoundStep(double bound) { if (bound<boundStep) boundStep=bound; };
    void setDiscontinuity(Int i) { if (i<0) return; if (discontinuity<0 || i<discontinuity) discontinuity=i; };
    bool setBreakPoint(double t, CommonData& commons) {
        if (std::abs(t-time) <= timeRelativeTolerance*time) {
            // Breakpoint now or close to now, it is too late to take it into account. 
            // It should have been set earlier. 
            // Signal discontinuity
        } else if (t<time) {
            // Breakpoint in past, ignore
        } else {
            // Set next breakpoint
            if (t<nextBreakPoint) {
                nextBreakPoint = t;
            }
        }
        return true;
    };
    void setMaxFreq(double freq) { if (freq>maxFreq) maxFreq=freq; }; 
} EvalSetup;


typedef struct LoadSetup {
    // {} for default initialization
    // States - need them whenever
    // - maxReactiveResidualContribution is not nullptr
    // - maxReactiveResidualDerivativeContribution is not nullptr
    // - reactiveResidualDerivative is not nullptr
    // From states we retrieve reactive residual and its derivative wrt time. 
    VectorRepository<double>* states {}; 
    
    // What part of Jacobian to bound locations
    
    // Add resistive Jacobian to bound locations
    bool loadResistiveJacobian {};
    
    // Add reactive Jacobian to bound locations
    // Multiplies Jacobian entries with reactiveJacobianFactor before adding them. 
    bool loadReactiveJacobian {};
    double reactiveJacobianFactor { 1.0 };

    // Add to bound locations
    // - resistive Jacobian 
    // - reactive Jacobian scaled by integCoeffs->leadingCoeff()
    bool loadTransientJacobian {}; 
    IntegratorCoeffs* integCoeffs {};

    // Offset for loading Jacobian elements
    MatrixEntryIndex jacobianLoadOffset {0}; 

    // Where to load resistive residual, skip if nullptr
    double* resistiveResidual {}; // with bucket

    // Where to load reactive residual, skip if nullptr
    double* reactiveResidual {}; // with bucket
    
    // Where to load linearized resistive residual, skip loading if nullptr
    double* linearizedResistiveRhsResidual {}; // with bucket

    // Where to load linearized reactive residual, skip loading if nullptr
    double* linearizedReactiveRhsResidual {}; // with bucket

    // Where to load reactive residual derivative, skip if nullptr
    // Assumes reactive residual derivative was computed at evaluation time 
    // and stored in the states vector (i.e. integCoeffs was not nullptr). 
    double* reactiveResidualDerivative {}; // with bucket

    // Maximal resistive residual contribution per node, skip if nullptr
    double* maxResistiveResidualContribution {}; // with bucket

    // Maximal reactive residual contribution per node, skip if nullptr
    // Assumes reactive residual was stored at evaluation time in the states vector
    // (i.e. storeReactiveState was set to true)
    double* maxReactiveResidualContribution {}; // with bucket

    // Maximal reactive residual derivative contribution per node, skip if nullptr
    // Assumes reactive residual derivative was computed at evaluation time 
    // and stored in the states vector (i.e. integCoeffs was not nullptr). 
    double* maxReactiveResidualDerivativeContribution {}; // with bucket

    // Where to load DC small-signal residual, skip if nullptr
    double* dcIncrementResidual {}; // with bucket

    // Where to load AC small-signal residual, skip if nullptr
    Complex* acResidual {}; // with bucket
    
    // 
    // Internals
    // 

    // Fast access pointers - do not set manually
    double* oldStates; // states (current data)
    double* newStates; // states (future data)
    
    // Methods
    bool initialize() {
        DBGCHECK(states && states->size()<2, "States history must have at least two slots.");
        if (states) {
            oldStates = states->data();
            newStates = states->futureData();
        } else {
            oldStates = newStates = nullptr;
        }
        
        return true;
    };
} LoadSetup;

}

#endif
