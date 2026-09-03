#ifndef __SOLSUPERLUITF_DEFINED
#define __SOLSUPERLUITF_DEFINED

#include "common.h"

namespace NAMESPACE {
namespace superlu_wrapper {

// Number of threads pdgstrf spawns. Circuit Jacobians are usually small enough
// that 1 is fastest; override with VACASK_SUPERLU_NPROCS.
int threadCount();

typedef struct IEnvData {
    int panelSize {20}; 
    int relax {6};
    int maxSize {200};
    int minRow {200};
    int minCol {100};
    int lSize {-50};
    int uSize {-50};
    int subsSize {-30};

    void setBlockSize(UnknownIndex blockSize);

    static IEnvData* activeData;
} IEnvData;

}
}

#endif
