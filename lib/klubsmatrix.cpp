#include "klubsmatrix.h"
#include "common.h"
#include <iomanip>
#include <algorithm>
#include <type_traits>

namespace NAMESPACE {

template<typename IndexType, typename ValueType> KluBlockSparseMatrixCore<IndexType, ValueType>::KluBlockSparseMatrixCore(bool largeBucket) 
    : blockBucket_(nullptr), largeBucket_(largeBucket) {
}

template<typename IndexType, typename ValueType> KluBlockSparseMatrixCore<IndexType, ValueType>::~KluBlockSparseMatrixCore() {
}

template<typename IndexType, typename ValueType> 
double* KluBlockSparseMatrixCore<IndexType, ValueType>::valueArray() {
    if constexpr(std::is_same<ValueType, Complex>::value) {
        return nullptr;
    } else {
        return KluMatrixCore<IndexType, ValueType>::data();
    }
} 

template<typename IndexType, typename ValueType> 
Complex* KluBlockSparseMatrixCore<IndexType, ValueType>::cxValueArray() {
    if constexpr(std::is_same<ValueType, Complex>::value) {
        return KluMatrixCore<IndexType, ValueType>::data();
    } else {
        return nullptr;
    }
} 

template<typename IndexType, typename ValueType> 
std::tuple<IndexType, bool> KluBlockSparseMatrixCore<IndexType, ValueType>::valueIndex(
    const MatrixEntryPosition& mep, const std::optional<MatrixEntryPosition>& blockMep
) const {
    return elementIndex(mep, blockMep);
}

template<typename IndexType, typename ValueType> 
double* KluBlockSparseMatrixCore<IndexType, ValueType>::valuePtr(
    const MatrixEntryPosition& mep, Component comp, const std::optional<MatrixEntryPosition>& blockMep
) {
    return elementPtr(mep, comp, blockMep);
}

template<typename IndexType, typename ValueType> 
Complex* KluBlockSparseMatrixCore<IndexType, ValueType>::cxValuePtr(
    const MatrixEntryPosition& mep, const std::optional<MatrixEntryPosition>& blockMep
) {
    if constexpr(std::is_same<ValueType, Complex>::value) {
        auto [nzPosition, found] = elementIndex(mep, blockMep);
        if (found) {
            return Ax.data()+nzPosition;
        } else {
            // Bucket contract (see MatrixAccess): writes discarded, reads
            // meaningless. blockMep is ignored here - every missing subentry
            // aliases the bucket origin.
            return blockBucket_;
        }
    } else {
        return nullptr;
    }
}

template<typename IndexType, typename ValueType> 
bool KluBlockSparseMatrixCore<IndexType, ValueType>::rebuild(SparsityMap& m, EquationIndex n, EquationIndex nbRow, UnknownIndex nbCol, bool storageOnly) {
    KluMatrixCore<IndexType, ValueType>::clearError();
    
    KluMatrixCore<IndexType, ValueType>::deleteKluObjects();

    n_ = n;
    nbRow_ = nbRow;
    nbCol_ = nbCol;

    smap = &m;
    
    // Set number of columns of elements
    AN = n_*nbCol_;

    // Number of nonzeros (for now, assume all dense blocks are fully dense)
    nnz_ = m.size()*nbRow_*nbCol_;

    // Allocate arrays
    denseColumnBegin.resize(n+1);
    blockColumnOrigin.resize(n);
    blockColumnStride.resize(n);
    if (!storageOnly) {
        AP.resize(AN+1);
        AI.resize(nnz_);
    } else {
        AP.clear();
        AI.clear();
    }
    
    // Element column index
    decltype(nnz_) atCol = 0;

    // Nonzero element index
    decltype(nnz_) atNz = 0;
    
    // Collect indices of the beginnings of columns in the sparsity map positions vector. 
    // Entries are already sorted by column first, then row
    // so they are in the same order as the dense blocks appear in column-major ordering. 
    denseColumnBegin[0] = 0;
    atCol = 0;
    auto& positions = m.positions();
    for(size_t posNdx=0; posNdx<positions.size(); posNdx++) {
        auto [mep, flags] = positions[posNdx];
        auto [row, col] = mep;
        // Make column index 0-based
        col--;
        // Reached next column
        if (col!=atCol) {
            // Make sure empty columns of blocks are also handled
            for(; atCol<col;) {
                atCol++;
                denseColumnBegin[atCol] = posNdx;
            }
        }
    }
    // Trailing empty columns
    // n-1<atCol is an internal error!
    for(; atCol<n-1;) {
        atCol++;
        denseColumnBegin[atCol] = positions.size();
    }
    // Number of dense blocks
    denseColumnBegin[n] = positions.size();

    // Index of a column of elements
    atCol = 0;
    // Index of nonzero element
    atNz = 0;

    // Iterate through columns of dense blocks
    // This also iterates throuh columns with no dense blocks
    for(decltype(n) blockColNdx=0; blockColNdx<n; blockColNdx++) {
        // Beginning and end of a column of blocks
        auto colBeginNdx = denseColumnBegin[blockColNdx];
        auto colEndNdx = denseColumnBegin[blockColNdx+1];

        // How many dense blocks do we have in this column
        auto blocksInColumn = colEndNdx-colBeginNdx;

        // Add column origin and stride
        blockColumnOrigin[blockColNdx] = atNz;
        blockColumnStride[blockColNdx] = blocksInColumn*nbRow_;

        // Iterate through subcolumns of each dense block
        // This loop runs even if there are no dense blocks in this column of dense blocks
        // Therefore AP is filled with indices correctly even in this case
        for(decltype(nbCol_) subColNdx=0; subColNdx<nbCol_; subColNdx++) {
            // Add index of first nonzero element in column
            if (!storageOnly) {
                AP[atCol] = atNz;
            }

            // For each dense block in column of dense blocks
            for(auto blkPos = colBeginNdx; blkPos < colEndNdx ; blkPos++) {
                // Get block row and column index
                // We do not need blkCol - it is useful for debugging
                auto [blkMep, blkFlags] = positions[blkPos];
                auto [blkRow, blkCol] = blkMep;
                // These indices are 1-based, make them 0-based
                blkRow--;
                blkCol--;

                // For each row in dense block
                if (!storageOnly) {
                    IndexType rowIndex = blkRow * nbRow_;
                    for(decltype(nbRow_) subRowNdx=0; subRowNdx<nbRow_; subRowNdx++) {
                        // Write row index
                        AI[atNz] = rowIndex;

                        // Advance row index
                        rowIndex++;

                        // Advance nonzero element index
                        atNz++;
                    }
                } else {
                    atNz += nbRow_;
                }
            }

            // Advance column index
            atCol++;
        }
    }

    // Add final AP entry
    if (!storageOnly) {
        AP[atCol] = atNz;
    }
    
    // Allocate array for nozero element values
    Ax.resize(nnz_);

    // Point blockBucket_ at the scratch sink for missing positions (bucket
    // contract on MatrixAccess: writes discarded, reads meaningless). A large
    // bucket is a full nbRow_*nbCol_ column-major block so that offset-based
    // loading (base pointer + bounded element offset) and block()'s real-layout
    // view stay in bounds; a small bucket is the inherited single scalar.
    // Not re-zeroed on same-size rebuilds, and never zeroed by zero() - callers
    // must honour the "reads meaningless" half of the contract.
    if (largeBucket_) {
        bucketStorage_.resize(nbRow_*nbCol_);
        blockBucket_ = bucketStorage_.data();
    } else {
        blockBucket_ = &bucket_;
    }

    // Zero array (Ax only - not the bucket)
    KluMatrixCore<IndexType, ValueType>::zero();
    
    // Set up KLU structures
    int st;
    if constexpr(std::is_same<int32_t, IndexType>::value) {
        st = klu_defaults(&common);
    } else {
        st = klu_l_defaults(&common);
    }
    if (!st) {
        lastError = Error::Defaults;
        // Set smap to nullptr indicating failed rebuild()
        smap = nullptr;
        return false;
    }

    if (!storageOnly) {
        if constexpr(std::is_same<int32_t, IndexType>::value) {
            symbolic = klu_analyze(AN, AP.data(), AI.data(), &common);
        } else {
            symbolic = klu_l_analyze(AN, AP.data(), AI.data(), &common);
        }
        if (!symbolic) {
            lastError = Error::Analysis;
            // Set smap to nullptr indicating failed rebuild()
            smap = nullptr;
            return false;
        }
    } else {
        symbolic = nullptr;
    }
    
    return true;
}

template<typename IndexType, typename ValueType>
void KluBlockSparseMatrixCore<IndexType, ValueType>::dumpBlockSparsity(std::ostream& os) {
   for(IndexType row=0; row<n_; row++) {
        for(IndexType col=0; col<n_; col++) {
            auto [_, found] = block(MatrixEntryPosition(row+1, col+1));
            if (found) {
                os << "x";
            } else {
                os << ".";
            }
        }
        os << "\n";
    }
}


// Instantiate template class for int32 and int64 indices, double and Complex values
template class KluBlockSparseMatrixCore<int32_t, double>;
template class KluBlockSparseMatrixCore<int32_t, Complex>;
template class KluBlockSparseMatrixCore<int64_t, double>;
template class KluBlockSparseMatrixCore<int64_t, Complex>;

}
