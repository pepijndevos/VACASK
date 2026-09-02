// Complex backend for superlu
//
// Isolated translation unit: SuperLU_MT's headers redeclare the raw Fortran
// BLAS symbols with signatures that conflict with blaslapack.h, so only the
// clash-free common.h (via solsuperlu_common.h) is pulled in from VACASK.
//
// Compiled with -fopenmp -D__OPENMP

#include <slu_mt_zdefs.h>
#include <slu_dcomplex.h>

#include "solsuperlu_common.h"

// zlangs (matrix norm) lives in the library but is declared in no header;
// pzgssvx.c declares it locally the same way. zgscon is in slu_mt_zdefs.h.
extern "C" double zlangs(char*, SuperMatrix*);

namespace NAMESPACE {
namespace superlu_wrapper {

template<>
struct SuperluBackend<Complex> {
    using SluVal = doublecomplex;   // { double r, i; } - layout-compatible with Complex

    static void createCompCol(
        SuperMatrix* A, int_t n, int_t nnz,
        SluVal* nz, int_t* ri, int_t* cp
    ) {
        zCreate_CompCol_Matrix(A, n, n, nnz, nz, ri, cp, SLU_NC, SLU_Z, SLU_GE);
    }
    static void createDense(SuperMatrix* B, int_t n, int_t nrhs, SluVal* b, int_t ldb) {
        zCreate_Dense_Matrix(B, n, nrhs, b, ldb, SLU_DN, SLU_Z, SLU_GE);
    }
    static void gstrfInit(
        int_t nprocs, fact_t fact, trans_t trans, yes_no_t refact,
        int_t panel, int_t relax, double u, yes_no_t usepr, double droptol,
        int_t* pc, int_t* pr, void* work, int_t lwork,
        SuperMatrix* A, SuperMatrix* AC, superlumt_options_t* o, Gstat_t* g
    ) {
        pzgstrf_init(
            nprocs, fact, trans, refact, panel, relax, u, usepr, droptol,
            pc, pr, work, lwork, A, AC, o, g
        );
    }
    static void gstrf(
        superlumt_options_t* o, SuperMatrix* AC, int_t* pr,
        SuperMatrix* L, SuperMatrix* U, Gstat_t* g, int_t* info
    ) {
        pzgstrf(o, AC, pr, L, U, g, info);
    }
    static void gstrs(
        trans_t trans, SuperMatrix* L, SuperMatrix* U, int_t* pr, int_t* pc,
        SuperMatrix* B, Gstat_t* g, int_t* info
    ) {
        zgstrs(trans, L, U, pr, pc, B, g, info);
    }

    static double langs(char* norm, SuperMatrix* A) {
        return zlangs(norm, A);
    }
    
    static void gscon(
        char* norm, SuperMatrix* L, SuperMatrix* U,
        double anorm, double* rcond, int_t* info
    ) {
        zgscon(norm, L, U, anorm, rcond, info);
    }
};

VACASK_SUPERLU_INSTANTIATE(Complex);

}
}
