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
    if (blockSize < 1) {
        return; // keep defaults
    }
    panelSize = blockSize;
    relax = blockSize; // kxm, k is small
    // smallest multiple of blockSize that is >= the sp_ienv default (200)
    maxSize = ((200 + blockSize - 1) / blockSize) * blockSize;
};

static IEnvData defaultData;

IEnvData* IEnvData::activeData = &defaultData;

}

}

extern "C" {
// Single global IEnvData pointer; one live factorization per value type at a
// time (SolverImpl::gluOwner enforces it). No concurrent factorizations.
// TODO: TLS + per-instance GlobalLU_t to lift both limits.
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

