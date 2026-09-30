#include <algorithm>
#include "circuit.h"
#include "common.h"

namespace NAMESPACE {

void Circuit::partitionInstances(size_t n) {
    evalPartitioning.clear();
    if (n<=0) {
        return;
    }

    // Count all instances, skip Hierarchical device
    auto ndev = deviceCount();
    size_t total = primitiveInstanceCount();
    if (total==0) {
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

    // End sentinel, one past the last real instance
    evalPartitioning.emplace_back(ndev, 0, 0);
}

}