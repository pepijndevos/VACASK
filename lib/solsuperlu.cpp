#include <complex>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <cstdlib>

#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

namespace superlu_wrapper {

// Separate this from SuperLU backed because backend cannot include anything BLAS
int32_t threadCount() {
    return Simulator::nCpu();
}

}

}
