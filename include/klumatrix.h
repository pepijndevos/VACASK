#ifndef __KLUMATRIX_DEFINED
#define __KLUMATRIX_DEFINED

#include <suitesparse/klu.h>
#include <unordered_map>
#include <complex>
#include <type_traits>
#include <optional>
#include <memory>
#include "status.h"
#include "identifier.h"
#include "flags.h"
#include "hash.h"
#include "acct.h"
#include "ansupport.h"
#include "densematrix.h"
#include "errorstack.h"
#include "common.h"


namespace NAMESPACE {

// TODO: make multiple matrices share the same sparsity pattern without allocating a new copy
//       of KLU sparsity pattern

// Abstract class that resolves a row/column index into a name
// Should be defined by the user of the sparse matrix
class NameResolver {
public:
    NameResolver() {};
    virtual ~NameResolver() = default;

    // Indices in matrix are 0-based, operator() must take this into account
    virtual Id operator()(MatrixEntryIndex u) = 0;
};


// Index pair holding row and column position
typedef std::pair<EquationIndex,UnknownIndex> MatrixEntryPosition;


// Hash function for MatrixEntryPosition
 typedef struct MatrixEntryPositionHash {
    auto operator()(const MatrixEntryPosition& p) const -> size_t {
        if constexpr(sizeof(MatrixEntryPosition)<=sizeof(size_t)) {
            // Faster hash if entry coordinates are 32+32 bits on a machine with a 64-bit size_t
            return std::hash<size_t>{}(
                (static_cast<size_t>(p.first) << (sizeof(size_t)*8/2)) +
                p.second
            );
        } else {
            // Standard approach
            return hash_val(p.first, p.second);
        }
    }
} MatrixEntryPositionHash;

// Sparsity map entry flags
enum class EntryFlags : uint8_t {
    NoFlags = 0,
    Resistive = 1,
    Reactive = 2,
    ResistiveReactive = 3, 
    Delay = 4, 
    EntryType = 7, 
};
DEFINE_FLAG_OPERATORS(EntryFlags);

// Sparsity map - maps MatrixEntyPosition to an index in a linear array
// MatrixEntryPosition of a sparsity map entry uses 1-based indices
// Index 0 is reserved for the reference node (i.e. ground). 
class SparsityMap {
public:
    SparsityMap() {};

    SparsityMap           (const SparsityMap&)  = delete;
    SparsityMap           (      SparsityMap&&) = delete;
    SparsityMap& operator=(const SparsityMap&)  = delete;
    SparsityMap& operator=(      SparsityMap&&) = delete;

    struct Entry {
        MatrixEntryIndex index;
        EntryFlags flags { EntryFlags::NoFlags };
    };

    // Type of map from MatrixEntryPosition into a linear array index
    typedef std::unordered_map<MatrixEntryPosition, Entry, MatrixEntryPositionHash> Map;

    // Ordered entries type
    typedef std::tuple<MatrixEntryPosition, EntryFlags> OrderedEntry;

    // Clear
    void clear();
    
    // Size
    MatrixEntryIndex size() const { return smap.size(); };

    // Insert, the returned pointer points to an integer that 
    // will containt the entry index after enumerate() is called
    // Returns flag indicating new entry created, ok
    std::tuple<bool, bool> insert(EquationIndex e, UnknownIndex u, EntryFlags f=EntryFlags::NoFlags) {
        auto [it, inserted] = smap.insert({std::make_pair(e, u), { 0, f }});
        if (!inserted) {
            // Not inserted because it is already there, update flags
            it->second.flags = it->second.flags | f;
        }
        return std::make_tuple( inserted, true);
    };

    // Find, bool value indicates if an entry was found
    const Entry* find(const MatrixEntryPosition& mep) const {
        auto it = smap.find(mep);
        if (it==smap.end()) {
            return nullptr;
        }
        return &(it->second);
    };

    // Get map of entries
    const Map& sparsity() const { return smap; };

    // Get vector of sorted matrix entry positions
    std::vector<OrderedEntry>& positions() { return ordering; };
    const std::vector<OrderedEntry>& positions() const { return ordering; };

    // Enumerate entries
    void enumerate();

    // Dump map
    void dump(int indent, std::ostream& os) const;

private:
    Map smap;
    std::vector<OrderedEntry> ordering;
};


// Element component when element is complex
enum class Component { Real=1, Imaginary=2 };
DEFINE_FLAG_OPERATORS(Component);


// Matrix binding interface for accessing element indices and pointers
// Because Circuit::bind() should be matrix type agnostic we need this interface.
// Analyses can use different types of matrices, but instances must be able to
// handle them all in the same way via this interface.
// Assumes the underlying type of element is either double or std::complex<double> (Complex)
// This interface is used by the Device::bind() method to bind instances
// to matrix elements and their components.
//
// The bucket contract:
// When a requested position does not exist in the matrix, valuePtr()/cxValuePtr()
// return a pointer into a shared scratch area (the "bucket") instead of nullptr,
// so callers can write through the returned pointer unconditionally without a
// per-element "does this exist?" branch. Semantics of that pointer:
//   - writes are DISCARDED: many missing positions map onto the same bucket
//     storage and it is never read back into the solve, so a store just goes
//     nowhere meaningful (it is not an error).
//   - reads are MEANINGLESS: the bucket is not zeroed between loads and is
//     aliased by every missing position, so a load returns arbitrary leftover
//     data. Code that needs a real value must check the found flag from
//     valueIndex()/elementIndex() first.
// The bucket is sized so that offset-based loading (adding a bounded element
// offset to a resolved base pointer, see KluBlockSparseMatrixCore) stays in
// bounds even when the base resolved to the bucket.
template<typename IndexType> class MatrixAccess {
public:
    // Return array holding matrix nonzero elements
    // For complex matrices returns nullptr
    virtual double* valueArray() = 0;

    // Return array holding matrix nonzero elements
    // For real matrices returns nullptr
    virtual Complex* cxValueArray() = 0;

    // Return index into element array corresponding to row, column given by mep (1-based)
    // For block matrices, mep is the block position (1-based) and 
    // blockMep is the position of an element within the block (0-based). 
    // If blockMep is not given, (0,0) is assumed. 
    // Return value: index, found
    virtual std::tuple<IndexType, bool> valueIndex(const MatrixEntryPosition& mep, const std::optional<MatrixEntryPosition>& blockMep=std::nullopt) const = 0;

    // Return pointer to element's component
    // For block matrices, mep is the block position (1-based) and
    // blockMep is the position of the element within the block (0-based).
    // If blockMep is not given, (0,0) is assumed.
    // Returns a bucket pointer if the element is not found (see the bucket
    // contract above: writes discarded, reads meaningless).
    // Returns nullptr if imaginary part is requested from a real matrix
    virtual double* valuePtr(const MatrixEntryPosition& mep, Component comp=Component::Real, const std::optional<MatrixEntryPosition>& blockMep=std::nullopt) = 0;

    // Return pointer to element's component (complex matrix)
    // For block matrices, mep is the block position (1-based) and
    // blockMep is the position of the element within the block (0-based).
    // If blockMep is not given, (0,0) is assumed.
    // Returns a complex bucket pointer if the element is not found (see the
    // bucket contract above: writes discarded, reads meaningless).
    // Returns nullptr if matrix is real
    virtual Complex* cxValuePtr(const MatrixEntryPosition& mep, const std::optional<MatrixEntryPosition>& blockMep=std::nullopt) = 0;
};

//
// KLU matrix errors
//
// Row/column indices that identify an offending node are resolved to an Id
// (via a NameResolver) at the moment the error is created and stored in the
// error. format() prints the node name when the Id is valid, and falls back to
// the raw 1-based index when it is a bad Id (no resolver was available).
//

// -- data-free --

SIMPLE_ERRORCLASS(KluDefaultsError, "Cannot set up KLU defaults.");

SIMPLE_ERRORCLASS(KluAnalysisError, "KLU matrix analysis failed. Probably the matrix is singular.");

SIMPLE_ERRORCLASS(KluPivotGrowthError, "Failed to compute reciprocal pivot growth.");

SIMPLE_ERRORCLASS(KluCondEstimateError, "Failed to compute reciprocal condition number estimate.");

SIMPLE_ERRORCLASS(KluSolveError, "Failed to solve factorized system.");

SIMPLE_ERRORCLASS(KluMulVecSizeMismatch, "Matrix-vector multiplication vector size mismatch.");

// -- data-carrying --

ERRORCLASS(KluFactorizationError)
    MatrixEntryIndex size;      // matrix order
    MatrixEntryIndex rank;      // computed rank, < 0 if not available
    MatrixEntryIndex column;    // 0-based zero-pivot column
    Id node;                    // zero-pivot node, bad Id if unresolved
    KluFactorizationError(MatrixEntryIndex size, MatrixEntryIndex rank, MatrixEntryIndex column, Id node)
        : size(size), rank(rank), column(column), node(node) {}
    std::string format() const {
        std::string txt = "Factorization failed, size=" + std::to_string(size);
        if (rank >= 0) {
            txt += ", rank=" + std::to_string(rank);
        }
        if (node) {
            txt += ", zero pivot @ node '" + std::string(node) + "'";
        } else {
            txt += ", zero pivot @ column " + std::to_string(column + 1);
        }
        return txt + ".";
    }
END_ERRORCLASS(KluFactorizationError);

ERRORCLASS(KluRefactorizationError)
    MatrixEntryIndex size;      // matrix order
    MatrixEntryIndex rank;      // computed rank, < 0 if not available
    KluRefactorizationError(MatrixEntryIndex size, MatrixEntryIndex rank)
        : size(size), rank(rank) {}
    std::string format() const {
        std::string txt = "Refactorization failed, size=" + std::to_string(size);
        if (rank >= 0) {
            txt += ", rank=" + std::to_string(rank);
        }
        return txt + ".";
    }
END_ERRORCLASS(KluRefactorizationError);

ERRORCLASS(KluMatrixInfNan)
    bool nan;                   // true: NaN, false: Inf
    MatrixEntryIndex row;       // 0-based
    MatrixEntryIndex col;       // 0-based
    Id rowNode;                 // bad Id if unresolved
    Id colNode;                 // bad Id if unresolved
    KluMatrixInfNan(bool nan, MatrixEntryIndex row, MatrixEntryIndex col, Id rowNode, Id colNode)
        : nan(nan), row(row), col(col), rowNode(rowNode), colNode(colNode) {}
    std::string format() const {
        std::string txt = nan ? "NaN found in matrix" : "Inf found in matrix";
        if (rowNode || colNode) {
            txt += ", row node '" + std::string(rowNode) + "', column node '" + std::string(colNode) + "'";
        } else {
            txt += ", row " + std::to_string(row + 1) + ", column " + std::to_string(col + 1);
        }
        return txt + ".";
    }
END_ERRORCLASS(KluMatrixInfNan);

ERRORCLASS(KluVectorInfNan)
    bool nan;                   // true: NaN, false: Inf
    MatrixEntryIndex row;       // 0-based
    Id rowNode;                 // bad Id if unresolved
    KluVectorInfNan(bool nan, MatrixEntryIndex row, Id rowNode)
        : nan(nan), row(row), rowNode(rowNode) {}
    std::string format() const {
        std::string txt = nan ? "NaN found in vector" : "Inf found in vector";
        if (rowNode) {
            txt += ", row node '" + std::string(rowNode) + "'";
        } else {
            txt += ", row " + std::to_string(row + 1);
        }
        return txt + ".";
    }
END_ERRORCLASS(KluVectorInfNan);


template<typename IndexType, typename ValueType> class KluMatrixCore {
public: 
    using Common = typename std::conditional<std::is_same<int32_t,IndexType>::value, klu_common, klu_l_common>::type;
    using Symbolic = typename std::conditional<std::is_same<int32_t,IndexType>::value, klu_symbolic, klu_l_symbolic>::type;
    using Numeric = typename std::conditional<std::is_same<int32_t,IndexType>::value, klu_numeric, klu_l_numeric>::type;

    // Every method that can fail takes an ErrorConsumer& to report through
    // (a default-constructed one is a silent sink). A NameResolver, used to turn
    // an offending row/column index into a node name when an error is built, can
    // be installed with setResolver(); until then errors fall back to raw indices.
    // The matrix does not own the resolver (see setResolver()).
    KluMatrixCore();

    KluMatrixCore           (const KluMatrixCore&)  = delete;
    KluMatrixCore           (      KluMatrixCore&&) = delete;
    KluMatrixCore& operator=(const KluMatrixCore&)  = delete;
    KluMatrixCore& operator=(      KluMatrixCore&&) = delete;

    virtual ~KluMatrixCore();

    bool isBuilt() const { return smap!=nullptr; };

    void deleteKluObjects();

    // Set accounting structure
    void setAccounting(Accounting& accounting) { acct = &accounting; }; 

    // Turn off accounting
    void noAccounting() { acct = nullptr; };

    // Install the name resolver used when building errors. The matrix does NOT
    // take ownership: the resolver is owned by the analysis core that installs
    // it and must outlive the matrix's use of it. A null resolver (the default)
    // -> errors fall back to raw indices.
    void setResolver(NameResolver* resolver) { resolver_ = resolver; }
    NameResolver* resolver() const { return resolver_; }

    // (row, col) of the nonzero at value-array index idx, or (-1, -1) if the
    // sparsity pattern is not available (storage-only matrix).
    std::tuple<IndexType, IndexType> elementAt(IndexType idx) const {
        if (AP.size()==AN+1) {
            for(IndexType col=0; col<AN; col++) {
                if (AP[col]<=idx && idx<AP[col+1]) {
                    return std::make_tuple(AI[idx], col);
                }
            }
        }
        return std::make_tuple(-1, -1);
    }

    // Rebuild it based on the given sparsity map, set to zero, clear error
    bool rebuild(SparsityMap& m, EquationIndex n, ErrorConsumer& ec);

    // Checks if matrix is valid (rebuild completed successfully)
    bool valid() const { return symbolic; };

    // Returns a pointer to element (component). If the element is not found
    // returns a pointer to bucket_ (see the bucket contract on MatrixAccess:
    // writes through it are discarded, reads from it are meaningless - check the
    // found flag if you need a real value).
    // Assumes the undelying type is double or std::complex<double> (Complex)
    // This method is used when the type of the matrix is known.
    double* elementPtr(const MatrixEntryPosition& mep, Component comp=Component::Real) {
        auto entry = smap->find(mep);
        if (!entry) {
            return reinterpret_cast<double*>(&bucket_);
        }
        auto offset = entry->index;
        if constexpr(std::is_same<ValueType, Complex>::value) {
            return (comp==Component::Imaginary) ? 
                reinterpret_cast<double*>(Ax.data()+offset)+1 : 
                reinterpret_cast<double*>(Ax.data()+offset);
        } else {
            return (comp==Component::Imaginary) ? nullptr : (Ax.data()+offset);
        }
    };

    // Returns internal data array or a real matrix
    ValueType* data() { return reinterpret_cast<ValueType*>(Ax.data()); };

    // Return number of unknowns
    IndexType nRow() const { return AN; };
    IndexType nCol() const { return AN; };

    // Return number of nonzeros
    IndexType nnz() const { return nnz_; };

    // Set entries to 0, clear error
    void zero(Component what=Component::Real|Component::Imaginary);

    // Factorization
    bool factor(ErrorConsumer& ec);
    bool refactor(ErrorConsumer& ec);
    bool isFactored() const { return numeric; };

    // Reciprocal pivot growth
    bool rgrowth(double& rgrowth, ErrorConsumer& ec);

    // Cheap reciprocal condition number estimation
    bool rcond(double& rcond, ErrorConsumer& ec);

    // Check matrix for inf/nan
    bool isFinite(bool infCheck, bool nanCheck, ErrorConsumer& ec);

    // Check vector for inf/nan
    bool isFinite(ValueType* vec, bool infCheck, bool nanCheck, ErrorConsumer& ec);

    // Maximal element in row
    bool rowMaxNorm(double* maxNorm);

    // Solve after factorization, result is stored in rhs
    bool solve(ValueType* b, ErrorConsumer& ec);

    // Block solve: solve for nrhs right-hand sides simultaneously.
    // B is stored column-major with leading dimension AN (ldim = AN).
    // On return B contains the solution columns, overwriting the RHS.
    bool solveBlock(ValueType* B, IndexType nrhs, ErrorConsumer& ec);

    // Transpose solve after factorization, result is stored in b
    bool tsolve(ValueType* b, ErrorConsumer& ec);

    // Block transpose solve: solve A^T X = B for nrhs right-hand sides simultaneously.
    // B is stored column-major with leading dimension AN (ldim = AN).
    // On return B contains the solution columns, overwriting the RHS.
    bool tsolveBlock(ValueType* B, IndexType nrhs, ErrorConsumer& ec);

    // Matrix-vector product, result is stored in res
    bool product(ValueType* vec, ValueType* res);

    // Matrix-vector product taking views (arbitrary stride, e.g. a matrix
    // column), result is stored in res. vec and res must not overlap.
    bool product(VectorView<ValueType> vec, VectorView<ValueType> res, ErrorConsumer& ec);

    // Transpose matrix-vector product A^T v, result is stored in res
    bool tproduct(ValueType* vec, ValueType* res);

    // Transpose matrix-vector product A^T v taking views (arbitrary stride,
    // e.g. a matrix column), result is stored in res. vec and res must not overlap.
    bool tproduct(VectorView<ValueType> vec, VectorView<ValueType> res, ErrorConsumer& ec);

    // Residual (Ax-b), stored in res
    bool residual(ValueType* x, ValueType* b, ValueType* res);
    
    // Structural rank
    IndexType structuralRank() const { return common.structural_rank; };

    // Numerical rank
    IndexType numericalRank() const { return common.numerical_rank; };

    // Singular column
    IndexType singularColumn() const { return common.singular_col; };

    // Computes offset of a nonzero element
    std::tuple<IndexType, bool> nonzeroOffset(EquationIndex row, UnknownIndex col);

    // Dump nonzero pattern
    void dumpSparsity(std::ostream& os);

    // Dump nonzero pattern vectors
    void dumpSparsityTables(std::ostream& os);

    // Dump entry values
    void dumpEntries(std::ostream& os);

    // Dump matrix and optional rhs with given column width and precision
    void dump(std::ostream& os, ValueType* rhs=nullptr, int colw=12, int prec=2, bool zeroindex=false);
    
    // Dump vector
    void dumpVector(std::ostream& os, ValueType* v, int colw=12, int prec=2);
    
protected:
    NameResolver* resolver_;    // index -> node name for error messages, not owned; may be null

    Accounting* acct;
    bool isComplex_;
    IndexType nnz_;
    IndexType AN;
    Vector<IndexType> AP;
    Vector<IndexType> AI;
    Vector<ValueType> Ax;
    Symbolic* symbolic;
    Numeric* numeric;
    Common common;
    SparsityMap* smap;

    // Scratch sink returned by elementPtr()/valuePtr()/cxValuePtr() for a
    // position that is not in the matrix. See the bucket contract on
    // MatrixAccess: writes are discarded, reads are meaningless. Never zeroed
    // after rebuild() and aliased by every missing position.
    ValueType bucket_;
};

// KluMatrixCore does not include a MatrixAcces interface
// It can be used to derive more advanced classes (e.g. block-sparse matrix). 
// KluAtomicMatrix includes a MatrixAcces interface. 
// It should not be used as the base class for new matrix classes. 
template<typename IndexType, typename ValueType> 
class KluAtomicMatrix : public KluMatrixCore<IndexType, ValueType>, public MatrixAccess<IndexType> {
public:
    // Matrix binding interface
    // Block element position is ignored
    virtual double* valueArray();
    virtual Complex* cxValueArray();
    virtual std::tuple<IndexType, bool> valueIndex(const MatrixEntryPosition& mep, const std::optional<MatrixEntryPosition>& blockMep=std::nullopt) const;
    virtual double* valuePtr(const MatrixEntryPosition& mep, Component comp=Component::Real, const std::optional<MatrixEntryPosition>& blockMep=std::nullopt);
    virtual Complex* cxValuePtr(const MatrixEntryPosition& mep, const std::optional<MatrixEntryPosition>& blockMep=std::nullopt);
};

// KLU matrix classes (used as base for more advanced classes)
typedef KluMatrixCore<MatrixEntryIndex, double> KluRealMatrixCore;
typedef KluMatrixCore<MatrixEntryIndex, Complex> KluComplexMatrixCore;

// KLU matrix classes with a MatrixAcces interface
typedef KluAtomicMatrix<MatrixEntryIndex, double> KluRealMatrix;
typedef KluAtomicMatrix<MatrixEntryIndex, Complex> KluComplexMatrix;

// MatrixAccess interface class
typedef MatrixAccess<MatrixEntryIndex> KluMatrixAccess;
}

#endif
