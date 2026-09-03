#ifndef __SOLSUPERLU_DEFINED
#define __SOLSUPERLU_DEFINED

#include <cstdint>
#include <tuple>
#include <stdexcept>
#include <type_traits>

#include "simulator.h"
#include "solver.h"
#include "solsuperluitf.h"
#include "common.h"


// SuperLU_MT's headers (slu_mt_ddefs.h / slu_mt_zdefs.h) redeclare the raw
// Fortran BLAS symbols (dgemm_, dtrsv_, ...) with signatures that conflict with
// blaslapack.h, and define generic-named structs (GlobalLU_t, ...) incompatibly
// between the real and complex variants. Everything that touches SuperLU_MT is
// therefore kept in the isolated backend TUs lib/solsuperlu_real.cpp and
// lib/solsuperlu_complex.cpp (shared body in lib/solsuperlu_common.h); this
// header sees only the opaque handle and entry points below.

namespace NAMESPACE {

namespace superlu_wrapper {

// Opaque per-value-type solver handle
template<typename SLUValue> class SolverImpl;

// Allocate / free a solver instance
template<typename SLUValue> SolverImpl<SLUValue>* create();
template<typename SLUValue> void               destroy(SolverImpl<SLUValue>*);

// Install / replace the CSC sparsity pattern.
// colptr / rowind / nzval are the caller's CSCMatrix buffers. 
template<typename SLUValue>
void setPattern(
    SolverImpl<SLUValue>*, int n, int nnz,
    const MatrixEntryIndex* colptr, const MatrixEntryIndex* rowind,
    const SLUValue* nzval
);

// Numeric factorization (threshold partial pivoting) of the values currently in
// the buffer handed to setPattern. fact: 0 = first factorization, 1/2 = reuse
// the column ordering and symbolic structure. Return value is the pdgstrf info:
//   0        success
//   < 0      argument -info was rejected
//   1..n     U(info,info) is exactly zero (zero-pivot column is info-1)
//   > n      out of memory after (info - n) bytes
template<typename SLUValue> int factor(SolverImpl<SLUValue>*, int fact);

// Triangular solves against the stored factorization. B is column-major with
// leading dimension n and is overwritten with the solution. trans: 0 = A,
// 1 = A^T. Returns 0 on success, a nonzero info code otherwise.
template<typename SLUValue> int solve(SolverImpl<SLUValue>*, SLUValue* B, int nrhs, int trans);

// Reciprocal condition estimate (dlangs + dgscon / zlangs + zgscon) 
// Returns the gscon info: 0 = ok, < 0 = argument -info rejected. 
template<typename SLUValue> int rcond(SolverImpl<SLUValue>*, double* rc);

// Drop the factorization but keep the pattern and the column ordering.
template<typename SLUValue> void freeFactor(SolverImpl<SLUValue>*);

// Drop the factorization and the pattern.
template<typename SLUValue> void clear(SolverImpl<SLUValue>*);

}


SIMPLE_ERRORCLASS(SuperLUEnvError, "Failed to initialize the SuperLU_MT solver.");

SIMPLE_ERRORCLASS(SuperLUSolveError, "Failed to solve factorized system.");

// info < 0: argument -info of the SuperLU_MT driver had an illegal value.
ERRORCLASS(SuperLUArgumentError)
    int argument;
    SuperLUArgumentError(int argument) : argument(argument) {}
    std::string format() const {
        return "SuperLU_MT driver rejected argument " + std::to_string(argument) + ".";
    }
END_ERRORCLASS(SuperLUArgumentError);

// info > n: memory allocation failed after (info - n) bytes.
ERRORCLASS(SuperLUMemoryError)
    std::size_t bytes;
    SuperLUMemoryError(std::size_t bytes) : bytes(bytes) {}
    std::string format() const {
        return "SuperLU_MT ran out of memory after " + std::to_string(bytes) + " bytes.";
    }
END_ERRORCLASS(SuperLUMemoryError);

ERRORCLASS(SuperLUFactorizationError)
    MatrixEntryIndex size;
    MatrixEntryIndex column;
    Id node;
    SuperLUFactorizationError(MatrixEntryIndex size, MatrixEntryIndex column, Id node)
        : size(size), column(column), node(node) {}
    std::string format() const {
        std::string txt = "Factorization failed, size=" + std::to_string(size);
        if (node) {
            txt += ", zero pivot @ node '" + std::string(node) + "'";
        } else {
            txt += ", zero pivot @ column " + std::to_string(column + 1);
        }
        return txt + ".";
    }
END_ERRORCLASS(SuperLUFactorizationError);

ERRORCLASS(SuperLURefactorizationError)
    MatrixEntryIndex size;
    MatrixEntryIndex column;
    SuperLURefactorizationError(MatrixEntryIndex size, MatrixEntryIndex column)
        : size(size), column(column) {}
    std::string format() const {
        return "Refactorization failed, size=" + std::to_string(size) +
               ", zero pivot @ column " + std::to_string(column + 1) + ".";
    }
END_ERRORCLASS(SuperLURefactorizationError);


// SuperLU_MT-backed sparse direct solver (multithreaded)
//
//   setPattern() -> CSC pattern + fill-reducing column ordering (get_perm_c)
//   factor()     -> full numeric LU  (pdgstrf_init refact=NO, then pdgstrf)
//   refactor()   -> numeric LU reusing the column ordering / symbolic structure
//                   (pdgstrf_init refact=YES); still re-pivots
//   solve()      -> triangular solves (dgstrs)
//   rcond()      -> 1-norm reciprocal condition estimate (dlangs + dgscon)
template<typename IndexType, typename ValueType>
class SuperLUMTLinearSparseSolver : public LinearSparseSolver<IndexType, ValueType> {
public:
    using Base   = LinearSparseSolver<IndexType, ValueType>;
    using Matrix = typename Base::Matrix;

    static constexpr bool complexValue = std::is_same<ValueType, Complex>::value;

    static_assert(
        std::is_same<ValueType, double>::value || std::is_same<ValueType, Complex>::value,
        "SuperLUMTLinearSparseSolver value type is neither double nor std::complex<double>."
    );
    static_assert(
        std::is_same<IndexType, std::int32_t>::value,
        "SuperLUMTLinearSparseSolver index type must be int32_t (SuperLU_MT int_t)."
    );

    // Name this solver is registered under.
    static inline const Id solverId = Id::createStatic("superlu");

    // No SuperLU state is created here; impl_ is allocated lazily in rebuild().
    explicit SuperLUMTLinearSparseSolver(Matrix& matrix)
        : Base(matrix) {}

    ~SuperLUMTLinearSparseSolver() override {
        superlu_wrapper::IEnvData::activeData = &iEnv;

        if (impl_) {
            superlu_wrapper::destroy(impl_);
        }
    }

    static Base* create(Matrix& matrix) {
        return new SuperLUMTLinearSparseSolver(matrix);
    }

    bool isBuilt() const override { return built_; }
    bool isFactored() const override { return factored_; }

    void clear() override {
        superlu_wrapper::IEnvData::activeData = &iEnv;

        if (impl_) {
            superlu_wrapper::clear(impl_);
        }
        built_        = false;
        factored_     = false;
        factExecuted_ = false;
    }

    virtual void setBlockSize(UnknownIndex blockSize) override { iEnv.setBlockSize(blockSize); };

    bool rebuild(ErrorConsumer& ec) override {
        superlu_wrapper::IEnvData::activeData = &iEnv;

        if (!impl_) {
            impl_ = superlu_wrapper::create<ValueType>();
        }
        if (!impl_) {
            ec.push(SuperLUEnvError{});
            return false;
        }
        clear();

        auto& m = this->matrix();
        superlu_wrapper::setPattern(
            impl_,
            static_cast<int>(m.nRow()), static_cast<int>(m.nnz()),
            m.apData(), m.aiData(), m.axData()
        );
        built_ = true;
        return true;
    }

    bool factor(ErrorConsumer& ec) override {
        superlu_wrapper::IEnvData::activeData = &iEnv;

        if (!built_ && !rebuild(ec)) {
            return false;
        }

        auto* acct = this->matrix().accounting();
        auto t0 = Accounting::wclk();
        if (acct) {
            if constexpr (complexValue) { acct->acctNew.cxfactor++; }
            else { acct->acctNew.factor++; }
        }

        int info = superlu_wrapper::factor(impl_, factExecuted_ ? 1 : 0);
        bool ok = checkInfo(ec, info, false);

        if (acct) {
            if constexpr (complexValue) { acct->acctNew.tcxfactor += Accounting::wclkDelta(t0); }
            else { acct->acctNew.tfactor += Accounting::wclkDelta(t0); }
        }
        return ok;
    }

    bool refactor(ErrorConsumer& ec) override {
        superlu_wrapper::IEnvData::activeData = &iEnv;

        if (!factored_) {
            return factor(ec);
        }

        auto* acct = this->matrix().accounting();
        auto t0 = Accounting::wclk();
        if (acct) {
            if constexpr (complexValue) { acct->acctNew.cxrefactor++; }
            else { acct->acctNew.refactor++; }
        }

        int info = superlu_wrapper::factor(impl_, 2);
        bool ok = checkInfo(ec, info, true);

        if (acct) {
            if constexpr (complexValue) { acct->acctNew.tcxrefactor += Accounting::wclkDelta(t0); }
            else { acct->acctNew.trefactor += Accounting::wclkDelta(t0); }
        }
        return ok;
    }

    // LAPACK-style 1-norm reciprocal condition estimate over the current factors.
    std::tuple<bool, double> rcond(ErrorConsumer& ec) override {
        superlu_wrapper::IEnvData::activeData = &iEnv;

        if (!factored_) {
            return { false, 0.0 };
        }
        double rc = 0.0;
        int info = superlu_wrapper::rcond(impl_, &rc);
        if (info < 0) {
            ec.push(SuperLUArgumentError{ -info });
            return { false, 0.0 };
        }
        return { true, rc };
    }

    bool solve(ValueType* rhs, ErrorConsumer& ec) override {
        return runSolve<false>(rhs, 1, ec);
    }

    bool solve(ValueType* B, IndexType nrhs, ErrorConsumer& ec) override {
        return runSolve<false>(B, nrhs, ec);
    }

    bool tsolve(ValueType* rhs, ErrorConsumer& ec) override {
        return runSolve<true>(rhs, 1, ec);
    }

    bool tsolve(ValueType* B, IndexType nrhs, ErrorConsumer& ec) override {
        return runSolve<true>(B, nrhs, ec);
    }

protected:
    superlu_wrapper::SolverImpl<ValueType>* impl_ { nullptr };

    bool built_        { false };
    bool factored_     { false };
    bool factExecuted_ { false };   // a factorization has populated L / U

    bool checkInfo(ErrorConsumer& ec, int info, bool isRefactor) {
        auto n = static_cast<MatrixEntryIndex>(this->matrix().nRow());
        if (info == 0) {
            factExecuted_ = true;
            factored_     = true;
            return true;
        }
        if (info < 0) {
            ec.push(SuperLUArgumentError{ -info });
            return false;
        }
        if (info <= static_cast<int>(n)) {
            auto col = static_cast<MatrixEntryIndex>(info - 1);
            if (isRefactor) {
                ec.push(SuperLURefactorizationError{ n, col });
            } else {
                auto* resolver = this->matrix().resolver();
                ec.push(SuperLUFactorizationError{ n, col, resolver ? (*resolver)(col) : Id() });
            }
        } else {
            ec.push(SuperLUMemoryError{ static_cast<std::size_t>(info - static_cast<int>(n)) });
        }
        superlu_wrapper::freeFactor(impl_);
        factExecuted_ = false;
        factored_     = false;
        return false;
    }

    template<bool Transpose>
    bool runSolve(ValueType* B, IndexType nrhs, ErrorConsumer& ec) {
        superlu_wrapper::IEnvData::activeData = &iEnv;

        if (!factored_) {
            throw std::logic_error(
                std::string("SuperLUMTLinearSparseSolver::") + (Transpose ? "tsolve" : "solve") +
                ": matrix is not factored."
            );
        }

        auto* acct = this->matrix().accounting();
        auto t0 = Accounting::wclk();
        if (acct) {
            if constexpr (complexValue) { acct->acctNew.cxsolve += nrhs; }
            else { acct->acctNew.solve += nrhs; }
        }

        int info = superlu_wrapper::solve(impl_, B, static_cast<int>(nrhs), Transpose ? 1 : 0);

        if (acct) {
            if constexpr (complexValue) { acct->acctNew.tcxsolve += Accounting::wclkDelta(t0); }
            else { acct->acctNew.tsolve += Accounting::wclkDelta(t0); }
        }

        if (info != 0) {
            ec.push(SuperLUSolveError{});
            return false;
        }
        return true;
    }

private:
    superlu_wrapper::IEnvData iEnv;
};


typedef SuperLUMTLinearSparseSolver<MatrixEntryIndex, double>  SuperLURealSparseSolver;
typedef SuperLUMTLinearSparseSolver<MatrixEntryIndex, Complex> SuperLUComplexSparseSolver;

}

#endif
