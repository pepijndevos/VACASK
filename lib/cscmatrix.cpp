#include "cscmatrix.h"
#include "common.h"
#include <iomanip>
#include <algorithm>
#include <type_traits>

namespace NAMESPACE {

void SparsityMap::clear() {
    smap.clear();
    ordering.clear();
}

// We could make ordering more efficient - now we must order up to n^2 elements 
// which is on average n^2 log(n^2) operations with std::sort(). 
// If we collect them by columns and then sort each column separately, 
// the average complexity with std::sort() is n n log(n) operations which is half 
// of the previous. 
void SparsityMap::enumerate() {
    // Prepare a vector of map keys
    ordering.clear();
    for(auto it=smap.begin(); it!=smap.end(); ++it) {
        ordering.push_back({it->first, it->second.flags});
    }
    
    // Order them
    struct {
        bool operator()(const OrderedEntry& lhs, const OrderedEntry& rhs) const {
            const auto& [l, lflags] = lhs;
            const auto& [r, rflags] = rhs;
            // Compare first by column (unknown), then by row (equation)
            return (l.second < r.second) || ((l.second == r.second) && (l.first < r.first));
        }
    } comparison;
    
    std::sort(ordering.begin(), ordering.end(), comparison);

    // Traverse keys, enumerate entries
    MatrixEntryIndex num = 0;
    for(auto it=ordering.begin(); it!=ordering.end(); ++it) {
        // first = equation, second = unknown
        const auto& [mep, flags] = *it;
        smap[mep].index = num;
        num++;
    }
}

void SparsityMap::dump(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');
    for(auto& it : ordering) {
        const auto& [mep, flags] = it;
        auto entry = find(mep);
        auto [e, u] = mep;
        os << pfx << "(" << e << ", " << u << ") : ";
        if (entry) {
            os << entry->index;
        } else {
            os << "?";
        }
        os << "\n";
    }
}

template<typename IndexType, typename ValueType> CSCMatrixCore<IndexType, ValueType>::CSCMatrixCore()
    : resolver_(nullptr),
      acct(nullptr),
      isComplex_(std::is_same<ValueType, Complex>::value),
      nnz_(0),
      AN(0),
      smap(nullptr),
      bucket_{} {
    // Sanity check: IndexType can only be int32_t or int64_t
    static_assert(
        std::is_same<IndexType, int>::value || std::is_same<IndexType, int64_t>::value, 
        "Klu matrix index type is neither int32_t nor int64_t."
    );
    // Sanity check: ValueType can only be double or std::complex<double>
    static_assert(
        std::is_same<ValueType, double>::value || std::is_same<ValueType, std::complex<double>>::value, 
        "Klu matrix value type is neither double nor std::complex<double>."
    );
}

template<typename IndexType, typename ValueType> CSCMatrixCore<IndexType, ValueType>::~CSCMatrixCore() {
}

template<typename IndexType, typename ValueType> bool CSCMatrixCore<IndexType, ValueType>::rebuild(SparsityMap& m, EquationIndex n, ErrorConsumer& ec) {

    smap = &m;
    
    AN = n;
    nnz_ = m.size();
    AP.resize(n+1);
    AI.resize(nnz_);
    
    decltype(nnz_) atCol = 0;
    decltype(nnz_) atNz = 0;
    decltype(nnz_) curCol = 0;   // columns 0..curCol already have AP[] written
    AP[0] = 0;

    // Go through allocated Jacobian entries.
    // These entries are already sorted by column first, then row,
    // so they are in the same order as they appear in jacI.
    // Note that Jacobian indices are zero-based, so we subtract 1 from row and column index.
    // CSC column pointers: AP has AN+1 entries, AP[c] is the offset of column c's
    // first nonzero, AP[AN] == nnz, and an empty column satisfies AP[c] == AP[c+1].
    for(auto it=m.positions().begin(); it!=m.positions().end(); ++it) {
        const auto& [mep, flags] = *it;
        auto row = mep.first;
        auto col = mep.second;

        // Skip entries that have zero index (they correspond to ground)
        if (!row || !col) {
            continue;
        }

        // KLU sparsity pattern indices are 0-based
        row -= 1;
        col -= 1;

        // Close every column before col (including empty ones) at the
        // current running nonzero count.
        while (curCol < col) {
            AP[curCol+1] = atNz;
            curCol++;
        }
        AI[atNz] = row;
        atNz++;
    }
    // Close the last populated column and any trailing empty columns.
    while (curCol < AN) {
        AP[curCol+1] = atNz;
        curCol++;
    }
    
    // New does not call default constructor for builtin types
    // i.e. doubles are not initialized to 0. 
    // If, however we do
    //   new double[...]() 
    // then initialization takes place. 
    Ax.resize(nnz_);
    zero();

    return true;
}

template<typename IndexType, typename ValueType> void CSCMatrixCore<IndexType, ValueType>::zero(Component what) {
    if constexpr(std::is_same<ValueType, Complex>::value) {
        if (what==(Component::Real|Component::Imaginary)) {
            for(IndexType i=0; i<nnz_; i++) {
                Ax[i] = 0.0;
            }
        } else if (what==Component::Real) {
            for(IndexType i=0; i<nnz_; i++) {
                Ax[i].real(0.0);
            }
        } else if (what==Component::Imaginary) {
            for(IndexType i=0; i<nnz_; i++) {
                Ax[i].imag(0.0);
            }
        }
    } else {
        if ((what&Component::Real)==Component::Real) {
            for(IndexType i=0; i<nnz_; i++) {
                Ax[i] = 0.0;
            }
        }
    }
}

template<typename IndexType, typename ValueType> bool CSCMatrixCore<IndexType, ValueType>::isFinite(bool infCheck, bool nanCheck, ErrorConsumer& ec) {

    if (!infCheck && !nanCheck) {
        return true;
    }
    // Check matrix
    bool gotInf = false;
    bool gotNan = false;
    IndexType i;
    for(i=0; i<nnz_; i++) {
        if constexpr(std::is_same<ValueType, Complex>::value) {
            if (nanCheck) {
                gotNan = gotNan || (std::isnan(Ax[i].real()) || std::isnan(Ax[i].imag()));
            } 
            if (infCheck) {
                gotInf = gotInf || (std::isinf(Ax[i].real()) || std::isinf(Ax[i].imag()));
            }
        } else {
            if (nanCheck) {
                gotNan = gotNan || std::isnan(Ax[i]);
            }
            if (infCheck) {
                gotInf = gotInf || std::isinf(Ax[i]);
            }
        }
        if (gotInf || gotNan) {
            break;
        }
    }
           
    if (gotInf || gotNan) {
        auto [row, col] = elementAt(i);
        ec.push(CSCMatrixInfNan{
            gotNan,
            static_cast<MatrixEntryIndex>(row),
            static_cast<MatrixEntryIndex>(col),
            resolver_ ? (*resolver_)(row) : Id(),
            resolver_ ? (*resolver_)(col) : Id()
        });
        return false;
    }
    return true;
}

template<typename IndexType, typename ValueType> bool CSCMatrixCore<IndexType, ValueType>::isFinite(ValueType* vec, bool infCheck, bool nanCheck, ErrorConsumer& ec) {

    if (!infCheck && !nanCheck) {
        return true;
    }
    for(IndexType i=0; i<AN; i++) {
        bool gotInf = false;
        bool gotNan = false;
        if constexpr(std::is_same<ValueType, Complex>::value) {
            if (nanCheck && (std::isnan(vec[i].real()) || std::isnan(vec[i].imag()))) {
                // NaN found
                gotNan = true;
            } 
            if (infCheck && (std::isinf(vec[i].real()) || std::isinf(vec[i].imag()))) {
                // Inf found
                gotInf = true;
            }
        } else {
            if (nanCheck && std::isnan(vec[i])) {
                // NaN found
                gotNan = true;
            } 
            if (infCheck && std::isinf(vec[i])) {
                // Inf found
                gotInf = true;
            }
        }
        if (gotInf || gotNan) {
            ec.push(CSCVectorInfNan{
                gotNan,
                static_cast<MatrixEntryIndex>(i),
                resolver_ ? (*resolver_)(i) : Id()
            });
            return false;
        }
    }  
    return true;
}

template<typename IndexType, typename ValueType> bool CSCMatrixCore<IndexType, ValueType>::rowMaxNorm(double* maxNorm) {
    // Zero out result
    for(IndexType i=0; i<AN; i++) {
        maxNorm[i] = 0.0;
    }

    // Go through entries
    for(IndexType i=0; i<nnz_; i++) {
        auto row = AI[i];
        double nrm;
        if constexpr(std::is_same<double, ValueType>::value) {
            nrm = std::abs(Ax[i]);
        } else {
            nrm = std::sqrt(Ax[i].real()*Ax[i].real() + Ax[i].imag()*Ax[i].imag());
        }
        if (nrm>maxNorm[row]) {
            maxNorm[row] = nrm;
        }
    }

    return true;
}

// Both vectors must be distinct
template<typename IndexType, typename ValueType> bool CSCMatrixCore<IndexType, ValueType>::product(ValueType* vec, ValueType* res) {
    // Zero out result
    for(IndexType i=0; i<AN; i++) {
        res[i] = 0.0;
    }

    // Go through entries
    for(IndexType col=0; col<AN; col++) {
        IndexType col1 = AP[col];
        IndexType col2 = AP[col+1];
        ValueType v = vec[col];
        for(IndexType i=col1; i<col2; i++) {
            auto row = AI[i];
            res[row] += Ax[i]*v;
        }
    }

    return true;
}

// Both views must be distinct
template<typename IndexType, typename ValueType> bool CSCMatrixCore<IndexType, ValueType>::product(VectorView<ValueType> vec, VectorView<ValueType> res, ErrorConsumer& ec) {
    if (vec.n()!=static_cast<size_t>(AN) || res.n()!=static_cast<size_t>(AN)) {
        ec.push(CSCMulVecSizeMismatch{});
        return false;
    }

    // TODO: check for VectorView overlap

    // Zero out result
    for(IndexType i=0; i<AN; i++) {
        res[i] = 0.0;
    }

    // Go through entries
    for(IndexType col=0; col<AN; col++) {
        IndexType col1 = AP[col];
        IndexType col2 = AP[col+1];
        ValueType v = vec[col];
        for(IndexType i=col1; i<col2; i++) {
            auto row = AI[i];
            res[row] += Ax[i]*v;
        }
    }

    return true;
}

// Both vectors must be distinct
template<typename IndexType, typename ValueType> bool CSCMatrixCore<IndexType, ValueType>::tproduct(ValueType* vec, ValueType* res) {
    for(IndexType i=0; i<AN; i++) {
        res[i] = 0.0;
    }

    for(IndexType col=0; col<AN; col++) {
        IndexType col1 = AP[col];
        IndexType col2 = AP[col+1];
        for(IndexType i=col1; i<col2; i++) {
            res[col] += Ax[i]*vec[AI[i]];
        }
    }

    return true;
}

// Both views must be distinct
template<typename IndexType, typename ValueType> bool CSCMatrixCore<IndexType, ValueType>::tproduct(VectorView<ValueType> vec, VectorView<ValueType> res, ErrorConsumer& ec) {
    if (vec.n()!=static_cast<size_t>(AN) || res.n()!=static_cast<size_t>(AN)) {
        ec.push(CSCMulVecSizeMismatch{});
        return false;
    }

    // TODO: check for VectorView overlap

    for(IndexType i=0; i<AN; i++) {
        res[i] = 0.0;
    }

    for(IndexType col=0; col<AN; col++) {
        IndexType col1 = AP[col];
        IndexType col2 = AP[col+1];
        for(IndexType i=col1; i<col2; i++) {
            res[col] += Ax[i]*vec[AI[i]];
        }
    }

    return true;
}

// All 3 vectors must be distinct
template<typename IndexType, typename ValueType> bool CSCMatrixCore<IndexType, ValueType>::residual(ValueType* x, ValueType* b, ValueType* res) {
    product(x, res);
    for(IndexType i=0; i<AN; i++) {
        res[i] -= b[i];
    }
    return true;
}

template<typename IndexType, typename ValueType> std::tuple<IndexType, bool> CSCMatrixCore<IndexType, ValueType>::nonzeroOffset(EquationIndex row, UnknownIndex col) {
    IndexType i1 = AP[col];
    IndexType i2 = AP[col+1];

    // Last entry for this column is at i2-1
    i2 = i2-1;
    // i1>i2 ... column is empty
    if (i1>i2) {
        return std::make_tuple(0, false);
    }
    // Check endpoints
    if (AI[i1]==row) {
        return std::make_tuple(i1, true);
    }
    if (AI[i2]==row) {
        return std::make_tuple(i2, true);
    }

    // Bisect for row between i1 and i2
    // At the beginning of the loop body both endpoints i1 and i2 are already checked
    // Therefore if i2-i1==1 we are done.
    while (i2-i1>1) {
        IndexType ic = (i1+i2)/2;
        if (AI[ic]==row) {
            return std::make_tuple(ic, true);
        } else if (AI[ic]>row) {
            i2 = ic;
        } else {
            i1 = ic;
        }
    }
    
    return std::make_tuple(0, false);
}

template<typename IndexType, typename ValueType> void CSCMatrixCore<IndexType, ValueType>::dumpSparsity(std::ostream& os) {
   for(IndexType row=0; row<AN; row++) {
        if (row>0) {
            os << "\n";
        }
        for(IndexType col=0; col<AN; col++) {
            auto [offs, found] = nonzeroOffset(row, col);
            if (found) {
                os << "x";
            } else {
                os << ".";
            }
        }
    }
}

template<typename IndexType, typename ValueType> void CSCMatrixCore<IndexType, ValueType>::dumpSparsityTables(std::ostream& os) {
    os << "Ap: ";
    for(IndexType i=0; i<=AN; i++) {
        os << AP[i] << " ";
    }
    os << "\n";
    os << "Ai: ";
    for(IndexType i=0; i<nnz_; i++) {
        os << AI[i] << " ";
    }
}

template<typename IndexType, typename ValueType> void CSCMatrixCore<IndexType, ValueType>::dumpEntries(std::ostream& os) {
    os << "Ax: ";
    for(IndexType i=0; i<nnz_; i++) {
        os << Ax[i] << " ";
    }
}

template<typename IndexType, typename ValueType> void CSCMatrixCore<IndexType, ValueType>::dump(std::ostream& os, ValueType* rhs, int colw, int prec, bool zeroindex) {
    std::ios oldState(nullptr);
    oldState.copyfmt(os);
    os << std::scientific << std::setprecision(prec);

    os << "     ";
    for(IndexType col=0; col<AN; col++) {
        if constexpr(std::is_same<ValueType, Complex>::value) {
            os << std::setw(colw*2+1) << (zeroindex ? col : col+1);
        } else {
            os << std::setw(colw) << (zeroindex ? col : col+1);
        }
    }
    if (rhs) {
        if constexpr(std::is_same<ValueType, Complex>::value) {
            os << std::setw(colw*2+1) << "rhs";
        } else {
            os << std::setw(colw) << "rhs";
        }
    }
    os << "\n";
    for(IndexType row=0; row<AN; row++) {
        if (row>0) {
            os << "\n";
        }
        os << std::setw(4) << (zeroindex ? row : row+1) << ":";
        for(IndexType col=0; col<AN; col++) {
            double* ptr;
            auto [offs, found] = nonzeroOffset(row, col);
            if (found) {
                if constexpr(std::is_same<ValueType, Complex>::value) {
                    os << std::setw(colw) << (Ax.data()+offs)->real();
                    if ((Ax.data()+offs)->imag()>=0) {
                        os << "+" << std::setw(colw-1) << (Ax.data()+offs)->imag();
                    } else {
                        os << std::setw(colw) << (Ax.data()+offs)->imag();
                    }
                    os << "i";
                } else {
                    os << std::setw(colw) << *(Ax.data()+offs); 
                }
            } else {
                if constexpr(std::is_same<ValueType, Complex>::value) {
                    os << std::setw(colw*2+1) << "0+0i";
                } else {
                    os << std::setw(colw) << 0;
                }
            }
        }
        if (rhs) {
            if constexpr(std::is_same<ValueType, Complex>::value) {
                os << std::setw(colw) << rhs[row].real();
                if (rhs[row].imag()>=0) {
                    os << "+" << std::setw(colw-1) << rhs[row].imag();
                } else {
                    os << std::setw(colw) << rhs[row].imag();
                }
                os << "i";
            } else {
                os << std::setw(colw) << rhs[row];
            }
        }
    }

    os.copyfmt(oldState);
}

template<typename IndexType, typename ValueType> void CSCMatrixCore<IndexType, ValueType>::dumpVector(std::ostream& os, ValueType* v, int colw, int prec) {
    std::ios oldState(nullptr);
    oldState.copyfmt(os);
    os << std::scientific << std::setprecision(prec);

    for(IndexType i=0; i<AN; i++) {
        if (i>0) {
            os << "\n";
        }
        if constexpr(std::is_same<ValueType, Complex>::value) {
            os << std::setw(colw) << v[i].real();
            if (v[i].imag()>=0) {
                os << "+" << std::setw(colw-1) << v[i].imag();
            } else {
                os << std::setw(colw) << v[i].imag();
            }
            os << "i";
        } else {
            os << std::setw(colw) << v[i];
        }
    }

    os.copyfmt(oldState);
}

template<typename IndexType, typename ValueType>
double* CSCAtomicMatrix<IndexType, ValueType>::valueArray() {
    if constexpr(std::is_same<ValueType, Complex>::value) {
        return nullptr;
    } else {
        return CSCMatrixCore<IndexType, ValueType>::axData();
    }
} 

template<typename IndexType, typename ValueType> 
Complex* CSCAtomicMatrix<IndexType, ValueType>::cxValueArray() {
    if constexpr(std::is_same<ValueType, Complex>::value) {
        return CSCMatrixCore<IndexType, ValueType>::axData();
    } else {
        return nullptr;
    }
} 

template<typename IndexType, typename ValueType> 
std::tuple<IndexType, bool> CSCAtomicMatrix<IndexType, ValueType>::valueIndex(
    const MatrixEntryPosition& mep, const std::optional<MatrixEntryPosition>& blockMep
) const {
    auto entry = CSCMatrixCore<IndexType, ValueType>::smap->find(mep);
    if (entry) {
        return std::make_tuple(entry->index, true);
    } else {
        return std::make_tuple(0, false);
    }
}

template<typename IndexType, typename ValueType> 
double* CSCAtomicMatrix<IndexType, ValueType>::valuePtr(
    const MatrixEntryPosition& mep, Component comp, const std::optional<MatrixEntryPosition>& blockMep
) {
    return CSCMatrixCore<IndexType, ValueType>::elementPtr(mep, comp);
}

template<typename IndexType, typename ValueType> 
Complex* CSCAtomicMatrix<IndexType, ValueType>::cxValuePtr(
    const MatrixEntryPosition& mep, const std::optional<MatrixEntryPosition>& blockMep
) {
    if constexpr(std::is_same<ValueType, Complex>::value) {
        auto entry = CSCMatrixCore<IndexType, ValueType>::smap->find(mep);
        if (entry) {
            return CSCMatrixCore<IndexType, ValueType>::Ax.data()+entry->index;
        } else {
            // Missing position: bucket contract (see MatrixAccess) - writes
            // discarded, reads meaningless.
            return &(CSCMatrixCore<IndexType, ValueType>::bucket_);
        }
    } else {
        return nullptr;
    }
}

// Instantiate template class for int32 and int64 indices, double and Complex values
template class CSCMatrixCore<int32_t, double>;
template class CSCMatrixCore<int32_t, Complex>;
template class CSCMatrixCore<int64_t, double>;
template class CSCMatrixCore<int64_t, Complex>;
template class CSCAtomicMatrix<int32_t, double>;
template class CSCAtomicMatrix<int32_t, Complex>;
template class CSCAtomicMatrix<int64_t, double>;
template class CSCAtomicMatrix<int64_t, Complex>;

}
