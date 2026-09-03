#include <complex>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <cstdlib>

#include "simulator.h"
#include "solsuperluitf.h"
#include "common.h"

namespace NAMESPACE {
namespace superlu_wrapper {

// Separate this from SuperLU backed because backend cannot include anything BLAS
int32_t threadCount() {
    return Simulator::nCpu();
}

void IEnvData::setBlockSize(UnknownIndex blockSize) {
    panelSize = blockSize;
    relax = 1*blockSize; // kxm, k is small
    maxSize = 2*blockSize; // lxm, >=default=200
};

static IEnvData defaultData;

IEnvData* IEnvData::activeData = &defaultData;

}

}

extern "C" {
// Uses a single global IEnvData pointer. This means you cannot use multiple 
// solvers each in its own thread. Just one solver at once. 
// TODO: use thread-local storage to make this work properly. 
int sp_ienv(int ispec) {
    switch (ispec) {
        case 1: return NAMESPACE::superlu_wrapper::IEnvData::activeData->panelSize; /* SUPERLU_PANEL_SIZE */
        case 2: return NAMESPACE::superlu_wrapper::IEnvData::activeData->relax;     /* SUPERLU_RELAX */
        case 3: return NAMESPACE::superlu_wrapper::IEnvData::activeData->maxSize;   /* Max supernode size */
        case 4: return NAMESPACE::superlu_wrapper::IEnvData::activeData->minRow;    /* Row permutation control */
        case 5: return NAMESPACE::superlu_wrapper::IEnvData::activeData->minCol;    /* Column permutation control */
        case 6: return NAMESPACE::superlu_wrapper::IEnvData::activeData->lSize;     /* Fill ratio for L (Values) */
        case 7: return NAMESPACE::superlu_wrapper::IEnvData::activeData->uSize;     /* Fill ratio for U (Values) */
        case 8: return NAMESPACE::superlu_wrapper::IEnvData::activeData->subsSize;  /* Subscript array size */            
        default: return 0;
    }
}

}

