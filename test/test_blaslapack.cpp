#include "blaslapack.h"
#include "common.h"
#include <complex>
#include <iostream>
#include <cmath>

using namespace sim;

// zdotc_ is the only routine declared in blaslapack.h that returns a value
// of Fortran type DOUBLE COMPLEX (NAMESPACE::Complex here). That matters
// because two different ABI conventions exist for returning such a value:
// gfortran (what this codebase assumes) returns it C-style, by ordinary
// value (the XMM0:XMM1 register pair on x86-64 SysV); f2c/g77-era compilers
// (g77, clapack) instead pass a hidden pointer as the function's first
// argument and write the result through it. blaslapack.h's zdotc_
// prototype has no such hidden pointer. If the linked BLAS were actually
// built the f2c way, this call would silently misinterpret every argument
// by one slot and return an untouched/garbage value instead of failing to
// link - see the header comment on this in include/blaslapack.h.
//
// This test cannot inspect the calling convention directly, so it checks
// the one thing observable from C++: that zdotc_'s returned value matches
// a conjugated dot product computed independently, in plain C++, for a
// vector chosen so the expected result is not accidentally zero (which a
// hidden-pointer mismatch would produce here, since the hidden pointer
// argument this build's prototype does not pass would leave zdotc_'s
// result buffer unwritten).
int main() {
    const int n = 3;
    Complex x[n] = { Complex(1.0, 2.0), Complex(-1.0, 1.0), Complex(0.0, 3.0) };
    Complex y[n] = { Complex(2.0, -1.0), Complex(3.0, 0.0), Complex(1.0, 1.0) };
    const int inc = 1;

    // Expected result, computed independently of zdotc_
    Complex expected(0.0, 0.0);
    for (int i = 0; i < n; i++) {
        expected += std::conj(x[i]) * y[i];
    }

    bool ok = true;

    if (std::abs(expected) < 1e-6) {
        std::cout << "Test vectors give a degenerate (near-zero) expected dot product - fix the test data\n";
        ok = false;
    }

    Complex actual = zdotc_(&n, x, &inc, y, &inc);

    double err = std::abs(actual - expected);
    std::cout << "zdotc_ expected " << expected << ", got " << actual << ", error " << err << "\n";
    if (err > 1e-9) {
        std::cout << "zdotc_ result does not match - the linked BLAS may be returning DOUBLE COMPLEX via a hidden pointer argument (f2c/g77 convention) instead of by value (gfortran convention); see include/blaslapack.h\n";
        ok = false;
    }

    std::cout << (ok ? "BLAS/LAPACK complex-return-value test OK\n" : "BLAS/LAPACK complex-return-value test FAILED\n");

    return ok ? 0 : 1;
}
