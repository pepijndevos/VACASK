#ifndef __ELSETUP_DEFINED
#define __ELSETUP_DEFINED

#include <cmath>
#include <limits>
#include "ansupport.h"
#include "coretrancoef.h"
#include "coredelay.h"
#include "libplatform.h"
#include "common.h"


namespace NAMESPACE {

class Circuit;

class Model;
class Instance;

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

    void merge(DeviceRequests& other) {
        abort = abort || other.abort;
        finish = finish || other.finish;
        stop = stop || other.stop;
    }
} Requests;

// EvalSetup data that must be stored per thread
typedef struct EvalSetupThreadData {
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

    // Counter of convergence checks, is reset by initialize()
    size_t instancesConvergenceChecks;
    // Counter of convergence checks that resulted in a converged instance, is reset by initialize()
    size_t convergedInstances;

    // Counter of instances that are not converged, is reset by initialize()
    size_t bypassableInstances;
    size_t bypassOpportunuties;
    size_t bypassedInstances;

    // Methods
    void clearInstanceFlags() {
        requests.clear();
        discontinuity = -1;
        limitingApplied = false;
    };

    bool initialize() {
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
    bool setBreakPoint(double t, CommonData& commons) { if (t<nextBreakPoint) { nextBreakPoint = t; } return true; };
    void setMaxFreq(double freq) { if (freq>maxFreq) maxFreq=freq; }; 

    void merge(struct EvalSetupThreadData& other, CommonData& commons) {
        requests.merge(other.requests);
        limitingApplied = limitingApplied || other.limitingApplied;
        setDiscontinuity(other.discontinuity);
        setBoundStep(other.boundStep);
        if (other.nextBreakPoint>=0) setBreakPoint(other.nextBreakPoint, commons);
        setMaxFreq(other.maxFreq);
        instancesConvergenceChecks += other.instancesConvergenceChecks;
        convergedInstances += other.convergedInstances;
        bypassableInstances += other.bypassableInstances;
        bypassOpportunuties += other.bypassOpportunuties;
        bypassedInstances += other.bypassedInstances;
    };
} EvalSetupThreadData;

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
    
    // Former members of CommonData
    double time {0};

    // Convergence check
    // Check reactive residual and Jacobian for convergence
    bool checkReactiveConvergece {};
    
    
    // 
    // Internals
    // 

    // Fast access pointers - do not set manually
    double* oldSolution; // with bucket
    double* oldStates; // states (current data)
    double* newStates; // can be either from states (future data) or dummyStates (current data)

    // Merged evaluation data across threads
    EvalSetupThreadData data;

    // Evaluation data, one structure per thread
    std::vector<EvalSetupThreadData> threadDataStruct;

    // Boolean vector indicating which thread structures were touched
    std::vector<bool> threadUsed;

    // Running in parallel mode
    bool parallel {false};

    EvalSetupThreadData& threadData() { 
        if (parallel) {
            // More than one thread
            return threadDataStruct[cpuIndex()] ;
        } else {
            // One thread
            return data;
        }
    };

//    //
//    // OMP: Need these to be created per-thread and merged after parallel run
//    // 
//
//    // OMP: See also ParserTables::accounting() - maybe we should create these per-thread and merge
//
//    // OMP: osdi_log() needs a mutex - who cares about speed when we are writing messages
//    
//    // Return information on what happened during evaluation
//    // Verilog-A abort/finish/stop
//    struct DeviceRequests requests;
//    
//    // Limiting applied (i.e. $discontinuity(-1))
//    bool limitingApplied {};
//    
//    // Discontinuity signalled
//    // Negative when no discontinuity, set first by an instance that calls $discontinuity with 
//    // a nonnegative argument, updated by subsequent instances that call $discontinuity 
//    // with a lower nonnegative argument. 
//    Int discontinuity;
//    
//    // For setting the upper bound on the timestep
//    // Infinite initially, set first by an instance that calls $bound_step with 
//    // an argument greater than 0, updated by subsequent instances that call 
//    // $bound_step with a lower argument that is greater than 0. 
//    double boundStep {};
//
//    // Next breakpoint
//    // Infinite initially, set first by an instance if the set value is greater than current
//    // time, updated by subsequent instances that set it to a value greater than current time. 
//    double nextBreakPoint;
//    
//    // For setting maximal source frequency
//    // Zero initially, increased by instances that generate a signal. 
//    double maxFreq {};
//
//    // Counter of convergence checks, is reset by initialize()
//    size_t instancesConvergenceChecks;
//    // Counter of convergence checks that resulted in a converged instance, is reset by initialize()
//    size_t convergedInstances;
//
//    // Counter of instances that are not converged, is reset by initialize()
//    size_t bypassableInstances;
//    size_t bypassOpportunuties;
//    size_t bypassedInstances;

    //
    // OMP: Check when these are called - handle properly in OMP case
    // 

    // Methods
    void clearInstanceFlags() {
        data.clearInstanceFlags(); 
    }
    void clearBounds() {
        data.clearBounds();
    }
    void clearThreadInstanceFlags() {
        threadData().clearInstanceFlags();
    };
    void clearThreadBounds() { 
        threadData().clearBounds();
    };
    void markThreadUsed() {
        threadUsed[cpuIndex()] = true;
    }
    bool wasThreadUsed(int i) {
        return threadUsed[i];
    }
    
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

        // Mark as serial
        parallel = false;

        // Initialize common slot (always)
        return data.initialize();
    };

    bool initializeThreads() {
        // Mark as parallel
        parallel = true;
        // Create thread data slots
        auto availableCpus = cpuCount();
        // Resize is cheap if space is already allocated
        threadDataStruct.resize(availableCpus);
        // Initialize slots
        for(decltype(availableCpus) i=0; i<availableCpus; i++) {
            if (!threadDataStruct[i].initialize()) {
                return false;
            }
            threadDataStruct[i].clearInstanceFlags();
            threadDataStruct[i].clearBounds();
        }
        // Flags for indicating a thread was used
        threadUsed.resize(availableCpus);
        std::fill(threadUsed.begin(), threadUsed.end(), false); 
        return true;
    }

    void setBoundStep(double bound) { threadData().setBoundStep(bound); };
    void setDiscontinuity(Int i) { threadData().setDiscontinuity(i); };
    bool setBreakPoint(double t, CommonData& commons) {
        if (std::abs(t-time) <= timeRelativeTolerance*time) {
            // Breakpoint now or close to now, it is too late to take it into account. 
            // It should have been set earlier. 
            // TODO: Maybe signal discontinuity of order 1
        } else if (t<time) {
            // Breakpoint in past, ignore
        } else {
            // Set next breakpoint
            threadData().setBreakPoint(t, commons);
        }
        return true;
    };
    void setMaxFreq(double freq) { threadData().setMaxFreq(freq); }; 

    void merge(CommonData& commons) {
        auto availableCpus = cpuCount();
        for(decltype(availableCpus) i=0; i<availableCpus; i++) {
            if (!threadUsed[i]) {
                continue;
            }
            data.merge(threadDataStruct[i], commons);
        }
    };
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

    // Delay lines
    DelayLines* delayLines_ {nullptr};
    bool firstTimepoint {};

    // Noise modulation function values
    Vector<double>* noiseModulationFunction {nullptr};
    GlobalStorageIndex noiseSourceStride {0};
    // Use jacobianLoadOffset as offset into noiseModulationFunction vector's slot
    Vector<double>* noiseExponent {nullptr};
    // No need to have these per thread in the future because 
    // load is always serial due to Jacobian += races. 
    // TODO: get rid of these by adding per-source acceess functions to OSDI
    Vector<double> noiseDensityScratchpad;
    Vector<double> noiseExponentScratchpad;
    
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
