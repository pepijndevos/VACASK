#ifndef __SOLSUPERLU_COMMON_DEFINED
#define __SOLSUPERLU_COMMON_DEFINED

// Shared body of the SuperLU_MT backend, included by solsuperlu_real.cpp and
// solsuperlu_complex.cpp *after* their <slu_mt_ddefs.h> / <slu_mt_zdefs.h>.
//
// Each backend TU supplies a SuperluBackend<SLUValue> specialization. 
//
// The CSC pattern and value arrays are NOT copied: A aliases the caller's
// CSCMatrix buffers directly (pdgstrf works on the column-permuted copy AC, so
// A stays pure input). CSCMatrix reallocates those buffers only in its own
// rebuild(), which is always paired with a solver rebuild() -> setPattern().

#if !defined(__SLU_MT_DDEFS) && !defined(__SLU_MT_ZDEFS)
#error "solsuperlu_common.h must be included after <slu_mt_ddefs.h> or <slu_mt_zdefs.h>"
#endif

#include <complex>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include "solsuperluitf.h"
#include "common.h"

namespace NAMESPACE {
namespace superlu_wrapper {

// A aliases CSCMatrix's index arrays as int_t*; that only works when the
// SuperLU_MT library was built with 32-bit int_t (i.e. without -D_LONGINT).
static_assert(sizeof(int_t) == sizeof(MatrixEntryIndex),
              "SuperLU_MT int_t width differs from MatrixEntryIndex (library "
              "built with -D_LONGINT?); VACASK aliases CSCMatrix index arrays directly.");

// Primary template left undefined; each backend TU provides one specialization
template<typename SLUValue> struct SuperluBackend;

template<typename SLUValue>
class SolverImpl {
    using B      = SuperluBackend<SLUValue>;
    using SluVal = typename B::SluVal;

    // A / B matrices alias the caller's value buffer as SluVal*.
    static_assert(sizeof(SLUValue) == sizeof(SluVal),
                  "SuperLU value type layout differs from the VACASK value type.");

    int_t               nprocs {1};
    superlumt_options_t options {};
    Gstat_t             stat {};
    SuperMatrix         A {};                 // SLU_NC view over the caller's CSC buffers
    SuperMatrix         L {}, U {};

    std::vector<int_t>  permC, permR;         // owned: get_perm_c / pivoting write here

    int_t n {0};
    int_t nnz {0};
    int_t panelSize {0};
    int_t relaxParam {0};

    bool haveA        {false};   // A store + Gstat allocated
    bool haveSymbolic {false};   // options.etree/colcnt_h/part_super_h allocated by gstrf_init
    bool luAllocated  {false};   // L / U hold a (possibly singular) factorization
    bool factored     {false};   // the last factorization succeeded

    // SuperLU_MT keeps factorization workspace in a per-value-type static
    // GlobalLU_t (p?gstrf_thread_init). A reusing factorization (refact=YES)
    // reads capacity fields left there by the last factorization of any
    // instance, so it is only valid while this instance is still that last one.
    static inline const SolverImpl* gluOwner {nullptr};

    void freeLU() {
        if (luAllocated) {
            Destroy_SuperNode_SCP(&L);
            Destroy_CompCol_NCP(&U);
            luAllocated = false;
        }
        factored = false;
    }

    void freeSymbolic() {
        if (haveSymbolic) {
            superlu_free(options.etree);
            superlu_free(options.colcnt_h);
            superlu_free(options.part_super_h);
            options.etree = options.colcnt_h = options.part_super_h = nullptr;
            haveSymbolic = false;
        }
    }

public:
    ~SolverImpl() { clearAll(); }

    void freeFactor() { freeLU(); }

    void clearAll() {
        freeLU();
        freeSymbolic();
        if (gluOwner == this) {
            gluOwner = nullptr;
        }
        if (haveA) {
            StatFree(&stat);
            Destroy_SuperMatrix_Store(&A);   // frees the Store struct only; arrays are the caller's
            haveA = false;
        }
        permC.clear();
        permR.clear();
        n = nnz = 0;
    }

    // cp / ri / nz point at the caller's CSC buffers and must stay valid (see the
    // file header) until the next setPattern or clearAll. Values in nz may change
    // freely between factor() calls; the pattern must not.
    void setPattern(int n_, int nnz_, const MatrixEntryIndex* cp,
                    const MatrixEntryIndex* ri, const void* nz) {
        clearAll();
        nprocs = threadCount();
        n   = n_;
        nnz = nnz_;

        permC.assign(n_, 0);
        permR.assign(n_, 0);

        B::createCompCol(
            &A, n, nnz,
            static_cast<SluVal*>(const_cast<void*>(nz)),
            const_cast<MatrixEntryIndex*>(ri),
            const_cast<MatrixEntryIndex*>(cp)
        );

        panelSize  = sp_ienv(1);
        relaxParam = sp_ienv(2);

        StatAlloc(n, nprocs, panelSize, relaxParam, &stat);
        StatInit(n, nprocs, &stat);

        // Fill-reducing column ordering (3 = approximate minimum degree for
        // unsymmetric matrices, i.e. COLAMD). Computed once, reused every factor.
        get_perm_c(3, &A, permC.data());

        options = superlumt_options_t{};
        options.perm_c = permC.data();
        options.perm_r = permR.data();

        haveA = true;
    }

    int factor(int fact) {
        if (!haveA) {
            return -1;
        }
        // A already aliases the caller's live value buffer - nothing to copy.

        // fact != 0 asks to reuse the column ordering / symbolic structure, but
        // only if a prior factorization built it.
        yes_no_t refact = (fact != 0 && haveSymbolic) ? YES : NO;

        if (refact == YES && gluOwner != this) {
            throw std::logic_error(
                "SuperLU_MT: this solver's factorization workspace was taken "
                "over by another solver instance of the same value type; "
                "overlapping in-use solver lifetimes are not supported."
            );
        }

        if (refact == NO) {
            freeLU();
            freeSymbolic();     // gstrf_init(refact=NO) allocates it afresh
        }

        StatInit(n, nprocs, &stat);

        SuperMatrix AC {};
        B::gstrfInit(
            nprocs, DOFACT, NOTRANS, refact, panelSize, relaxParam,
            /*diag_pivot_thresh=*/1.0, /*usepr=*/NO, /*drop_tol=*/0.0,
            permC.data(), permR.data(), /*work=*/nullptr, /*lwork=*/0,
            &A, &AC, &options, &stat
        );
        haveSymbolic = true;
        gluOwner = this;        // this instance now owns the static GlobalLU_t

        int_t info = 0;
        B::gstrf(&options, &AC, permR.data(), &L, &U, &stat, &info);

        Destroy_CompCol_Permuted(&AC);

        if (info == 0) {
            luAllocated = true;
            factored    = true;
        } else if (info > 0 && info <= n) {
            luAllocated = true;    // singular, but L / U are complete and destroyable
            factored    = false;
        } else {
            luAllocated = false;   // argument error or out of memory
            factored    = false;
        }
        return info;
    }

    int solve(void* rhs, int nrhs, int trans) {
        if (!factored) {
            return -1;
        }
        SuperMatrix Bmat {};
        B::createDense(&Bmat, n, static_cast<int_t>(nrhs), reinterpret_cast<SluVal*>(rhs), n);

        int_t info = 0;
        B::gstrs(trans ? TRANS : NOTRANS, &L, &U, permR.data(), permC.data(), &Bmat, &stat, &info);

        Destroy_SuperMatrix_Store(&Bmat);
        return info;
    }

    // 1-norm reciprocal condition estimate: ||A||_1 from langs over the aliased
    // value buffer, then gscon (Hager/Higham) over the stored L / U factors.
    int rcond(double* rc) {
        *rc = 0.0;
        if (!factored) {
            return -1;
        }
        char norm[] = "1";
        double anorm = B::langs(norm, &A);

        int_t info = 0;
        B::gscon(norm, &L, &U, anorm, rc, &info);
        return info;
    }
};

// Opaque-handle boundary. Declared (without definitions) in include/solsuperlu.h
// so the rest of VACASK can call these; defined here and explicitly instantiated
// per value type by each backend TU via VACASK_SUPERLU_INSTANTIATE.
template<typename SLUValue> SolverImpl<SLUValue>* create()  { return new SolverImpl<SLUValue>(); }
template<typename SLUValue> void               destroy(SolverImpl<SLUValue>* s) { delete s; }

template<typename SLUValue>
void setPattern(
    SolverImpl<SLUValue>* s, int n, int nnz,
    const MatrixEntryIndex* colptr, const MatrixEntryIndex* rowind,
    const SLUValue* nzval
) {
    s->setPattern(n, nnz, colptr, rowind, nzval);
}

template<typename SLUValue>
int factor(SolverImpl<SLUValue>* s, int fact) {
    return s->factor(fact);
}

template<typename SLUValue>
int solve(SolverImpl<SLUValue>* s, SLUValue* B, int nrhs, int trans) {
    return s->solve(B, nrhs, trans);
}

template<typename SLUValue>
int rcond(SolverImpl<SLUValue>* s, double* rc) {
    return s->rcond(rc);
}

template<typename SLUValue> void freeFactor(SolverImpl<SLUValue>* s) { s->freeFactor(); }
template<typename SLUValue> void clear(SolverImpl<SLUValue>* s)      { s->clearAll(); }

// Instantiates the class and the eight entry points.
#define VACASK_SUPERLU_INSTANTIATE(SLUValue)                                \
    template class SolverImpl<SLUValue>;                                    \
    template SolverImpl<SLUValue>* create<SLUValue>();                         \
    template void destroy<SLUValue>(SolverImpl<SLUValue>*);                    \
    template void setPattern<SLUValue>(                                     \
        SolverImpl<SLUValue>*, int, int,                                    \
        const MatrixEntryIndex*, const MatrixEntryIndex*, const SLUValue*   \
    );                                                                   \
    template int  factor<SLUValue>(SolverImpl<SLUValue>*, int);               \
    template int  solve<SLUValue>(SolverImpl<SLUValue>*, SLUValue*, int, int);    \
    template int  rcond<SLUValue>(SolverImpl<SLUValue>*, double*);             \
    template void freeFactor<SLUValue>(SolverImpl<SLUValue>*);                 \
    template void clear<SLUValue>(SolverImpl<SLUValue>*)

}
}

#endif
