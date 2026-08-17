#include "densematrix.h"
#include "value.h"
#include <iostream>

using namespace sim;

// Wraps DenseMatrix<T>::test() (lib/densematrix.cpp) for the double, Complex,
// and Int instantiations. Exit status 0 means all self-tests passed, 1 means
// at least one failed.
int main() {
    bool ok = true;

    std::cout << "== DenseMatrix<double> ==\n";
    ok = DenseMatrix<double>::test() && ok;

    std::cout << "== DenseMatrix<Complex> ==\n";
    ok = DenseMatrix<Complex>::test() && ok;

    std::cout << "== DenseMatrix<Int> ==\n";
    ok = DenseMatrix<Int>::test() && ok;

    std::cout << (ok ? "All DenseMatrix tests OK\n" : "DenseMatrix tests FAILED\n");

    return ok ? 0 : 1;
}
