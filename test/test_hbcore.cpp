#include "corehb.h"
#include <iostream>

using namespace sim;

// Wraps HBCore::test() (lib/corehb.cpp). Exit status 0 means the self-test
// passed, 1 means it failed.
int main() {
    bool ok = HBCore::test();

    std::cout << (ok ? "All HBCore tests OK\n" : "HBCore tests FAILED\n");

    return ok ? 0 : 1;
}
