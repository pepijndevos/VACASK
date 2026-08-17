#include "coretrancoef.h"
#include <iostream>

using namespace sim;

// Wraps IntegratorCoeffs::test() (lib/coretrancoef.cpp). Exit status 0
// means the self-test passed, 1 means it failed.
int main() {
    bool ok = IntegratorCoeffs::test();

    std::cout << (ok ? "All IntegratorCoeffs tests OK\n" : "IntegratorCoeffs tests FAILED\n");

    return ok ? 0 : 1;
}
