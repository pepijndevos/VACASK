#ifndef __DENSEMATRIX_DEFINED
#define __DENSEMATRIX_DEFINED

#include <stdexcept>
#include <vector>
#include <algorithm>
#include <cmath>
#include <optional>
#include <type_traits>
#include <iostream>
#include "common.h"
#include "blaslapack.h"

namespace NAMESPACE {

// True for T = std::complex<U> for any U, not just Complex (= std::complex<double>)
template<typename T> struct is_complex : std::false_type {};
template<typename U> struct is_complex<std::complex<U>> : std::true_type {};

// A vector view into another vector/matrix
// Due to stride_ it can handle columns, rows, and much more
template<typename T> class VectorView {
public:
    VectorView(std::vector<T>& v) : start_(v.data()), n_(v.size()), stride_(1) {};
    // Contiguous prefix of v (offset 0, stride 1), shorter than v itself
    VectorView(std::vector<T>& v, size_t length) : start_(v.data()), n_(length), stride_(1) {};
    VectorView(std::vector<T>& v, size_t offset, size_t length, size_t stride)
        : start_(v.data()+offset), n_(length), stride_(stride) {};
    VectorView(T* start, size_t n, size_t stride) : start_(start), n_(n), stride_(stride) {};
    VectorView(T* start, size_t offset, size_t n, size_t stride) : start_(start+offset), n_(n), stride_(stride) {};

    VectorView<T> subVector(size_t offset, size_t n, size_t stride=1) {
        // Last element of subvector should not be at or beyond last element of original vector
        DBGCHECK((n>0) && ((offset+(n-1)*stride)*stride_>=n_*stride_), "Requested subvector too long.");
        return VectorView<T>(start_+offset*stride_, n, stride_*stride);
    };
    
    // Access to members
    const T& at(size_t col) const { return *(start_+col*stride_); };
    T& at(size_t col) { return *(start_+col*stride_); };

    const T& operator[](size_t i) const { return *(start_+i*stride_); };
    T& operator[](size_t i) { return *(start_+i*stride_); };

    // Length
    size_t n() const { return n_; };

    // Stride
    size_t stride() const { return stride_; };

    // Assign elements from another VectorView
    VectorView<T>& operator=(const VectorView<T>& from) {
        DBGCHECK(n_ != from.n_, "Vector length mismatch.");
        if constexpr(std::is_same<T, double>::value) {
            int n = static_cast<int>(n_);
            int incFrom = static_cast<int>(from.stride_);
            int incThis = static_cast<int>(stride_);
            dcopy_(&n, from.start_, &incFrom, start_, &incThis);
        } else if constexpr(std::is_same<T, Complex>::value) {
            int n = static_cast<int>(n_);
            int incFrom = static_cast<int>(from.stride_);
            int incThis = static_cast<int>(stride_);
            zcopy_(&n, from.start_, &incFrom, start_, &incThis);
        } else {
            T* ptr = start_;
            T* ptrFrom = from.start_;
            for(decltype(n_) i=0; i<n_; i++) {
                *ptr = *ptrFrom;
                ptr += stride_;
                ptrFrom += from.stride_;
            }
        }
        return *this;
    };

    // Assign same value to all elements
    VectorView<T>& operator=(const T& from) {
        if constexpr(std::is_same<T, double>::value || std::is_same<T, Complex>::value) {
            char uplo = 'A';
            int m = 1;
            int n = static_cast<int>(n_);
            int lda = static_cast<int>(stride_);
            if constexpr(std::is_same<T, double>::value) {
                dlaset_(&uplo, &m, &n, &from, &from, start_, &lda);
            } else {
                zlaset_(&uplo, &m, &n, &from, &from, start_, &lda);
            }
        } else {
            T* ptr = start_;
            for(decltype(n_) i=0; i<n_; i++) {
                *ptr = from;
                ptr += stride_;
            }
        }
        return *this;
    };

    // Apply function to each element, put result in result
    void apply(T (*func)(T), VectorView<T>& result) const {
        for(size_t i=0; i<n_; i++) {
            result.at(i) = func(at(i));
        }
    };

    // Apply function to each element in place
    void apply(T (*func)(T)) {
        for(size_t i=0; i<n_; i++) {
            at(i) = func(at(i));
        }
    };

    // Squared norm
    double norm2() const {
        if constexpr(std::is_same<T, double>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            double nrm = dnrm2_(&n, start_, &inc);
            return nrm*nrm;
        } else if constexpr(std::is_same<T, Complex>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            double nrm = dznrm2_(&n, start_, &inc);
            return nrm*nrm;
        } else {
            double nrm = 0;
            T* ptr = start_;
            for(size_t i=0; i<n_; i++) {
                auto a = std::abs(*ptr);
                nrm += a*a;
                ptr += stride_;
            }
            return nrm;
        }
    };

    // Norm
    double norm() const {
        if constexpr(std::is_same<T, double>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            return dnrm2_(&n, start_, &inc);
        } else if constexpr(std::is_same<T, Complex>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            return dznrm2_(&n, start_, &inc);
        } else {
            double nrm = 0;
            T* ptr = start_;
            for(size_t i=0; i<n_; i++) {
                auto a = std::abs(*ptr);
                nrm += a*a;
                ptr += stride_;
            }
            return std::sqrt(nrm);
        }
    };

    // Dot product
    // Conjugates other if T is any std::complex<U>
    T dot(const VectorView<T>& other) const {
        DBGCHECK(n_ != other.n_, "Vector length mismatch.");
        if constexpr(std::is_same<T, double>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            int incOther = static_cast<int>(other.stride_);
            return ddot_(&n, start_, &inc, other.start_, &incOther);
        } else if constexpr(std::is_same<T, Complex>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            int incOther = static_cast<int>(other.stride_);
            // X=other so it gets conjugated, Y=this, matching this->conj(other) below
            return zdotc_(&n, other.start_, &incOther, start_, &inc);
        } else {
            T sum = 0;
            const T* ptr = start_;
            const T* ptrOther = other.start_;
            for(size_t i=0; i<n_; i++) {
                if constexpr(is_complex<T>::value) {
                    sum += *ptr * std::conj(*ptrOther);
                } else {
                    sum += *ptr * *ptrOther;
                }
                ptr += stride_;
                ptrOther += other.stride_;
            }
            return sum;
        }
    };

    // Orthogonalize to vector wrt
    void orthogonalize(const VectorView<T>& wrt) {
        // dot() checks vector compatibility
        auto prod = dot(wrt);
        auto nrm2 = wrt.norm2();
        auto fac = prod/nrm2;
        if constexpr(std::is_same<T, double>::value) {
            int n = static_cast<int>(n_);
            int incWrt = static_cast<int>(wrt.stride_);
            int incThis = static_cast<int>(stride_);
            double negFac = -fac;
            daxpy_(&n, &negFac, wrt.start_, &incWrt, start_, &incThis);
        } else if constexpr(std::is_same<T, Complex>::value) {
            int n = static_cast<int>(n_);
            int incWrt = static_cast<int>(wrt.stride_);
            int incThis = static_cast<int>(stride_);
            Complex negFac = -fac;
            zaxpy_(&n, &negFac, wrt.start_, &incWrt, start_, &incThis);
        } else {
            T* ptr = start_;
            T* ptrWrt = wrt.start_;
            for(size_t i=0; i<n_; i++) {
                *ptr -= fac * *ptrWrt;
                ptr += stride_;
                ptrWrt += wrt.stride_;
            }
        }
    };

    // Swap elements with other
    // Assume vectors have no common elements (e.g. row crossing a column)
    void swap(VectorView<T>& other) {
        DBGCHECK(n_ != other.n_, "Vector length mismatch.");
        if constexpr(std::is_same<T, double>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            int incOther = static_cast<int>(other.stride_);
            dswap_(&n, start_, &inc, other.start_, &incOther);
        } else if constexpr(std::is_same<T, Complex>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            int incOther = static_cast<int>(other.stride_);
            zswap_(&n, start_, &inc, other.start_, &incOther);
        } else {
            T* ptr = start_;
            T* ptrOther = other.start_;
            for(size_t i=0; i<n_; i++) {
                auto tmp = *ptr;
                *ptr = *ptrOther;
                *ptrOther = tmp;
                ptr += stride_;
                ptrOther += other.stride_;
            }
        }
    };

    // Swap elements with other (other is a rvalue reference)
    // Assume vectors have no common elements (e.g. row crossing a column)
    void swap(VectorView<T>&& other) {
        swap(other);
    };

    // Swap i-th and j-th elements
    void swap(size_t i, size_t j) {
        T tmp = at(j);
        at(j) = at(i);
        at(i) = tmp;
    };

    // v = v * factor
    void scale(T factor) {
        if constexpr(std::is_same<T, double>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            dscal_(&n, &factor, start_, &inc);
        } else if constexpr(std::is_same<T, Complex>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            zscal_(&n, &factor, start_, &inc);
        } else {
            T* ptr = start_;
            for(size_t i=0; i<n_; i++) {
                *ptr *= factor;
                ptr += stride_;
            }
        }
    };

    // v = v * factor, special case for complex v and real factor
    // Uses zdscal_ instead of promoting factor to Complex and going through scale(T)'s
    // zscal_, avoiding a complex-complex multiplication for what is really a
    // real scaling.
    template<typename U=T, typename=std::enable_if_t<std::is_same<U, Complex>::value>>
    void scale(double factor) {
        int n = static_cast<int>(n_);
        int inc = static_cast<int>(stride_);
        zdscal_(&n, &factor, start_, &inc);
    };

    // v = v + other * factor
    void addScaled(const VectorView<T>& other, T factor) {
        DBGCHECK(n_ != other.n_, "Vector length mismatch.");
        if constexpr(std::is_same<T, double>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            int incOther = static_cast<int>(other.stride_);
            daxpy_(&n, &factor, other.start_, &incOther, start_, &inc);
        } else if constexpr(std::is_same<T, Complex>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            int incOther = static_cast<int>(other.stride_);
            zaxpy_(&n, &factor, other.start_, &incOther, start_, &inc);
        } else {
            T* ptr = start_;
            const T* ptrOther = other.start_;
            for(size_t i=0; i<n_; i++) {
                *ptr += *ptrOther * factor;
                ptr += stride_;
                ptrOther += other.stride_;
            }
        }
    };

    // v = v0 + other * factor
    void vectorPlusScaledVector(const VectorView<T>& v0, const VectorView<T>& other, T factor) {
        DBGCHECK(n_ != v0.n_ || n_ != other.n_, "Vector length mismatch.");
        if constexpr(std::is_same<T, double>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            int incV0 = static_cast<int>(v0.stride_);
            int incOther = static_cast<int>(other.stride_);
            dcopy_(&n, v0.start_, &incV0, start_, &inc);
            daxpy_(&n, &factor, other.start_, &incOther, start_, &inc);
        } else if constexpr(std::is_same<T, Complex>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            int incV0 = static_cast<int>(v0.stride_);
            int incOther = static_cast<int>(other.stride_);
            zcopy_(&n, v0.start_, &incV0, start_, &inc);
            zaxpy_(&n, &factor, other.start_, &incOther, start_, &inc);
        } else {
            T* ptr = start_;
            const T* ptrV0 = v0.start_;
            const T* ptrOther = other.start_;
            for(size_t i=0; i<n_; i++) {
                *ptr = *ptrV0 + *ptrOther * factor;
                ptr += stride_;
                ptrV0 += v0.stride_;
                ptrOther += other.stride_;
            }
        }
    };

    // v = v + other
    void add(const VectorView<T>& other) {
        DBGCHECK(n_ != other.n_, "Vector length mismatch.");
        if constexpr(std::is_same<T, double>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            int incOther = static_cast<int>(other.stride_);
            double one = 1.0;
            daxpy_(&n, &one, other.start_, &incOther, start_, &inc);
        } else if constexpr(std::is_same<T, Complex>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            int incOther = static_cast<int>(other.stride_);
            Complex one = 1.0;
            zaxpy_(&n, &one, other.start_, &incOther, start_, &inc);
        } else {
            T* ptr = start_;
            const T* ptrOther = other.start_;
            for(size_t i=0; i<n_; i++) {
                *ptr += *ptrOther;
                ptr += stride_;
                ptrOther += other.stride_;
            }
        }
    };

    // v = other * factor
    // Write scaled vector
    void scaledVector(const VectorView<T>& other, T factor) {
        DBGCHECK(n_ != other.n_, "Vector length mismatch.");
        if constexpr(std::is_same<T, double>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            int incOther = static_cast<int>(other.stride_);
            dcopy_(&n, other.start_, &incOther, start_, &inc);
            dscal_(&n, &factor, start_, &inc);
        } else if constexpr(std::is_same<T, Complex>::value) {
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            int incOther = static_cast<int>(other.stride_);
            zcopy_(&n, other.start_, &incOther, start_, &inc);
            zscal_(&n, &factor, start_, &inc);
        } else {
            T* ptr = start_;
            const T* ptrOther = other.start_;
            for(size_t i=0; i<n_; i++) {
                *ptr = *ptrOther * factor;
                ptr += stride_;
                ptrOther += other.stride_;
            }
        }
    };

    // Maximal absolute element
    double maxAbs() const {
        if constexpr(std::is_same<T, double>::value) {
            if (n_ == 0) {
                return 0;
            }
            int n = static_cast<int>(n_);
            int inc = static_cast<int>(stride_);
            int idx = idamax_(&n, start_, &inc);
            return std::abs(start_[(idx-1)*stride_]);
        } else {
            // No BLAS routine here: izamax_ picks the max by |Re|+|Im| rather than the
            // true modulus, which can select a different (up to sqrt(2) off) element
            // than this loop's std::abs()-based comparison. No live Complex call site
            // exists today, so we keep the exact semantics instead of that approximation.
            double m = 0;
            T* ptr = start_;
            for(size_t i=0; i<n_; i++) {
                auto c = std::abs(*ptr);
                if (c>m) {
                    m = c;
                }
                ptr += stride_;
            }
            return m;
        }
    };

    void dump(std::ostream& os) const {
        T* ptr = start_;
        for(size_t i=0; i<n_; i++) {
            os << *ptr << " ";
            ptr += stride_;
        }
        os << "\n";
    };
    
private:
    T* start_;
    size_t n_;
    size_t stride_;
};

template<typename T> std::ostream& operator<<(std::ostream& os, const VectorView<T>& obj) {
    for(decltype(obj.n()) i=0; i<obj.n(); i++) {
        if (i>0) {
            os << " ";
        }
        os << obj[i];
    }
    return os;
}

// Deduction guides for VectorView
template<typename T> VectorView(std::vector<T>& v) -> VectorView<T>;
template<typename T> VectorView(std::vector<T>& v, size_t offset, size_t length, size_t stride) -> VectorView<T>;
template<typename T> VectorView(T* start, size_t n, size_t stride=1) -> VectorView<T>;

template<typename T> class DenseMatrixView {
public:
    // Default constructor, uninitialized view
    DenseMatrixView() 
        : start_(nullptr), nRow_(0), nCol_(0), rowStride_(0), colStride_(0) {};

    // Construct from array
    DenseMatrixView(T* start, size_t nRow, size_t nCol, size_t rowStride, size_t colStride) 
        : start_(start), nRow_(nRow), nCol_(nCol), rowStride_(rowStride), colStride_(colStride) {}; 

    // Size
    size_t nRows() const { return nRow_; }; 
    size_t nCols() const { return nCol_; }; 

    // Element access
    const T& at(size_t row, size_t col) const { return *(start_ + row*rowStride_ + col*colStride_); };
    T& at(size_t row, size_t col) { return *(start_ + row*rowStride_ + col*colStride_); };

    // Pointer to element (e.g. for building a truncated sub-row/column VectorView)
    const T* ptrOf(size_t row, size_t col) const { return start_ + row*rowStride_ + col*colStride_; };
    T* ptrOf(size_t row, size_t col) { return start_ + row*rowStride_ + col*colStride_; };

    // Row access
    const VectorView<T> row(size_t i) const { return VectorView<T>(start_ + i*rowStride_, nCol_, colStride_); };
    VectorView<T> row(size_t i) { return VectorView<T>(start_ + i*rowStride_, nCol_, colStride_); };
    
    // Column access
    const VectorView<T> column(size_t i) const { return VectorView<T>(start_ + i*colStride_, nRow_, rowStride_); };
    VectorView<T> column(size_t i) { return VectorView<T>(start_ + i*colStride_, nRow_, rowStride_); };

    // Diagonal access - min(nRow_,nCol_) elements, stride rowStride_+colStride_
    // (one step down and one step across per element, same as at(i,i)-at(i-1,i-1))
    const VectorView<T> diagonal() const { return VectorView<T>(start_, std::min(nRow_, nCol_), rowStride_+colStride_); };
    VectorView<T> diagonal() { return VectorView<T>(start_, std::min(nRow_, nCol_), rowStride_+colStride_); };

    // Assign elements from another MatrixView
    DenseMatrixView<T>& operator=(const DenseMatrixView<T>& other) {
        DBGCHECK(nRow_!=other.nRow_ || nCol_!=other.nCol_, "Matrices do not match.");
        if constexpr(std::is_same<T, double>::value || std::is_same<T, Complex>::value) {
            // Both views must be contiguous in the SAME direction: either both
            // native column-major, or both row-major (read as transposed
            // column-major - a positional copy is not transpose-symmetric like
            // fill/identity, so a this-row-major/other-column-major mismatch
            // still falls through to the per-row loop below, same as before).
            bool nativeColMajor = rowStride_==1 && other.rowStride_==1;
            bool nativeRowMajor = colStride_==1 && other.colStride_==1;
            if (nativeColMajor || nativeRowMajor) {
                char uplo = 'A';
                int m = static_cast<int>(nativeColMajor ? nRow_ : nCol_);
                int n = static_cast<int>(nativeColMajor ? nCol_ : nRow_);
                int lda = static_cast<int>(nativeColMajor ? other.colStride_ : other.rowStride_);
                int ldb = static_cast<int>(nativeColMajor ? colStride_ : rowStride_);
                // LAPACK requires lda/ldb>=max(1,m); see identity()'s lda clamp
                // for why the degenerate nRow_==1/nCol_==1 case needs it, and
                // the explicit 1 floor for why nRow_==0/nCol_==0 needs it too.
                lda = std::max({lda, m, 1});
                ldb = std::max({ldb, m, 1});
                if constexpr(std::is_same<T, double>::value) {
                    dlacpy_(&uplo, &m, &n, other.start_, &lda, start_, &ldb);
                } else {
                    zlacpy_(&uplo, &m, &n, other.start_, &lda, start_, &ldb);
                }
                return *this;
            }
        }
        for(size_t i=0; i<nRow_; i++) {
            row(i) = other.row(i);
        }
        return *this;
    };

    // Assign value to all elements
    DenseMatrixView<T>& operator=(const T& val) {
        if (!laset(val, val)) {
            for(size_t i=0; i<nRow_; i++) {
                row(i) = val;
            }
        }
        return *this;
    };

    // Set to zero
    void zero() {
        *this = 0;
    };

    // Set to identity
    void identity() {
        if (laset(T(0), T(1))) {
            return;
        }
        for(size_t i=0; i<nRow_; i++) {
            for(size_t j=0; j<nCol_; j++) {
                at(i, j) = (i==j) ? 1 : 0;
            }
        }
    };

    // Perform LU decomposition in place, return row permutation vector
    // Use partial pivoting
    bool factor(VectorView<int>& rowPerm) {
        auto n = nCol_;
        DBGCHECK(rowPerm.n()!=n, "Row permutation vector length does not match matrix size.");
        DBGCHECK(nRow_!=n, "Matrix is not square.");
        // LAPACK needs elements within a column contiguous (rowStride_==1); it has no
        // increment parameter like BLAS, only a leading dimension between columns.
        // rowPerm is int-typed (matching LAPACK's ipiv storage width) and, when
        // contiguous, is passed directly as ipiv, no scratch buffer/copy needed.
        if constexpr(std::is_same<T, double>::value || std::is_same<T, Complex>::value) {
            if (rowStride_==1 && rowPerm.stride()==1) {
                return lapackFactor(rowPerm);
            }
        }
        return luSolveCore(static_cast<VectorView<T>*>(nullptr), &rowPerm);
    };

    // Solve Ax = rhs given the LU decomposition (stored in this matrix, see
    // factor()) and the associated row permutation vector.
    // Diagonal and upper triangle hold U, strictly below the diagonal holds
    // L (unit diagonal, not stored).
    // Permutes rhs in place, then solves by forward and backward substitution.
    bool luSolve(VectorView<T>& rhs, const VectorView<int>& rowPerm) {
        auto n = nCol_;
        DBGCHECK(rhs.n()!=n, "Vector length does not match matrix size.");
        DBGCHECK(rowPerm.n()!=n, "Row permutation vector length does not match matrix size.");
        DBGCHECK(nRow_!=n, "Matrix is not square.");

        // Same LAPACK storage requirements as factor(): matrix and rowPerm contiguous,
        // and additionally rhs itself (LAPACK's B has no increment parameter either,
        // only a leading dimension between right-hand-side columns).
        if constexpr(std::is_same<T, double>::value || std::is_same<T, Complex>::value) {
            if (rowStride_==1 && rowPerm.stride()==1 && rhs.stride()==1) {
                lapackSolve(&rhs[0], 1, static_cast<int>(n), rowPerm);
                return true;
            }
        }

        // Permute rhs in place by replaying the same interchanges performed
        // on the matrix rows in luSolveCore(): rowPerm[i] is the 1-based row
        // swapped with row i at elimination step i (LAPACK ipiv convention),
        // so replaying the swaps in increasing i order reproduces the same
        // permutation. swap() takes 0-based indices, hence the -1.
        for(size_t i=0; i+1<n; i++) {
            rhs.swap(i, static_cast<size_t>(rowPerm[i]-1));
        }

        // Forward substitution, L has implicit unit diagonal
        for(size_t i=0; i<n; i++) {
            auto matRow = row(i);
            for(size_t j=0; j<i; j++) {
                rhs[i] -= matRow[j]*rhs[j];
            }
        }

        // Back substitution, U is stored on and above the diagonal
        for(size_t cnt=0; cnt<n; cnt++) {
            auto i = n-1-cnt;
            auto matRow = row(i);
            for(size_t j=i+1; j<n; j++) {
                rhs[i] -= matRow[j]*rhs[j];
            }
            rhs[i] /= matRow[i];
        }

        return true;
    };

    // Solve Ax = rhs, destroy A, result in rhs (vector)
    // Use partial pivoting
    // rowPerm is optional scratch for the pivot sequence; when given (and the LAPACK
    // storage requirements below are met) it also enables the LAPACK fast path.
    bool factorAndLuSolve(VectorView<T>& rhs, VectorView<int>* rowPerm = nullptr) {
        auto n = nCol_;
        DBGCHECK(rhs.n()!=n, "Vector length does not match matrix size.");
        DBGCHECK(rowPerm && rowPerm->n()!=n, "Row permutation vector length does not match matrix size.");
        DBGCHECK(nRow_!=n, "Matrix is not square.");
        // Same LAPACK storage requirements as factor()/luSolve(): matrix, rhs and
        // rowPerm all contiguous. Without a rowPerm there is nowhere to put ipiv, so
        // LAPACK is skipped even for an otherwise-compatible matrix/rhs.
        if constexpr(std::is_same<T, double>::value || std::is_same<T, Complex>::value) {
            if (rowPerm && rowStride_==1 && rhs.stride()==1 && rowPerm->stride()==1) {
                if (!lapackFactor(*rowPerm)) {
                    return false;
                }
                lapackSolve(&rhs[0], 1, static_cast<int>(n), *rowPerm);
                return true;
            }
        }
        return luSolveCore(&rhs, rowPerm);
    };

    // Solve Ax = Rhs, destroy A, result in rhs (matrix)
    // Use partial pivoting
    // rowPerm is optional scratch for the pivot sequence; when given (and the LAPACK
    // storage requirements below are met) it also enables the LAPACK fast path.
    bool factorAndLuSolve(DenseMatrixView<T>& rhs, VectorView<int>* rowPerm = nullptr) {
        auto n = nCol_;
        DBGCHECK(rhs.nRow_!=n, "Vector length does not match matrix size.");
        DBGCHECK(rowPerm && rowPerm->n()!=n, "Row permutation vector length does not match matrix size.");
        DBGCHECK(nRow_!=n, "Matrix is not square.");
        // Same LAPACK storage requirements as the VectorView-rhs overload: this matrix,
        // rowPerm, and rhs (elements within each right-hand-side column) all contiguous.
        if constexpr(std::is_same<T, double>::value || std::is_same<T, Complex>::value) {
            if (rowPerm && rowStride_==1 && rhs.rowStride_==1 && rowPerm->stride()==1) {
                if (!lapackFactor(*rowPerm)) {
                    return false;
                }
                lapackSolve(rhs.start_, static_cast<int>(rhs.nCol_), static_cast<int>(rhs.colStride_), *rowPerm);
                return true;
            }
        }
        return luSolveCore(&rhs, rowPerm);
    };

    // Destructive invert, result must be distinct from this
    // rowPerm is optional scratch for the pivot sequence; when given (and the LAPACK
    // storage requirements in factorAndLuSolve() are met) it also enables the LAPACK
    // fast path there. Inverting is just solving A*X=I, so this reuses that method
    // rather than duplicating its LAPACK dispatch.
    bool factorAndInvert(DenseMatrixView<T>& result, VectorView<int>* rowPerm = nullptr) {
        DBGCHECK(nRow_!=result.nRow_ || nCol_!=result.nCol_, "Matrices are not compatible.");
        DBGCHECK(nRow_!=nCol_, "Matrix is not square.");
        result.identity();
        return factorAndLuSolve(result, rowPerm);
    };

    // Multiply with vector, store result in result
    // Vector must be distinct from result
    void multiply(const VectorView<T>& vector, VectorView<T>& result) const {
        DBGCHECK(nCol_!=vector.n(), "Matrix is not compatible with vector.");
        DBGCHECK(nRow_!=result.n(), "Result is not compatible with product.");
        for(size_t i=0; i<nRow_; i++) {
            result[i] = row(i).dot(vector);
        }
    };

    // Multiply with vector, add to result
    // Vector must be distinct from result
    void multiplyAdd(const VectorView<T>& vector, VectorView<T>& result) const {
        DBGCHECK(nCol_!=vector.n(), "Matrix is not compatible with vector.");
        DBGCHECK(nRow_!=result.n(), "Result is not compatible with product.");
        for(size_t i=0; i<nRow_; i++) {
            result[i] += row(i).dot(vector);
        }
    };

    // Multiply with matrix, store result in result
    // Result must be distinct from this and other
    void multiply(const DenseMatrixView<T>& other, DenseMatrixView<T>& result) const {
        DBGCHECK(nCol_!=other.nRow_, "Matrices are not compatible.");
        DBGCHECK(nRow_!=result.nRow_ || other.nCol_!=result.nCol_, "Result is not compatible with product.");
        for(size_t i=0; i<nRow_; i++) {
            for(size_t j=0; j<other.nCol_; j++) {
                result.at(i, j) = row(i).dot(other.column(j));
            }
        }
    };

    // this = other * factor
    // Write scaled matrix
    void scaledMatrix(const DenseMatrixView<T>& other, T factor) {
        DBGCHECK(nRow_!=other.nRow_ || nCol_!=other.nCol_, "Matrices are not compatible.");
        for(size_t i=0; i<nRow_; i++) {
            row(i).scaledVector(other.row(i), factor);
        }
    };

    // Add scaled other matrix, put result in this matrix
    void addScaledMatrix(const DenseMatrixView<T>& other, T factor) {
        DBGCHECK(nRow_!=other.nRow_ || nCol_!=other.nCol_, "Matrices are not compatible.");
        for(size_t i=0; i<nRow_; i++) {
            row(i).addScaled(other.row(i), factor);
        }
    };

    // Add other matrix to this matrix
    void addMatrix(const DenseMatrixView<T>& other) {
        addScaledMatrix(other, 1.0);
    };

    // Subtract other matrix from this matrix
    void subtractMatrix(const DenseMatrixView<T>& other) {
        addScaledMatrix(other, -1.0);
    };

    // this = other scaled by vector, row-wise
    void scaledRows(const DenseMatrixView<T>& other, const VectorView<T>& vector) {
        DBGCHECK(nRow_!=other.nRow_ || nCol_!=other.nCol_, "Matrices are not compatible.");
        DBGCHECK(nRow_!=vector.n(), "Matrix is not compatible with vector.");
        for(size_t i=0; i<nRow_; i++) {
            row(i).scaledVector(other.row(i), vector[i]);
        }
    };

    // this = this + other scaled by vector, row-wise
    void addScaledRows(const DenseMatrixView<T>& other, const VectorView<T>& vector) {
        DBGCHECK(nRow_!=other.nRow_ || nCol_!=other.nCol_, "Matrices are not compatible.");
        DBGCHECK(nRow_!=vector.n(), "Matrix is not compatible with vector.");
        for(size_t i=0; i<nRow_; i++) {
            row(i).addScaled(other.row(i), vector[i]);
        }
    };

    // this = other scaled by vector, column-wise
    void scaledColumns(const DenseMatrixView<T>& other, const VectorView<T>& vector) {
        DBGCHECK(nRow_!=other.nRow_ || nCol_!=other.nCol_, "Matrices are not compatible.");
        DBGCHECK(nCol_!=vector.n(), "Matrix is not compatible with vector.");
        for(size_t j=0; j<nCol_; j++) {
            column(j).scaledVector(other.column(j), vector[j]);
        }
    };

    // this = this + other scaled by vector, column-wise
    void addScaledColumns(const DenseMatrixView<T>& other, const VectorView<T>& vector) {
        DBGCHECK(nRow_!=other.nRow_ || nCol_!=other.nCol_, "Matrices are not compatible.");
        DBGCHECK(nCol_!=vector.n(), "Matrix is not compatible with vector.");
        for(size_t j=0; j<nCol_; j++) {
            column(j).addScaled(other.column(j), vector[j]);
        }
    };

    // Apply function to each element, put result in result
    void apply(T (*func)(T), DenseMatrixView<T>& result) const {
        for(size_t i=0; i<nRow_; i++) {
            auto rrow = result.row(i);
            row(i).apply(func, rrow);
        }
    };

    // Apply function to each element in place
    void apply(T (*func)(T)) {
        for(size_t i=0; i<nRow_; i++) {
            row(i).apply(func);
        }
    };

    // Absolute maximal element
    double maxAbs() const {
        if constexpr(std::is_same<T, double>::value || std::is_same<T, Complex>::value) {
            if (rowStride_==1 || colStride_==1) {
                // Row-major storage (colStride_==1) is handled by reinterpreting
                // as the transpose in LAPACK's column-major terms - max(abs(.))
                // over all elements is invariant under transpose, so this is
                // safe either way (same reasoning as identity()/laset()).
                int m = static_cast<int>(rowStride_==1 ? nRow_ : nCol_);
                int n = static_cast<int>(rowStride_==1 ? nCol_ : nRow_);
                int lda = static_cast<int>(rowStride_==1 ? colStride_ : rowStride_);
                lda = std::max({lda, m, 1});
                char norm = 'M';
                if constexpr(std::is_same<T, double>::value) {
                    return dlange_(&norm, &m, &n, start_, &lda, nullptr);
                } else {
                    return zlange_(&norm, &m, &n, start_, &lda, nullptr);
                }
            }
        }
        double m = 0;
        for(size_t i=0; i<nRow_; i++) {
            auto c = row(i).maxAbs();
            if (c>m) {
                m = c;
            }
        }
        return m;
    };

    
    void dump(std::ostream& os) const {
        for(size_t i=0; i<nRow_; i++) {
            for(size_t j=0; j<nCol_; j++) {
                os << at(i, j) << " ";
            }
            os << "\n";
        }
    };
    
protected:
    T* start_;
    size_t nRow_;
    size_t nCol_;
    size_t rowStride_;
    size_t colStride_;

private:
    // Fill extra-diagonal elements with alpha and diagonal elements with beta
    // via LAPACK's dlaset_/zlaset_ (uplo='A' touches every element, so
    // alpha==beta gives a uniform fill and alpha=0/beta=1 gives identity()).
    // Returns false (no-op) if T isn't double/Complex or this view isn't
    // contiguous in one stride direction, so the caller can fall back to an
    // explicit loop.
    bool laset(const T& alpha, const T& beta) {
        if constexpr(std::is_same<T, double>::value || std::is_same<T, Complex>::value) {
            if (rowStride_==1 || colStride_==1) {
                // Row-major storage (colStride_==1) is handled by reinterpreting
                // as the transpose in LAPACK's column-major terms - filling by
                // extra-diagonal/diagonal value is symmetric so this is safe
                // either way. Using the real m/n/lda (rather than requiring
                // full packing) also fast-paths a genuine strided sub-block
                // view, e.g. a KLUBS block with colStride_ > nRow_ (see
                // docs/internals/klubsmatrix.md).
                char uplo = 'A';
                int m = static_cast<int>(rowStride_==1 ? nRow_ : nCol_);
                int n = static_cast<int>(rowStride_==1 ? nCol_ : nRow_);
                int lda = static_cast<int>(rowStride_==1 ? colStride_ : rowStride_);
                // LAPACK requires lda>=max(1,m); the degenerate nRow_==1/nCol_==1
                // case can otherwise report an lda smaller than m, and nRow_==0/
                // nCol_==0 (e.g. a matrix mid-addRow()-growth) can report lda==0
                // (harmless for dlaset_/zlaset_ since lda is unused when the
                // corresponding loop bound is 0, but keeps the call
                // contract-legal for stricter LAPACK backends).
                lda = std::max({lda, m, 1});
                if constexpr(std::is_same<T, double>::value) {
                    dlaset_(&uplo, &m, &n, &alpha, &beta, start_, &lda);
                } else {
                    zlaset_(&uplo, &m, &n, &alpha, &beta, start_, &lda);
                }
                return true;
            }
        }
        return false;
    }

    // Factor this (square, n x n, LAPACK-contiguous) matrix in place via
    // dgetrf_/zgetrf_, storing ipiv in rowPerm. Caller must have already
    // checked rowStride_==1 && rowPerm.stride()==1. Returns info==0
    // (successful, non-singular factorization).
    bool lapackFactor(VectorView<int>& rowPerm) {
        int m = static_cast<int>(nCol_);
        int nn = m;
        int lda = static_cast<int>(colStride_);
        int info = 0;
        if constexpr(std::is_same<T, double>::value) {
            dgetrf_(&m, &nn, start_, &lda, &rowPerm[0], &info);
        } else {
            zgetrf_(&m, &nn, start_, &lda, &rowPerm[0], &info);
        }
        if (info<0) {
            throw std::invalid_argument("Invalid argument passed to LAPACK dgetrf/zgetrf.");
        }
        return info==0;
    }

    // Solve using the factorization from lapackFactor(), via dgetrs_/zgetrs_.
    // rhs is n x nrhs with leading dimension ldb. Caller must have already
    // checked rowStride_==1 && rowPerm.stride()==1 and rhs's own contiguity.
    void lapackSolve(T* rhs, int nrhs, int ldb, const VectorView<int>& rowPerm) {
        char trans = 'N';
        int nn = static_cast<int>(nCol_);
        int lda = static_cast<int>(colStride_);
        int info = 0;
        if constexpr(std::is_same<T, double>::value) {
            dgetrs_(&trans, &nn, &nrhs, start_, &lda, &rowPerm[0], rhs, &ldb, &info);
        } else {
            zgetrs_(&trans, &nn, &nrhs, start_, &lda, &rowPerm[0], rhs, &ldb, &info);
        }
        if (info<0) {
            throw std::invalid_argument("Invalid argument passed to LAPACK dgetrs/zgetrs.");
        }
    }

    // Destructive solve/factor core
    // Used when LAPACK is not appropropriate
    // Replaces matrix content with LU decomposition. 
    // Solution is placed in rhs. 
    // Stores row permutation vector. 
    // Returns true on success. 
    template<typename RhsType> bool luSolveCore(RhsType* rhs, VectorView<int>* rowPerm = nullptr) {
        auto n = nCol_;
        if constexpr(std::is_same<RhsType, VectorView<T>>::value) {
            DBGCHECK(rhs && rhs->n()!=n, "Vector length does not match matrix size.");
        } else if constexpr(std::is_same<RhsType, DenseMatrixView<T>>::value) {
            DBGCHECK(rhs && rhs->nRows()!=n, "Vector length does not match matrix size.");
        } else {
            DBGCHECK(rhs, "Bad rhs type.");
        }
        DBGCHECK(rowPerm && rowPerm->n()!=n, "Row permutation vector length does not match matrix size.");
        DBGCHECK(nRow_!=n, "Matrix is not square.");

        // Eliminate
        for(size_t i=0; i<n-1; i++) {
            // Find pivot
            auto pivCol = column(i);
            size_t pivI = i;
            auto p = std::abs(pivCol[pivI]);
            for(size_t j=i; j<n; j++) {
                auto cand = std::abs(pivCol[j]);
                if (cand>p) {
                    p = cand;
                    pivI = j;
                }
            }
            if (p==0) {
                return false;
            }
            
            // Record pivot partner for this step (LAPACK ipiv convention, 1-based)
            if (rowPerm) {
                (*rowPerm)[i] = static_cast<int>(pivI)+1;
            }

            // Swap with pivot
            if (pivI!=i) {
                auto pivRow = row(pivI);
                auto pivDest = row(i);
                pivRow.swap(pivDest);
                if (rhs) {
                    if constexpr(std::is_same<RhsType, VectorView<T>>::value) {
                        rhs->swap(i, pivI);
                    } else {
                        rhs->row(i).swap(rhs->row(pivI));
                    }
                }
            }

            // Eliminate in column i, rows i+1..n-1
            for(size_t j=i+1; j<n; j++) {
                auto pivRow = row(i);
                auto targetRow = row(j);
                T fac = -targetRow[i]/pivRow[i];
                for(size_t k=i+1; k<n; k++) {
                    targetRow[k] += fac*pivRow[k];
                }
                targetRow[i] = -fac;
                if (rhs) {
                    if constexpr(std::is_same<RhsType, VectorView<T>>::value) {
                        rhs->at(j) += fac*rhs->at(i);
                    } else {
                        auto rhsRow = rhs->row(i);
                        rhs->row(j).addScaled(rhsRow, fac);
                    }
                }
            }
        }

        // Back-substitute
        if (rhs) {
            for(size_t cnt=0; cnt<n; cnt++) {
                auto i = n-1-cnt;
                auto matRow = row(i);
                if constexpr(std::is_same<RhsType, VectorView<T>>::value) {
                    for(size_t j=i+1; j<n; j++) {
                        rhs->at(i) -= rhs->at(j)*matRow[j];
                    }
                    rhs->at(i) /= matRow[i];
                } else {
                    auto rhsRowi = rhs->row(i);
                    for(size_t j=i+1; j<n; j++) {
                        auto rhsRowj = rhs->row(j);
                        auto fac = -matRow[j];
                        rhsRowi.addScaled(rhsRowj, fac);
                    }
                    rhsRowi.scale(1.0/matRow[i]);
                }
            }
        }
        return true;
    };
};


// Dense matrix stored in row-major order
template<typename T> class DenseMatrix : public DenseMatrixView<T> {
public:
    enum class Major { Row=0, Column=1 }; 

    using DenseMatrixView<T>::start_;
    using DenseMatrixView<T>::nRow_;
    using DenseMatrixView<T>::nCol_;
    using DenseMatrixView<T>::rowStride_;
    using DenseMatrixView<T>::colStride_;

    // Default constructor, uninitialized matrix
    DenseMatrix() 
        : DenseMatrixView<T>(nullptr, 0, 0, 1, 1), major_(Major::Row) {};

    // Copy constructor
    DenseMatrix(const DenseMatrix<T>& A) {
        major_ = A.major_;
        data_ = A.data_;
        start_ = data_.data();
        nRow_ = A.nRow_;
        nCol_ = A.nCol_;
        setStride();
    };

    // Move constructor
    DenseMatrix(DenseMatrix<T>&& A) {
        major_ = A.major_;
        data_ = std::move(A.data_);
        start_ = data_.data();
        nRow_ = A.nRow_;
        nCol_ = A.nCol_;
        setStride();
    };

    // Size-based constructor
    DenseMatrix(size_t nRow, size_t nCol, Major major=Major::Row) 
        : DenseMatrixView<T>(nullptr, nRow, nCol, 1, 1), major_(major) { 
        data_.resize(nRow*nCol); 
        start_ = data_.data();
        setStride();
    };

    // Move-construct from vector and size
    DenseMatrix(std::vector<T>&& from, size_t nRow, size_t nCol, Major major=Major::Row) {
        DBGCHECK(nRow*nCol != from.size(), "Matrix size inconsistent with data.");
        major_ = major;
        data_ = std::move(from);
        start_ = data_.data();
        nRow_ = nRow;
        nCol_ = nCol;
        setStride();
    };

    // Copy-construct from vector and size
    DenseMatrix(const std::vector<T>& from, size_t nRow, size_t nCol, Major major=Major::Row) {
        DBGCHECK(nRow*nCol != from.size(), "Matrix size inconsistent with data.");
        major_ = major;
        data_ = from;
        start_ = data_.data();
        nRow_ = nRow;
        nCol_ = nCol;
        setStride();
    };

    // Explicit view of this matrix's data, e.g. for
    // coeffs.view() = other; - avoids DenseMatrix's own operator=(const
    // DenseMatrix&) (which also copies major_) without the ambiguous
    // "DenseMatrixView(coeffs) = other;" CTAD parse (most-vexing-parse:
    // that's a declaration of a new variable named coeffs, not an
    // expression, when coeffs is already declared in the same scope).
    DenseMatrixView<T> view() { return DenseMatrixView<T>(start_, nRow_, nCol_, rowStride_, colStride_); };

    // Copy assignment
    DenseMatrix<T>& operator=(const DenseMatrix<T>& other) {
        major_ = other.major_;
        data_ = other.data_;
        start_ = data_.data();
        nRow_ = other.nRow_;
        nCol_ = other.nCol_;
        setStride();
        return *this;
    };

    // Move assignment
    DenseMatrix<T>& operator=(DenseMatrix<T>&& other) {
        major_ = other.major_;
        data_ = std::move(other.data_);
        start_ = data_.data();
        nRow_ = other.nRow_;
        nCol_ = other.nCol_;
        setStride();
        return *this;
    };

    // Assign value to all elements (forwards to DenseMatrixView's fill
    // overload explicitly, rather than a blanket 'using
    // DenseMatrixView<T>::operator=' - that would also expose the base's
    // view-to-view operator=(const DenseMatrixView<T>&) on a DenseMatrix<T>
    // lvalue, which copy-vs-move assignment above already covers via
    // major_-copying DenseMatrix-to-DenseMatrix semantics. Callers who
    // specifically want "copy values, keep my own major_" still reach the
    // base operator= explicitly via a DenseMatrixView<T>& cast, as
    // lib/corehbxform.cpp already does.
    DenseMatrix<T>& operator=(const T& val) {
        DenseMatrixView<T>::operator=(val);
        return *this;
    };
    
    // Resize, does not reorder elements (content is invalidated)
    void resize(size_t nRow, size_t nCol, Major major=Major::Row) { 
        major_ = major;
        data_.resize(nRow*nCol);
        nRow_ = nRow; 
        nCol_ = nCol; 
        start_ = data_.data();
        setStride();
    }; 

    // Grows the matrix by one row and returns a VectorView over it. The new
    // row's contents are unspecified from the caller's point of view - don't
    // rely on it being zero (the current implementation happens to
    // value-initialize it via std::vector, but that's an implementation
    // detail, not a guarantee); every caller must fill it explicitly.
    VectorView<T> addRow() {
        if (major_==Major::Column) {
            // Column-major: every column's block grows by one element (the
            // new row's entry), so the buffer can't just grow at the tail
            // like the row-major case below - it must be reshaped, with
            // each old nRow_-element column block copied into the
            // corresponding (nRow_+1)-element block of a fresh buffer.
            std::vector<T> newData((nRow_+1)*nCol_);
            for(size_t j=0; j<nCol_; j++) {
                VectorView<T> newCol(newData.data()+j*(nRow_+1), nRow_, 1);
                newCol = this->column(j);
            }
            data_ = std::move(newData);
            nRow_++;
        } else {
            nRow_++;
            data_.resize(nRow_*nCol_);
        }
        start_ = data_.data();
        setStride();
        return this->row(nRow_-1);
    };

    // Retrieve internal data structure. The returned vector must not be
    // resized/reallocated directly (push_back, resize, insert, ...) - that
    // would invalidate start_ (and, for column-major matrices, colStride_)
    // without triggering a resync. Use resize()/addRow() instead.
    std::vector<T>& data() { return data_; };
    const std::vector<T>& data() const { return data_; };

    // Get index of element within internal data structure
    size_t indexOf(size_t row, size_t col) const {
        switch (major_) {
            case Major::Row:
                return row*nCol_+col; 
            case Major::Column:
            default:
                return row+nRow_*col; 
        }
    }

    static bool test();

private:
    void setStride() {
        switch (major_) {
            case Major::Row:
                rowStride_ = nCol_;
                colStride_ = 1;
                break;
            case Major::Column:
                rowStride_ = 1;
                colStride_ = nRow_;
                break;
        }
    };

    Major major_;
    std::vector<T> data_;
};

}

#endif
