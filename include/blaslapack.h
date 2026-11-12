#ifndef __BLASLAPACK_DEFINED
#define __BLASLAPACK_DEFINED

#include "common.h"

// No system C header exists for these (cblas.h/lapacke.h are wrapper APIs
// with different names), so we declare the raw Fortran-linkage routines
// ourselves, as Eigen does (Eigen/src/misc/blas.h/lapack.h). gfortran ABI:
// lowercase name + trailing underscore, args by pointer. DOUBLE COMPLEX
// function results (zdotc_) are returned by value in XMM0:XMM1 on x86-64
// SysV (a 16-byte all-SSE-class aggregate is returned in registers, not via
// a hidden pointer arg - that convention is f2c/g77-era, not gfortran's
// default here; confirmed empirically against this system's reference BLAS,
// which silently returns a zero/untouched result if called the other way).
// Consequence: the linked BLAS/LAPACK must be built with gfortran (or
// another compiler using gfortran's modern COMPLEX-return convention), not
// f2c or a f2c-derived toolchain (e.g. g77, clapack) - those return DOUBLE
// COMPLEX via the hidden-pointer convention zdotc_'s prototype here does
// NOT use, and would silently produce wrong (zero/garbage) results instead
// of a link or runtime error.
//
// Swapping in OpenBLAS needs no changes here - it's ABI/symbol-compatible
// with reference BLAS/LAPACK (same sonames, names, signatures). Only
// CMakeLists.txt changes, plus an explicit omp_set_num_threads() at startup
// to cap its thread count.
// WebAssembly requires a call's signature to match the callee's exactly (a
// mismatch fails at link time or is replaced by a trap), unlike native targets
// where a stray return value is harmless. The OpenBLAS built for WebAssembly
// (ci/build-wasm-deps.sh) defines every BLAS/LAPACK subroutine as returning int,
// so declare them that way there.
#ifdef __EMSCRIPTEN__
#define BLASLAPACK_VOID int
#else
#define BLASLAPACK_VOID void
#endif

extern "C" {
    // ---- BLAS ----

    // y := x (copy n elements of x, stride incx, into y, stride incy)
    BLASLAPACK_VOID dcopy_(const int* n, const double* x, const int* incx, double* y, const int* incy);
    BLASLAPACK_VOID zcopy_(const int* n, const NAMESPACE::Complex* x, const int* incx, NAMESPACE::Complex* y, const int* incy);

    // Euclidean (2-)norm of x (n elements, stride incx), computed with internal scaling
    // to avoid intermediate overflow/underflow.
    double dnrm2_(const int* n, const double* x, const int* incx);
    double dznrm2_(const int* n, const NAMESPACE::Complex* x, const int* incx);

    // Dot product of x and y (n elements each, strides incx/incy)
    double ddot_(const int* n, const double* x, const int* incx, const double* y, const int* incy);
    // Conjugated dot product: returns sum(conj(x_i) * y_i). Conjugates x (the first
    // vector), not y. Returned by value (see file header comment - not a hidden
    // pointer arg).
    // Clang warns (-Wreturn-type-c-linkage, an error under pyodide-build) about
    // a C++ class returned from an extern "C" function; this is intentional and
    // is the gfortran/clang by-value convention described above.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#endif
    NAMESPACE::Complex zdotc_(const int* n, const NAMESPACE::Complex* x, const int* incx, const NAMESPACE::Complex* y, const int* incy);
#ifdef __clang__
#pragma clang diagnostic pop
#endif

    // y := alpha*x + y (n elements, strides incx/incy)
    BLASLAPACK_VOID daxpy_(const int* n, const double* alpha, const double* x, const int* incx, double* y, const int* incy);
    BLASLAPACK_VOID zaxpy_(const int* n, const NAMESPACE::Complex* alpha, const NAMESPACE::Complex* x, const int* incx, NAMESPACE::Complex* y, const int* incy);

    // Interchange x and y elementwise (n elements, strides incx/incy)
    BLASLAPACK_VOID dswap_(const int* n, double* x, const int* incx, double* y, const int* incy);
    BLASLAPACK_VOID zswap_(const int* n, NAMESPACE::Complex* x, const int* incx, NAMESPACE::Complex* y, const int* incy);

    // x := alpha*x (n elements, stride incx)
    BLASLAPACK_VOID dscal_(const int* n, const double* alpha, double* x, const int* incx);
    BLASLAPACK_VOID zscal_(const int* n, const NAMESPACE::Complex* alpha, NAMESPACE::Complex* x, const int* incx);
    // x := alpha*x (n elements, stride incx), alpha real - cheaper than zscal_ for a
    // real scale factor since it avoids complex-complex multiplication.
    BLASLAPACK_VOID zdscal_(const int* n, const double* alpha, NAMESPACE::Complex* x, const int* incx);

    // 1-based index of the element with the largest |x_i| (n elements, stride incx)
    int idamax_(const int* n, const double* x, const int* incx);

    // ---- LAPACK ----

    // Set the M x N matrix A (leading dimension lda): off-diagonal entries := alpha,
    // diagonal entries := beta (uplo selects which part to touch; 'A'/anything else = all
    // of it). Used here with m=1 to fill a length-n strided vector (lda=stride) with a
    // constant, since column-major LDA is the only adjustable stride and only applies
    // across columns (m=1 makes the vector the single row).
    BLASLAPACK_VOID dlaset_(const char* uplo, const int* m, const int* n, const double* alpha, const double* beta, double* a, const int* lda);
    BLASLAPACK_VOID zlaset_(const char* uplo, const int* m, const int* n, const NAMESPACE::Complex* alpha, const NAMESPACE::Complex* beta, NAMESPACE::Complex* a, const int* lda);

    // Copy all (uplo='A') or part of the M x N matrix A (leading dimension lda) into B
    // (leading dimension ldb), column-major. The 2D counterpart of dcopy_: unlike dcopy_'s
    // single stride per vector, this honors two independent leading dimensions, so it can
    // copy between two sub-blocks of different parent matrices directly.
    BLASLAPACK_VOID dlacpy_(const char* uplo, const int* m, const int* n, const double* a, const int* lda, double* b, const int* ldb);
    BLASLAPACK_VOID zlacpy_(const char* uplo, const int* m, const int* n, const NAMESPACE::Complex* a, const int* lda, NAMESPACE::Complex* b, const int* ldb);

    // Matrix norm of the M x N matrix A (leading dimension lda). norm='M' gives
    // max(abs(A(i,j))) - unlike izamax_, confirmed empirically that zlange_'s 'M'
    // norm uses the true complex modulus, not |Re|+|Im|, so (unlike
    // VectorView::maxAbs()'s deliberate avoidance of izamax_ for Complex) this is
    // safe to use for both double and Complex. work is unreferenced for norm='M'
    // (only needed for norm='I') - pass nullptr.
    double dlange_(const char* norm, const int* m, const int* n, const double* a, const int* lda, double* work);
    double zlange_(const char* norm, const int* m, const int* n, const NAMESPACE::Complex* a, const int* lda, double* work);

    // LU factorization with partial pivoting of the M x N matrix A (leading dimension
    // LDA, column-major: elements within a column are contiguous, LDA is the stride
    // between columns). A := P*L*U (unit-diagonal L below the diagonal, U on and above
    // it, stored in place of A). ipiv is 1-based: row i was interchanged with row
    // ipiv[i]. info==0 on success, info>0 (=k) means U(k,k) is exactly zero (singular,
    // factorization completed but unusable for solving), info<0 (=-k) means the k-th
    // argument had an illegal value (a bug in our call, not a numerical failure).
    BLASLAPACK_VOID dgetrf_(const int* m, const int* n, double* a, const int* lda, int* ipiv, int* info);
    BLASLAPACK_VOID zgetrf_(const int* m, const int* n, NAMESPACE::Complex* a, const int* lda, int* ipiv, int* info);

    // Solve A*X=B (trans='N') using the LU factorization from dgetrf_/zgetrf_ (A, lda,
    // ipiv, unchanged here) and its pivot vector. B (N x NRHS, leading dimension ldb,
    // column-major) holds the right-hand side(s) on entry and the solution on exit.
    // info==0 on success; info<0 (=-k) means the k-th argument had an illegal value (a
    // bug in our call). Unlike dgetrf_, there is no info>0 case here - singularity was
    // already reported by dgetrf_ when the factorization was computed.
    BLASLAPACK_VOID dgetrs_(const char* trans, const int* n, const int* nrhs, const double* a, const int* lda, const int* ipiv, double* b, const int* ldb, int* info);
    BLASLAPACK_VOID zgetrs_(const char* trans, const int* n, const int* nrhs, const NAMESPACE::Complex* a, const int* lda, const int* ipiv, NAMESPACE::Complex* b, const int* ldb, int* info);
}

#endif
