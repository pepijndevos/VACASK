#ifndef __KLUBSMATRIX_DEFINED
#define __KLUBSMATRIX_DEFINED

#include <unordered_map>
#include <complex>
#include <type_traits>
#include <optional>
#include "status.h"
#include "densematrix.h"
#include "klumatrix.h"
#include "identifier.h"
#include "flags.h"
#include "hash.h"
#include "acct.h"
#include "common.h"


namespace NAMESPACE {

// Blocks of size nb x nb
// mep          .. coordinates of the block
// blockMep     .. element coordinates within the block
// block origin .. element with blockMep=(0,0)
template<typename IndexType, typename ValueType> 
class KluBlockSparseMatrixCore : public KluMatrixCore<IndexType, ValueType>, public MatrixAccess<IndexType> {
public: 
    // largeBucket controls the scratch block returned for a missing position
    // (the "bucket", see the contract on MatrixAccess):
    //   true  - blockBucket_ is a full nbRow_*nbCol_ column-major block. Required
    //           for Jacobian loading with offsets (base pointer + bounded element
    //           offset), and lets block() hand back a real-layout view.
    //   false - blockBucket_ is a single scalar (bucket_). Only safe when the
    //           matrix is never offset-loaded and block() views of a missing
    //           block are never dereferenced past element 0.
    // All current users pass true.
    KluBlockSparseMatrixCore(bool largeBucket=true);
    ~KluBlockSparseMatrixCore();

    KluBlockSparseMatrixCore           (const KluBlockSparseMatrixCore&)  = delete;
    KluBlockSparseMatrixCore           (      KluBlockSparseMatrixCore&&) = delete;
    KluBlockSparseMatrixCore& operator=(const KluBlockSparseMatrixCore&)  = delete;
    KluBlockSparseMatrixCore& operator=(      KluBlockSparseMatrixCore&&) = delete;

    // No need to override elementPtr() since the returns value of valueIndex()
    // is the index into flat sparse matrix. 
    
    // BlockSparseMatrixCore specific interface
    // Returns a dense matrix view of a block. 
    // Storage is column major due to KLU. 
    // A block column occupies a consecutive block of memory. 
    // Consecutive columns of the same bloc do not generally occupy a continuous block of memory. 
    // Column stride depends on the number of dense blocks in a column of dense blocks. 
    //   column stride = number of dense blocks in the column x nb
    // Returns DenseMatrixView of block, found flag
    // If the block is not found a view of the blockBucket_ scratch is returned.
    // With a large bucket that scratch is a full nbRow_ x nbCol_ column-major
    // block, so the view has the same layout as a real block (row stride 1,
    // column stride nbRow_ - the bucket is contiguous). With a single-scalar
    // bucket every element must alias blockBucket_[0], so both strides are 0.
    std::tuple<DenseMatrixView<ValueType>, bool> block(const MatrixEntryPosition& mep) {
        auto [nzPosition, found] = elementIndex(mep);
        if (!found) {
            if (largeBucket_) {
                return std::make_tuple(DenseMatrixView<ValueType>(blockBucket_, nbRow_, nbCol_, 1, nbRow_), false);
            }
            return std::make_tuple(DenseMatrixView<ValueType>(blockBucket_, nbRow_, nbCol_, 0, 0), false);
        }
        // KLU organizes elements in column major order
        // row stride is 1, column stride depends on the column of dense blocks
        // Get 0-based block position
        auto [row, col] = mep;
        row--;
        col--;
        return std::make_tuple(
            DenseMatrixView<ValueType>(Ax.data()+nzPosition, nbRow_, nbCol_, 1, blockColumnStride[col]), 
            true
        );
    };

    // Rebuild it based on the given sparsity map of dense blocks, 
    // n x n dense blocks with nb x nb elements
    // Set elements to zero, clear error
    // If storageOnly is true the structures for accessing scalar entries (AP, AI) are not built. 
    // Such matrices cannot be factored/solved. 
    bool rebuild(SparsityMap& m, EquationIndex n, EquationIndex nbRow, UnknownIndex nbCol, bool storageOnly=false);

    // Returns the linear nonzero element index coresponding to dense block
    // at block position mep (0-based), block element position blockMep (1-based). 
    // If blockMep is not given assumes (0, 0), i.e. block origin. 
    // Returns index, found. found=true if element exists. 
    std::tuple<IndexType, bool> elementIndex(const MatrixEntryPosition& mep, const std::optional<MatrixEntryPosition>& blockMep=std::nullopt) const {
        auto entry = smap->find(mep);
        if (!entry) {
            return std::make_tuple(0, false);
        }
        
        // Get 0-based block position
        auto [row, col] = mep;
        row--;
        col--;
        // Get index of the first dense block in column
        auto firstBlockInColumn = denseColumnBegin[col];
        // Which block in column is this
        auto blockInColumn = entry->index - firstBlockInColumn;
        // Compute element index of block origin
        auto nzPosition = blockColumnOrigin[col] + nbRow_*blockInColumn;

        // Do we have a blockMep
        if (blockMep.has_value()) {
            // Get 0-based dense block element position
            auto [brow, bcol] = blockMep.value();
            // Compute element position
            nzPosition += bcol * blockColumnStride[col] + brow;
        }
        return std::make_tuple(nzPosition, true); 
    };

    // Returns a pointer to element (component).
    // Assumes the undelying type is double or std::complex<double> (Complex)
    // This method is used when the type of the matrix is known.
    // If blockMep is not given returns the element at the origin of a dense block.
    // If the block is not found returns the origin of blockBucket_ (see the
    // bucket contract on MatrixAccess: writes discarded, reads meaningless).
    // Note: the not-found return ignores blockMep and comp - every missing
    // subentry of a missing block aliases the bucket origin. Offset-based
    // loading (base pointer + bounded element offset) is only done into
    // large-bucket matrices, where blockBucket_ is a full nbRow_*nbCol_ block
    // and the offset stays in bounds.
    double* elementPtr(const MatrixEntryPosition& mep, Component comp=Component::Real, const std::optional<MatrixEntryPosition>& blockMep=std::nullopt) {
        auto [nzPosition, found] = elementIndex(mep, blockMep);
        if (!found) {
            return reinterpret_cast<double*>(blockBucket_);
        }
        // Return pointer
        if constexpr(std::is_same<ValueType, Complex>::value) {
            return (comp==Component::Imaginary) ? 
                reinterpret_cast<double*>(Ax.data()+nzPosition)+1 : 
                reinterpret_cast<double*>(Ax.data()+nzPosition);
        } else {
            return (comp==Component::Imaginary) ? nullptr : (Ax.data()+nzPosition);
        }
    };

    IndexType nBlockRows() const { return n_; };
    IndexType nBlockCols() const { return n_; };
    IndexType nBlockElementRows() const { return nbRow_; };
    IndexType nBlockElementCols() const { return nbCol_; };
    
    void dumpBlockSparsity(std::ostream& os);

protected:
    using Error = KluMatrixCore<IndexType, ValueType>::Error;
    using KluMatrixCore<IndexType, ValueType>::smap;
    using KluMatrixCore<IndexType, ValueType>::nnz_;
    using KluMatrixCore<IndexType, ValueType>::AN;
    using KluMatrixCore<IndexType, ValueType>::AP;
    using KluMatrixCore<IndexType, ValueType>::AI;
    using KluMatrixCore<IndexType, ValueType>::Ax;
    using KluMatrixCore<IndexType, ValueType>::lastError;
    using KluMatrixCore<IndexType, ValueType>::common;
    using KluMatrixCore<IndexType, ValueType>::symbolic;
    using KluMatrixCore<IndexType, ValueType>::bucket_;

    // Number of blocks in row/column
    // Blocks structure is square
    IndexType n_;

    // Number of rows/columns in a dense block
    // Internal structure of a block can be rectangular. 
    // Of course, such matrices cannot be LU decomposed, 
    // but hey can be useful for storing block-sparse data, 
    // e.g. in HB analysis. 
    IndexType nbRow_;
    IndexType nbCol_;

    // Origin of dense block column (origin of topmost dense block).
    // This is the linear index of the nonzero element at the topmost block's origin. 
    // Index is within the array of scalars holding matrix nomnzeros. 
    // Array has n entries. 
    Vector<IndexType> blockColumnOrigin;
    
    // Number of nnz elements to skip to reach the element 
    // in the next column of the same row of a dense block. 
    // Depends on the number of dense blocks in a column. 
    //   number of dense blocks in the column x nb
    // Array has n elements. 
    Vector<IndexType> blockColumnStride;

    // Dense blocks are organized in the same order as nonzeros in ordinary 
    // sparse matrices (column major order) in a linear sequence. 
    // For a dense block we can get its 0-based index in this sequence from 
    // the sparsity map. 
    // If we know the index of the first block in the column stored in 
    // denseColumnBegin and the index of the block we are interested in 
    // we can compute the 0-based consecutive number of this block in its 
    // column. This distance multiplied by the number of elements in a 
    // dense block's column is the offset of blocks's origin from the 
    // origin of the first block in this column in terms of scalars. 
    // Has n+1 elements where the n+1-th element is the number of dense blocks. 
    Vector<IndexType> denseColumnBegin;

    // Scratch sink for missing positions (see the bucket contract on
    // MatrixAccess: writes discarded, reads meaningless). Points at either
    // bucketStorage_ (largeBucket_, a full nbRow_*nbCol_ column-major block, so
    // an offset added to the resolved base pointer stays in bounds and block()
    // can return a real-layout view) or the inherited scalar bucket_
    // (!largeBucket_). Never zeroed after rebuild(); aliased by every missing
    // position.
    // TODO: make bucket static, resize when a larger one is requested
    ValueType* blockBucket_;
    bool largeBucket_;
    Vector<ValueType> bucketStorage_;
    
public:
    // Matrix binding interface
    // If blockMep is not given the block origin is returned
    virtual double* valueArray();
    virtual Complex* cxValueArray();
    virtual std::tuple<IndexType, bool> valueIndex(const MatrixEntryPosition& mep, const std::optional<MatrixEntryPosition>& blockMep=std::nullopt) const;
    virtual double* valuePtr(const MatrixEntryPosition& mep, Component comp=Component::Real, const std::optional<MatrixEntryPosition>& blockMep=std::nullopt);
    virtual Complex* cxValuePtr(const MatrixEntryPosition& mep, const std::optional<MatrixEntryPosition>& blockMep=std::nullopt);
};

// Default KLU matrix flavor
typedef KluBlockSparseMatrixCore<MatrixEntryIndex, double> KluBlockSparseRealMatrix;
typedef KluBlockSparseMatrixCore<MatrixEntryIndex, Complex> KluBlockSparseComplexMatrix;

}

#endif
