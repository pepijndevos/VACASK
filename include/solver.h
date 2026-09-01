#ifndef __SOLVER_DEFINED
#define __SOLVER_DEFINED

#include <tuple>
#include <unordered_map>

#include "cscmatrix.h"

namespace NAMESPACE {


// No solver creator is registered under the requested name.
ERRORCLASS(SolverNotFound)
    Id name;
    SolverNotFound(Id name) : name(name) {}
    std::string format() const {
        return "Unknown sparse solver '" + std::string(name) + "'.";
    }
END_ERRORCLASS(SolverNotFound);


// Abstract interface for a sparse direct linear solver.

template<typename IndexType, typename ValueType>
class LinearSparseSolver {
public:
    using Matrix = KluMatrixCore<IndexType, ValueType>;

    // Factory function: builds a concrete solver bound to the given matrix.
    typedef LinearSparseSolver* (*CreateFn)(Matrix& matrix);

    // Name of the default (KLU) solver.
    static inline const Id solverDefaultId = Id::createStatic("klu");

    // Registry of solver factories, keyed by solver name. Real and complex
    // solvers have separate registries (one per ValueType specialization).
    static std::unordered_map<Id, CreateFn>& getRegistry() {
        static std::unordered_map<Id, CreateFn> registry;
        return registry;
    };

    // Register solver type SolverType under the given name. 
    // Returns false if the name is already taken.
    template<typename SolverType>
    static bool registerSolver() {
        return getRegistry().insert({SolverType::solverId, &SolverType::create}).second;
    };

    // Look up a registered solver creator and build a solver bound to the given
    // matrix. Returns nullptr and pushes SolverNotFound if the name is unknown.
    static LinearSparseSolver* createSolver(Id name, Matrix& matrix, ErrorConsumer& ec) {
        if (!name) {
            name = solverDefaultId;
        }
        auto& registry = getRegistry();
        auto it = registry.find(name);
        if (it==registry.end()) {
            ec.push(SolverNotFound{name});
            return nullptr;
        }
        return it->second(matrix);
    };

    explicit LinearSparseSolver(Matrix& matrix) : matrix_(matrix) {}
    virtual ~LinearSparseSolver() = default;

    LinearSparseSolver           (const LinearSparseSolver&)  = delete;
    LinearSparseSolver           (      LinearSparseSolver&&) = delete;
    LinearSparseSolver& operator=(const LinearSparseSolver&)  = delete;
    LinearSparseSolver& operator=(      LinearSparseSolver&&) = delete;

    // The matrix this solver was bound to at construction.
    Matrix& matrix() const { return matrix_; }

    // Matrix order (number of unknowns).
    IndexType order() const { return matrix_.nRow(); }

    // (Re)build the solver from the matrix sparsity pattern
    virtual bool rebuild(ErrorConsumer& ec) = 0;

    // Build the numeric factorization from the current matrix values
    virtual bool factor(ErrorConsumer& ec) = 0;

    // Refresh the numeric factorization
    virtual bool refactor(ErrorConsumer& ec) = 0;

    // Return to state before rebuild(), i.e. unbuilt solver
    virtual void clear() = 0;

    virtual bool isBuilt() const = 0;
    virtual bool isFactored() const = 0;

    // Diagnostics (valid after a successful factor()/refactor())
    // Rank
    virtual std::tuple<bool, IndexType> structuralRank() const = 0;
    virtual std::tuple<bool, IndexType> numericalRank() const = 0;
    virtual std::tuple<bool, IndexType> singularColumn() const = 0;

    // Reciprocal pivot growth.
    virtual std::tuple<bool, double> rgrowth(ErrorConsumer& ec) = 0;

    // Reciprocal condition number estimate.
    virtual std::tuple<bool, double> rcond(ErrorConsumer& ec) = 0;

    // Solve a factored system
    // Solve A x = b for a single right-hand side.
    virtual bool solve(ValueType* rhs, ErrorConsumer& ec) = 0;

    // Solve A X = B for nrhs right-hand sides, B is column-major
    virtual bool solve(ValueType* B, IndexType nrhs, ErrorConsumer& ec) = 0;

    // Solve A^T x = b for a single right-hand side.
    virtual bool tsolve(ValueType* rhs, ErrorConsumer& ec) = 0;

    // Solve A^T X = B for nrhs right-hand sides, B is column-major.
    virtual bool tsolve(ValueType* B, IndexType nrhs, ErrorConsumer& ec) = 0;

protected:
    Matrix& matrix_;
};


template<typename ValueType>
using SparseSolver = LinearSparseSolver<MatrixEntryIndex, ValueType>;

typedef LinearSparseSolver<MatrixEntryIndex, double>  RealSparseSolver;
typedef LinearSparseSolver<MatrixEntryIndex, Complex> ComplexSparseSolver;

}

#endif
