#include "densematrix.h"
#include "value.h"
#include "common.h"

namespace NAMESPACE {

template<typename T> bool DenseMatrix<T>::test() {
    if constexpr (!std::is_same_v<T, double> && !std::is_same_v<T, Complex>) {
        return true;
    } else {

    // Test status
    bool ok = true;

    // Small matrix inverse
    DenseMatrix<T> S({1.0, 2, 3, 4}, 2, 2);
    std::cout << "Small matrix\n";
    S.dump(std::cout);
    std::cout << "\n";
    DenseMatrix<T> S1 = S;
    DenseMatrix<T> SI(2, 2);
    if (!S1.factorAndInvert(SI)) {
        ok = false;
        std::cout << "Small matrix inversion reported singular\n";
    }
    DenseMatrix<T> SIexact({-2, 1, 1.5, -0.5}, 2, 2);
    std::cout << "Small matrix inverse\n";
    SI.dump(std::cout);
    std::cout << "\n";
    for(size_t i=0; i<2; i++) {
        bool exit = false;
        for(size_t j=0; j<2; j++) {
            if (std::abs(SI.at(i,j)-SIexact.at(i,j))>1e-12) {
                ok = false;
                exit = true;
                std::cout << "Small matrix inverse failed\n";
                break;
            }
        }
        if (exit) {
            break;
        }
    }

    // Small matrix right-multiplied by inverse
    DenseMatrix<T> B(2, 2);
    S.multiply(SI, B);
    std::cout << "Small matrix right-multiplied by inverse\n";
    B.dump(std::cout);
    std::cout << "\n";
    for(size_t i=0; i<2; i++) {
        bool exit = false;
        for(size_t j=0; j<2; j++) {
            if (
                i==j && std::abs(B.at(i,j)-1.0)>1e-12 ||
                i!=j && std::abs(B.at(i,j))>1e-12
            ) {
                ok = false;
                exit = true;
                std::cout << "Small matrix right-multiplied by inverse failed\n";
                break;
            }
        }
        if (exit) {
            break;
        }
    }

    size_t n = 4;

    // Invert a scaled identity
    DenseMatrix<T> D1(n, n);
    D1.zero();
    for(size_t i=0; i<n; i++) {
        D1.at(i, i) = i+1;
    }
    DenseMatrix<T> D = D1;
    std::cout << "Diagonal matrix\n";
    D1.dump(std::cout);
    std::cout << "\n";
    
    DenseMatrix<T> ID(n, n);
    D1.factorAndInvert(ID);
    std::cout << "Inverted diagonal matrix\n";
    ID.dump(std::cout);
    std::cout << "\n";
    for(size_t i=0; i<n; i++) {
        if (std::abs(ID.at(i, i)-1.0/(i+1))>1e-12) {
            ok = false;
            std::cout << "Inverted diagonal matrix failed\n";
            break;
        }
    }

    // Matrix multiply
    DenseMatrix<T> PR(n, n);
    D.multiply(ID, PR);
    std::cout << "Diagonal matrix right-multiplied by inverse\n";
    PR.dump(std::cout);
    std::cout << "\n";
    for(size_t i=0; i<n; i++) {
        if (std::abs(PR.at(i, i)-1.0)>1e-12) {
            ok = false;
            std::cout << "Diagonal matrix right-multiplied by inverse failed\n";
            break;
        }
    }

    // Generic matrix multiplication
    DenseMatrix<T> A(n, n);
    for(size_t i=0; i<n; i++) {
        for(size_t j=0; j<n; j++) {
            A.at(i,j) = 1.0/(i+j+1);
        }
    }
    std::cout << "Generic matrix\n";
    A.dump(std::cout);
    std::cout << "\n";

    // Right-multiply by diagonal
    A.multiply(D, PR);
    std::cout << "Generic matrix right-multiplied by diagonal matrix\n";
    PR.dump(std::cout);
    std::cout << "\n";
    for(size_t i=0; i<n; i++) {
        for(size_t j=0; j<n; j++) {
            if (std::abs(PR.at(i,j)-A.at(i,j)*D.at(j,j))>1e-12) {
                ok = false;
                std::cout << "Generic matrix right-multiplied by diagonal matrix failed\n";
                break;
            }
        }
    }

    // Left-multiply by diagonal
    D.multiply(A, PR);
    std::cout << "Generic matrix left-multiplied by diagonal matrix\n";
    PR.dump(std::cout);
    std::cout << "\n";
    for(size_t i=0; i<n; i++) {
        for(size_t j=0; j<n; j++) {
            if (std::abs(PR.at(i,j)-A.at(i,j)*D.at(i,i))>1e-12) {
                ok = false;
                std::cout << "Generic matrix left-multiplied by diagonal matrix failed\n";
                break;
            }
        }
    }

    // Invert
    DenseMatrix<T> A1 = A;
    DenseMatrix<T> IA(n, n);
    if (!A1.factorAndInvert(IA)) {
        ok = false;
        std::cout << "Generic matrix inversion reported singular\n";
    }
    A.multiply(IA, PR);
    std::cout << "Generic matrix right-multiplied by inverse matrix\n";
    PR.dump(std::cout);
    std::cout << "\n";
    for(size_t i=0; i<n; i++) {
        for(size_t j=0; j<n; j++) {
            if (
                i==j && std::abs(PR.at(i,j)-1.0)>1e-12 ||
                i!=j && std::abs(PR.at(i,j))>1e-12    
            ) {
                ok = false;
                std::cout << "Generic matrix right-multiplied by inverse matrix failed\n";
                break;
            }
        }
    }


    // identity() on non-square matrices (exercises the row-major "read as
    // transpose" branch on a genuinely rectangular shape, not just square)
    {
        DenseMatrix<T> NS1(2, 3);
        NS1.identity();
        std::cout << "Identity on 2x3 row-major matrix\n";
        NS1.dump(std::cout);
        std::cout << "\n";
        bool localOk = true;
        for(size_t i=0; i<2; i++) {
            for(size_t j=0; j<3; j++) {
                T expected = (i==j) ? T(1) : T(0);
                if (std::abs(NS1.at(i,j)-expected)>1e-12) {
                    localOk = false;
                }
            }
        }
        if (!localOk) {
            ok = false;
            std::cout << "Identity on 2x3 row-major matrix failed\n";
        }
    }
    {
        DenseMatrix<T> NS2(3, 2);
        NS2.identity();
        std::cout << "Identity on 3x2 row-major matrix\n";
        NS2.dump(std::cout);
        std::cout << "\n";
        bool localOk = true;
        for(size_t i=0; i<3; i++) {
            for(size_t j=0; j<2; j++) {
                T expected = (i==j) ? T(1) : T(0);
                if (std::abs(NS2.at(i,j)-expected)>1e-12) {
                    localOk = false;
                }
            }
        }
        if (!localOk) {
            ok = false;
            std::cout << "Identity on 3x2 row-major matrix failed\n";
        }
    }

    // identity() on a padded/strided sub-block view (lda!=m), for both
    // dispatch branches - this is what a validating LAPACK backend or a
    // sign/branch-swap typo in identity()'s m/n/lda ternaries would catch
    // that a fully-packed square matrix test cannot. Padding is
    // sentinel-filled beforehand so any spill outside the logical block is
    // detected too.
    {
        size_t nb = 3;
        T sentinel = T(-99);

        // Native column-major branch (rowStride_==1, colStride_=lda>nRow_)
        DenseMatrix<T> BigCol(nb+2, nb+1, DenseMatrix<T>::Major::Column);
        BigCol = sentinel;
        DenseMatrixView<T> PadCol(BigCol.data().data(), nb, nb, 1, nb+2);
        PadCol.identity();
        std::cout << "Identity on padded column-major " << nb << "x" << nb << " sub-block (lda=" << (nb+2) << ")\n";
        BigCol.dump(std::cout);
        std::cout << "\n";
        bool localOk = true;
        for(size_t i=0; i<nb; i++) {
            for(size_t j=0; j<nb; j++) {
                T expected = (i==j) ? T(1) : T(0);
                if (std::abs(BigCol.at(i,j)-expected)>1e-12) {
                    localOk = false;
                }
            }
        }
        for(size_t j=0; j<nb; j++) {
            if (std::abs(BigCol.at(nb,j)-sentinel)>1e-12 || std::abs(BigCol.at(nb+1,j)-sentinel)>1e-12) {
                localOk = false;
            }
        }
        for(size_t i=0; i<nb+2; i++) {
            if (std::abs(BigCol.at(i,nb)-sentinel)>1e-12) {
                localOk = false;
            }
        }
        if (!localOk) {
            ok = false;
            std::cout << "Identity on padded column-major sub-block failed\n";
        }

        // Row-major "transpose" branch (colStride_==1, rowStride_=lda>nCol_)
        DenseMatrix<T> BigRow(nb+1, nb+2, DenseMatrix<T>::Major::Row);
        BigRow = sentinel;
        DenseMatrixView<T> PadRow(BigRow.data().data(), nb, nb, nb+2, 1);
        PadRow.identity();
        std::cout << "Identity on padded row-major " << nb << "x" << nb << " sub-block (lda=" << (nb+2) << ")\n";
        BigRow.dump(std::cout);
        std::cout << "\n";
        localOk = true;
        for(size_t i=0; i<nb; i++) {
            for(size_t j=0; j<nb; j++) {
                T expected = (i==j) ? T(1) : T(0);
                if (std::abs(BigRow.at(i,j)-expected)>1e-12) {
                    localOk = false;
                }
            }
        }
        for(size_t i=0; i<nb; i++) {
            if (std::abs(BigRow.at(i,nb)-sentinel)>1e-12 || std::abs(BigRow.at(i,nb+1)-sentinel)>1e-12) {
                localOk = false;
            }
        }
        for(size_t j=0; j<nb+2; j++) {
            if (std::abs(BigRow.at(nb,j)-sentinel)>1e-12) {
                localOk = false;
            }
        }
        if (!localOk) {
            ok = false;
            std::cout << "Identity on padded row-major sub-block failed\n";
        }
    }

    // New matrix-level helpers: view(), diagonal(), scaledMatrix(),
    // addScaledMatrix(), addMatrix(), subtractMatrix()
    {
        // view(): element-only copy through a DenseMatrixView (also
        // exercises the mismatched-major per-row fallback, since Src2 is
        // row-major and Dst2 is column-major)
        DenseMatrix<T> Src2({1,2,3,4,5,6}, 2, 3);
        DenseMatrix<T> Dst2(2, 3, DenseMatrix<T>::Major::Column);
        Dst2.view() = Src2;
        std::cout << "view() assignment\n";
        Dst2.dump(std::cout);
        std::cout << "\n";
        bool localOk = true;
        for(size_t i=0; i<2; i++) {
            for(size_t j=0; j<3; j++) {
                if (std::abs(Dst2.at(i,j)-Src2.at(i,j))>1e-12) {
                    localOk = false;
                }
            }
        }
        if (!localOk) {
            ok = false;
            std::cout << "view() assignment failed\n";
        }
    }
    {
        // diagonal(): rectangular matrix, set diagonal only, verify
        // off-diagonal untouched and diagonal length is min(nRow_,nCol_)
        DenseMatrix<T> M(3, 4);
        M.zero();
        M.diagonal() = T(5);
        std::cout << "diagonal() fill on 3x4 matrix\n";
        M.dump(std::cout);
        std::cout << "\n";
        bool localOk = true;
        for(size_t i=0; i<3; i++) {
            for(size_t j=0; j<4; j++) {
                T expected = (i==j) ? T(5) : T(0);
                if (std::abs(M.at(i,j)-expected)>1e-12) {
                    localOk = false;
                }
            }
        }
        if (!localOk) {
            ok = false;
            std::cout << "diagonal() fill failed\n";
        }
    }
    {
        // scaledMatrix(): this = other * factor
        DenseMatrix<T> Src3({1,2,3,4,5,6}, 2, 3);
        DenseMatrix<T> Dst3(2, 3);
        Dst3.scaledMatrix(Src3, T(2));
        std::cout << "scaledMatrix() (factor 2)\n";
        Dst3.dump(std::cout);
        std::cout << "\n";
        bool localOk = true;
        for(size_t i=0; i<2; i++) {
            for(size_t j=0; j<3; j++) {
                if (std::abs(Dst3.at(i,j)-Src3.at(i,j)*T(2))>1e-12) {
                    localOk = false;
                }
            }
        }
        if (!localOk) {
            ok = false;
            std::cout << "scaledMatrix() failed\n";
        }
    }
    {
        // addScaledMatrix(): this = this + other * factor
        DenseMatrix<T> Acc({10,20,30,40,50,60}, 2, 3);
        DenseMatrix<T> Src4({1,2,3,4,5,6}, 2, 3);
        DenseMatrix<T> AccOrig = Acc;
        Acc.addScaledMatrix(Src4, T(3));
        std::cout << "addScaledMatrix() (factor 3)\n";
        Acc.dump(std::cout);
        std::cout << "\n";
        bool localOk = true;
        for(size_t i=0; i<2; i++) {
            for(size_t j=0; j<3; j++) {
                if (std::abs(Acc.at(i,j)-(AccOrig.at(i,j)+Src4.at(i,j)*T(3)))>1e-12) {
                    localOk = false;
                }
            }
        }
        if (!localOk) {
            ok = false;
            std::cout << "addScaledMatrix() failed\n";
        }
    }
    {
        // addMatrix()/subtractMatrix(): round-trip should recover the original
        DenseMatrix<T> P({1,2,3,4}, 2, 2);
        DenseMatrix<T> Q({5,6,7,8}, 2, 2);
        DenseMatrix<T> POrig = P;
        P.addMatrix(Q);
        std::cout << "addMatrix()\n";
        P.dump(std::cout);
        std::cout << "\n";
        bool localOk = true;
        for(size_t i=0; i<2; i++) {
            for(size_t j=0; j<2; j++) {
                if (std::abs(P.at(i,j)-(POrig.at(i,j)+Q.at(i,j)))>1e-12) {
                    localOk = false;
                }
            }
        }
        P.subtractMatrix(Q);
        std::cout << "subtractMatrix() (should recover original)\n";
        P.dump(std::cout);
        std::cout << "\n";
        for(size_t i=0; i<2; i++) {
            for(size_t j=0; j<2; j++) {
                if (std::abs(P.at(i,j)-POrig.at(i,j))>1e-12) {
                    localOk = false;
                }
            }
        }
        if (!localOk) {
            ok = false;
            std::cout << "addMatrix()/subtractMatrix() failed\n";
        }
    }

    std::cout << "Dense matrix test " << (ok ? "OK" : "FAILED") << "\n";
    return ok;

    } // if constexpr
}

template class DenseMatrix<double>;
template class DenseMatrix<Complex>;
template class DenseMatrix<Int>;

}
