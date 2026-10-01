#include <algorithm>
#include <atomic>
#include <stdexcept>
#include "circuit.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

void Circuit::partitionInstances(size_t n) {
    if (n==0) {
        throw std::logic_error("Instance partitioning requires at least one slot.");
    }
    evalPartitioning.clear();

    // Count all instances, skip Hierarchical device
    auto ndev = deviceCount();
    size_t total = primitiveInstanceCount();
    if (total==0) {
        // Stop at first device after Hierarchical
        evalPartitioning.emplace_back(1, SIM_SIZE_T_MAX, SIM_SIZE_T_MAX);
        return;
    }

    // No more slots than instances
    size_t nSlots = std::min(n, total);

    // Even split across nSlots: the first (total % nSlots) slots get one extra instance
    size_t base = total/nSlots;
    size_t remainder = total%nSlots;

    // Make space for pointers and sentinel
    evalPartitioning.reserve(nSlots+1);

    // Start after Hierarchical device, model 0
    size_t idev = 1;
    size_t imod = 0;
    
    
    // Advance (idev, imod) past any devices/models with no instances
    auto skipEmpty = [&]() {
        while (idev<ndev) {
            auto nmod = device(idev)->modelCount();
            // Skip empty models in this device
            while (imod<nmod && device(idev)->model(imod)->instanceCount()==0) {
                imod++;
            }
            // Skipped empty models, not beyond last model in this device
            if (imod<nmod) {
                // Done
                return;
            }
            // Go to next device, first model
            idev++;
            imod = 0;
        }
    };

    // Skip empty devices/models at the beginning
    skipEmpty();

    // Jump by whole models/devices between slot boundaries; only the model a
    // boundary actually falls in gets touched at instance granularity.
    size_t iinst = 0;
    for (size_t slot=0; slot<nSlots; slot++) {
        // Place a slot start pointer
        evalPartitioning.emplace_back(idev, imod, iinst);
        
        // How many devices in this slot? Add one extra to first remainder slots. 
        size_t slotSize = base + ((slot<remainder) ? 1 : 0);
        
        // Count down to 0
        while (slotSize>0) {
            // Remaining devices in model
            auto remainingInModel = device(idev)->model(imod)->instanceCount()-iinst;
            
            // Can we reach slot end in this model?
            if (slotSize<remainingInModel) {
                // Yes
                iinst += slotSize;
                slotSize = 0;
            } else {
                // No, decrease the number of instances we need, skip to next model
                slotSize -= remainingInModel;
                imod++;
                iinst = 0;
                // Skip empty devices/models
                skipEmpty();
            }
        }
    }

    // End sentinel
    evalPartitioning.emplace_back(ndev, SIM_SIZE_T_MAX, SIM_SIZE_T_MAX);
}

// Evaluation across cores. Assumes 
// - all thread structures are in place
// - structure merging is done by the caller
bool Circuit::evalPartitions(CommonData& commons, EvalSetup* evalSetup, bool usingErrorStack) {
    auto nDev = devices.size();
    auto nPart = evalPartitioning.size()-1;
    // Shared failure flag, relaxed ordering suffices (only a hint to stop early)
    std::atomic<bool> failed {false};
    // Use -n threads (per-thread structures are sized for this count).
    // Not set globally via omp_set_num_threads(), that one is reserved for OpenBLAS (-b).
    int nThreads = Simulator::nCpu();
    #pragma omp parallel for schedule(dynamic, 1) num_threads(nThreads)
    for(decltype(nPart) i=0; i<nPart; i++) {
        // After a failure skip remaining batches (no break allowed in omp for)
        if (failed.load(std::memory_order_relaxed)) {
            continue;
        }
        auto from = evalPartitioning[i];
        auto to = evalPartitioning[i+1];
        auto dev1 = from.device;
        auto dev2 = to.device;
        // Each time we process a batch in a thread we mark that thread as used
        evalSetup->markThreadUsed();
        // Per-thread error consumer
        auto thread = cpuIndex();
        ErrorConsumer threadErrors = usingErrorStack ? ErrorConsumer(threadErrorStack[thread]) : ErrorConsumer(threadStatus[thread]);
        for(size_t iDev=dev1; !failed.load(std::memory_order_relaxed) && iDev<nDev && iDev<=dev2; iDev++) {
            auto fromEv = (iDev==dev1) ? Device::EvalPtr(from.model, from.instance) : Device::EvalPtr::begin;
            auto toEv = (iDev==dev2) ? Device::EvalPtr(to.model, to.instance) : Device::EvalPtr::end;
            auto* devPtr = devices[iDev].get();
            // Thread-private timepoint
            Accounting::Timepoint tdDev;
            if constexpr(devacct) {
                tdDev = Accounting::wclk();
            }
            if (!devPtr->evalAndLoad(*this, commons, evalSetup, nullptr, threadErrors, fromEv, toEv)) {
                failed.store(true, std::memory_order_relaxed);
            }
            if constexpr(devacct) {
                threadDevEvalLoadTimes[thread*(nDev+1)+iDev] += Accounting::wclkDelta(tdDev);
                threadDevEvalLoadCalls[thread*(nDev+1)+iDev]++;
            }
        }
    }
    return !failed.load();
}

}