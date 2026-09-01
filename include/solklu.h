#ifndef __SOLKLU_DEFINED
#define __SOLKLU_DEFINED

#include <type_traits>
#include <stdexcept>
#include <tuple>

#include "solver.h"

namespace NAMESPACE {

// KLU-backed sparse direct solver.
//
// Reads the raw CSC arrays from the bound KluMatrixCore and owns its own
// klu_common / klu_symbolic / klu_numeric objects.
template<typename IndexType, typename ValueType>
class KluLinearSparseSolver : public LinearSparseSolver<IndexType, ValueType> {
public:
    using Base     = LinearSparseSolver<IndexType, ValueType>;
    using Matrix   = typename Base::Matrix;
    using Common   = typename std::conditional<std::is_same<int32_t, IndexType>::value, klu_common,   klu_l_common  >::type;
    using Symbolic = typename std::conditional<std::is_same<int32_t, IndexType>::value, klu_symbolic, klu_l_symbolic>::type;
    using Numeric  = typename std::conditional<std::is_same<int32_t, IndexType>::value, klu_numeric,  klu_l_numeric >::type;

    static constexpr bool int32Index = std::is_same<int32_t, IndexType>::value;
    static constexpr bool complexValue = std::is_same<ValueType, Complex>::value;

    static_assert(
        std::is_same<IndexType, int32_t>::value || std::is_same<IndexType, int64_t>::value,
        "KluLinearSparseSolver index type is neither int32_t nor int64_t."
    );
    static_assert(
        std::is_same<ValueType, double>::value || std::is_same<ValueType, Complex>::value,
        "KluLinearSparseSolver value type is neither double nor std::complex<double>."
    );

    // Name this solver is registered under.
    static inline const Id solverId = Id::createStatic("klu");

    explicit KluLinearSparseSolver(Matrix& matrix)
        : Base(matrix), common_{}, symbolic_(nullptr), numeric_(nullptr) {}

    ~KluLinearSparseSolver() override { clear(); }

    static Base* create(Matrix& matrix) {
        return new KluLinearSparseSolver(matrix);
    }

    bool isBuilt() const override { return symbolic_ != nullptr; }
    bool isFactored() const override { return numeric_ != nullptr; }

    void clear() override {
        freeNumeric();
        freeSymbolic();
    }

    bool rebuild(ErrorConsumer& ec) override {
        freeNumeric();
        freeSymbolic();

        if (!kluDefaults(&common_)) {
            ec.push(KluDefaultsError{});
            return false;
        }

        auto& m = this->matrix();
        symbolic_ = kluAnalyze(m.nRow(), m.apData(), m.aiData(), &common_);
        if (!symbolic_) {
            ec.push(KluAnalysisError{});
            return false;
        }
        return true;
    }

    bool factor(ErrorConsumer& ec) override {
        if (!symbolic_ && !rebuild(ec)) {
            return false;
        }

        auto* acct = this->matrix().accounting();
        auto t0 = Accounting::wclk();
        if (acct) {
            if constexpr (complexValue) { acct->acctNew.cxfactor++; }
            else { acct->acctNew.factor++; }
        }

        freeNumeric();

        numeric_ = kluFactor();

        auto factOk = checkFactorization(ec, false);

        if (acct) {
            if constexpr (complexValue) { acct->acctNew.tcxfactor += Accounting::wclkDelta(t0); }
            else { acct->acctNew.tfactor += Accounting::wclkDelta(t0); }
        }

        return factOk;
    }

    bool refactor(ErrorConsumer& ec) override {
        if (!numeric_) {
            return factor(ec);
        }

        auto* acct = this->matrix().accounting();
        auto t0 = Accounting::wclk();
        if (acct) {
            if constexpr (complexValue) { acct->acctNew.cxrefactor++; }
            else { acct->acctNew.refactor++; }
        }

        refactorStatus_ = kluRefactor();

        auto factOk = checkFactorization(ec, true);

        if (acct) {
            if constexpr (complexValue) { acct->acctNew.tcxrefactor += Accounting::wclkDelta(t0); }
            else { acct->acctNew.trefactor += Accounting::wclkDelta(t0); }
        }

        return factOk;
    }

    // Reciprocal condition number estimate. Same failure convention as rgrowth().
    std::tuple<bool, double> rcond(ErrorConsumer& ec) override {
        if (!numeric_) {
            return { false, 0.0 };
        }
        if (!kluRcond()) {
            ec.push(KluCondEstimateError{});
            return { false, 0.0 };
        }
        return { true, common_.rcond };
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
    Common     common_;
    Symbolic*  symbolic_;
    Numeric*   numeric_;
    int        refactorStatus_ { 1 };

    template<bool Transpose>
    bool runSolve(ValueType* B, IndexType nrhs, ErrorConsumer& ec) {
        if (!numeric_) {
            throw std::logic_error(
                std::string("KluLinearSparseSolver::") + (Transpose ? "tsolve" : "solve") + ": matrix is not factored."
            );
        }

        auto* acct = this->matrix().accounting();
        auto t0 = Accounting::wclk();
        if (acct) {
            if constexpr (complexValue) { acct->acctNew.cxsolve += nrhs; }
            else { acct->acctNew.solve += nrhs; }
        }

        int st;
        if constexpr (Transpose) {
            st = kluTsolve(symbolic_, numeric_, this->matrix().nRow(), nrhs, B, &common_);
        } else {
            st = kluSolve(symbolic_, numeric_, this->matrix().nRow(), nrhs, B, &common_);
        }

        if (acct) {
            if constexpr (complexValue) { acct->acctNew.tcxsolve += Accounting::wclkDelta(t0); }
            else { acct->acctNew.tsolve += Accounting::wclkDelta(t0); }
        }

        if (!st) {
            ec.push(KluSolveError{});
            return false;
        }
        return true;
    }

    // Common post-factorization status / rank check. On failure pushes the
    // appropriate error, frees the numeric object and returns false.
    bool checkFactorization(ErrorConsumer& ec, bool refactor) {
        auto n = this->matrix().nRow();
        bool isSingular = common_.status == KLU_SINGULAR;
        auto nr = common_.numerical_rank;
        bool ok = refactor ? (refactorStatus_ != 0) : (numeric_ != nullptr);

        if (!ok || isSingular || (nr >= 0 && nr != n)) {
            if (refactor) {
                ec.push(KluRefactorizationError{
                    static_cast<MatrixEntryIndex>(n),
                    static_cast<MatrixEntryIndex>(common_.numerical_rank)
                });
            } else {
                auto col = common_.singular_col;
                auto* resolver = this->matrix().resolver();
                ec.push(KluFactorizationError{
                    static_cast<MatrixEntryIndex>(n),
                    static_cast<MatrixEntryIndex>(common_.numerical_rank),
                    static_cast<MatrixEntryIndex>(col),
                    resolver ? (*resolver)(static_cast<MatrixEntryIndex>(col)) : Id()
                });
            }
            freeNumeric();
            return false;
        }
        return true;
    }

    //
    // KLU entry points, dispatched on IndexType / ValueType.
    //

    // Structural rank is computed by the symbolic analysis . Available once
    // the solver is built; -1 (reported as invalid) if BTF was not run.
    std::tuple<bool, IndexType> structuralRank() const {
        if (symbolic_) {
            auto r = common_.structural_rank;
            if (r>=0) {
                return { true, r };
            }
        }
        return { false, 0 };
    }

    // Numerical rank is computed during factorization; -1 (reported as invalid)
    // if it was not computed.
    std::tuple<bool, IndexType> numericalRank() const {
        if (numeric_) {
            auto r = common_.numerical_rank;
            if (r>=0) {
                return { true, r };
            }
        }
        return { false, 0 };
    }

    // Zero-pivot column, meaningful only when the last factorization was
    // singular (otherwise KLU reports the matrix order).
    std::tuple<bool, IndexType> singularColumn() const {
        if (numeric_) {
            auto c = common_.singular_col;
            if (c>=0 && c < this->matrix().nRow()) {
                return { true, c };
            }
        }
        return { false, 0 };
    }

    static int kluDefaults(Common* c) {
        if constexpr (int32Index) {
            return klu_defaults(c);
        } else {
            return klu_l_defaults(c);
        }
    }

    static Symbolic* kluAnalyze(IndexType n, IndexType* Ap, IndexType* Ai, Common* c) {
        if constexpr (int32Index) {
            return klu_analyze(n, Ap, Ai, c);
        } else {
            return klu_l_analyze(n, Ap, Ai, c);
        }
    }

    Numeric* kluFactor() {
        auto& m = this->matrix();
        auto* Ap = m.apData();
        auto* Ai = m.aiData();
        if constexpr (complexValue) {
            auto* Ax = reinterpret_cast<double*>(m.axData());
            if constexpr (int32Index) {
                return klu_z_factor(Ap, Ai, Ax, symbolic_, &common_);
            } else {
                return klu_zl_factor(Ap, Ai, Ax, symbolic_, &common_);
            }
        } else {
            auto* Ax = m.axData();
            if constexpr (int32Index) {
                return klu_factor(Ap, Ai, Ax, symbolic_, &common_);
            } else {
                return klu_l_factor(Ap, Ai, Ax, symbolic_, &common_);
            }
        }
    }

    int kluRefactor() {
        auto& m = this->matrix();
        auto* Ap = m.apData();
        auto* Ai = m.aiData();
        if constexpr (complexValue) {
            auto* Ax = reinterpret_cast<double*>(m.axData());
            if constexpr (int32Index) {
                return klu_z_refactor(Ap, Ai, Ax, symbolic_, numeric_, &common_);
            } else {
                return klu_zl_refactor(Ap, Ai, Ax, symbolic_, numeric_, &common_);
            }
        } else {
            auto* Ax = m.axData();
            if constexpr (int32Index) {
                return klu_refactor(Ap, Ai, Ax, symbolic_, numeric_, &common_);
            } else {
                return klu_l_refactor(Ap, Ai, Ax, symbolic_, numeric_, &common_);
            }
        }
    }

    int kluRgrowth() {
        auto& m = this->matrix();
        auto* Ap = m.apData();
        auto* Ai = m.aiData();
        if constexpr (complexValue) {
            auto* Ax = reinterpret_cast<double*>(m.axData());
            if constexpr (int32Index) {
                return klu_z_rgrowth(Ap, Ai, Ax, symbolic_, numeric_, &common_);
            } else {
                return klu_zl_rgrowth(Ap, Ai, Ax, symbolic_, numeric_, &common_);
            }
        } else {
            auto* Ax = m.axData();
            if constexpr (int32Index) {
                return klu_rgrowth(Ap, Ai, Ax, symbolic_, numeric_, &common_);
            } else {
                return klu_l_rgrowth(Ap, Ai, Ax, symbolic_, numeric_, &common_);
            }
        }
    }

    int kluRcond() {
        if constexpr (complexValue) {
            if constexpr (int32Index) {
                return klu_z_rcond(symbolic_, numeric_, &common_);
            } else {
                return klu_zl_rcond(symbolic_, numeric_, &common_);
            }
        } else {
            if constexpr (int32Index) {
                return klu_rcond(symbolic_, numeric_, &common_);
            } else {
                return klu_l_rcond(symbolic_, numeric_, &common_);
            }
        }
    }

    static int kluSolve(Symbolic* symbolic, Numeric* numeric, IndexType n,
                        IndexType nrhs, ValueType* B, Common* common) {
        if constexpr (complexValue) {
            auto* b = reinterpret_cast<double*>(B);
            if constexpr (int32Index) {
                return klu_z_solve(symbolic, numeric, n, nrhs, b, common);
            } else {
                return klu_zl_solve(symbolic, numeric, n, nrhs, b, common);
            }
        } else {
            if constexpr (int32Index) {
                return klu_solve(symbolic, numeric, n, nrhs, B, common);
            } else {
                return klu_l_solve(symbolic, numeric, n, nrhs, B, common);
            }
        }
    }

    static int kluTsolve(Symbolic* symbolic, Numeric* numeric, IndexType n,
                         IndexType nrhs, ValueType* B, Common* common) {
        if constexpr (complexValue) {
            auto* b = reinterpret_cast<double*>(B);
            if constexpr (int32Index) {
                return klu_z_tsolve(symbolic, numeric, n, nrhs, b, 0, common);
            } else {
                return klu_zl_tsolve(symbolic, numeric, n, nrhs, b, 0, common);
            }
        } else {
            if constexpr (int32Index) {
                return klu_tsolve(symbolic, numeric, n, nrhs, B, common);
            } else {
                return klu_l_tsolve(symbolic, numeric, n, nrhs, B, common);
            }
        }
    }

    void freeNumeric() {
        if (!numeric_) {
            return;
        }
        if constexpr (complexValue) {
            if constexpr (int32Index) {
                klu_z_free_numeric(&numeric_, &common_);
            } else {
                klu_zl_free_numeric(&numeric_, &common_);
            }
        } else {
            if constexpr (int32Index) {
                klu_free_numeric(&numeric_, &common_);
            } else {
                klu_l_free_numeric(&numeric_, &common_);
            }
        }
        numeric_ = nullptr;
    }

    void freeSymbolic() {
        if (!symbolic_) {
            return;
        }
        if constexpr (int32Index) {
            klu_free_symbolic(&symbolic_, &common_);
        } else {
            klu_l_free_symbolic(&symbolic_, &common_);
        }
        symbolic_ = nullptr;
    }
};


typedef KluLinearSparseSolver<MatrixEntryIndex, double>  KluRealSparseSolver;
typedef KluLinearSparseSolver<MatrixEntryIndex, Complex> KluComplexSparseSolver;

}

#endif
