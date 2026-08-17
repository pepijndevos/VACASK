#include "coretrancoef.h"
#include "common.h"

namespace NAMESPACE {

IntegratorCoeffs::IntegratorCoeffs(Method method, Int order) 
    : method_(method), order_(order) {
    setOrder(order);
    b1_ = 0.0;
    err_ = 0.0;

    /*
    // Test solver
    matrix = DenseMatrix<double>({1, 2, 3, 2, 3, -5, -6, -8, 1}, 3, 3);
    rhs = {-7, 9, 22};
    solve(3);
    for(auto it : rhs) {
        std::cout << it*25 << " ";
    }
    std::cout << "\n";
    // Result should be: -393 217 72
    */ 
}

bool IntegratorCoeffs::setMethod(Method method, Int order, double xmu) {
    method_ = method;
    order_ = order;
    xmu_ = xmu;
    size();
    return xmu>=0 && xmu<=0.5;
}

bool IntegratorCoeffs::setOrder(Int order) {
    order_ = order;
    size();
    return true;
}

bool IntegratorCoeffs::size() {
    // Compute sizes of arrays
    switch (method_) {
    case Method::AdamsMoulton:
        implicit_ = true;
        numX_ = 1;
        numXdot_ = order_ - 1;
        // numX_ a coeffs + 1 (b_{-1}) + numXdot_ b coeffs
        // Don't need equation for a_0, because a_0 = 1.0
        n_ = numXdot_ + 1; 
        break;
    case Method::BDF:
        implicit_ = true;
        numX_ = order_;
        numXdot_ = 0;
        // numX_ a coeffs + 1 (b_{-1})
        n_ = numX_ + 1;
        break;
    case Method::AdamsBashforth:
        implicit_ = false;
        numX_ = 1;
        numXdot_ = order_;
        // numXdot_ b coeffs
        // Don't need equation for a_0, because a_0 = 1.0
        n_ = numXdot_;
        break;
    case Method::PolynomialExtrapolation:
        implicit_ = false;
        numX_ = order_ + 1;
        numXdot_ = 0;
        // numX_ a coeffs
        n_ = numX_;
        break;
    }
    return true;
}

bool IntegratorCoeffs::compute(CircularBuffer<double>& pastSteps, double newStep) {
    pastSteps_ = &pastSteps;
    // Compute sizes of arrays
    switch (method_) {
    case Method::AdamsMoulton:
        implicit_ = true;
        numX_ = 1;
        numXdot_ = order_ - 1;
        // numX_ a coeffs + 1 (b_{-1}) + numXdot_ b coeffs
        // Don't need equation for a_0, because a_0 = 1.0
        n_ = numXdot_ + 1; 
        break;
    case Method::BDF:
        implicit_ = true;
        numX_ = order_;
        numXdot_ = 0;
        // numX_ a coeffs + 1 (b_{-1})
        n_ = numX_ + 1;
        break;
    case Method::AdamsBashforth:
        implicit_ = false;
        numX_ = 1;
        numXdot_ = order_;
        // numXdot_ b coeffs
        // Don't need equation for a_0, because a_0 = 1.0
        n_ = numXdot_;
        break;
    case Method::PolynomialExtrapolation:
        implicit_ = false;
        numX_ = order_ + 1;
        numXdot_ = 0;
        // numX_ a coeffs
        n_ = numX_;
        break;
    }

    // Unknowns order: a_0, ..., a_{numx-1}
    //                 b_{-1}
    //                 b_0, ..., b_{numxdot-1}
    matrix.resize(n_, n_, DenseMatrix<double>::Major::Column);
    rhs.resize(n_);

    // Prepare space for coeffs
    a_.resize(numX_);
    b_.resize(numXdot_);
    
    // Coeffs are 0.0 by default
    a_.assign(numX_, 0.0);
    b1_ = 0.0;
    b_.assign(numXdot_, 0.0);

    // All matrix and RHS entries will be set, so there is no need to set them to 0
    DBGCHECK(pastSteps.valueCount()+1<numX_ || pastSteps.valueCount()+1<numXdot_, "Timestep history is too short.");
    
    // Compute past timepoints, index 0 is timepoint 0.0 (last computed solution)
    normalizedTimePoint.resize(pastSteps.valueCount()+1);
    normalizedTimePoint[0] = 0.0;
    for(Int i=0;i<pastSteps.valueCount();i++) {
        normalizedTimePoint[i+1] = normalizedTimePoint[i]-pastSteps.at(i);
    }

    // Normalize by newStep
    for(Int i=1; i<normalizedTimePoint.size(); i++) {
        normalizedTimePoint[i] /= newStep;
    }

    // System of equations
    // 0:
    //   sum_{i=0}^{numX-1} a_i = 1
    // j = 1..n:
    //   sum_{i=0}^{numX-1} (t_{k-i}/h_k)^j a_i + sum_{i=-1}^{numXdot-1} j (t_{k-i}/h_k)^(j-1) b_i = 1
    // after setting t_k = 0
    //   sum_{i=1}^{numX-1} (t_{k-i}/h_k)^j a_i + sum_{i=-1}^{numXdot-1} j (t_{k-i}/h_k)^(j-1) b_i = 1
    // make sure (t_{k}/h_k)^(j-1) for k=0, j=1 is 1
    switch (method_) {
    case Method::AdamsMoulton:
        // Handle single step methods without equations (constant coeffs)
        // First equation is always a_0 = 1.0, so we don't need it
        a_[0] = 1.0;
        switch (order_) {
        case 1:
            // Backward Euler
            b1_ = 1.0;
            break;
        case 2:
            // Trapezoidal
            //
            {
                b_[0] = xmu_; // 0.5;
                b1_ = 1.0-xmu_; // 0.5;

                // xmu=0.5 - pure trapezoidal
                //   b_[0] = 0.5
                //   b1_   = 0.5
                //
                // xmu=0 - pure Euler
                //   b_[0] = 0
                //   b1_   = 1
            }
            break;
        default:
            // Multistep Adams-Moulton methods
            // Fill matrix and RHS
            // t_{k}=0.0, a_0 term vanishes
            // j = 1..order
            //   sum_{i=-1}^{numXdot-1} j (t_{k-i}/h_k)^(j-1) b_i = 1
            // Unknowns: b_{-1}, b_0, b_1, ...
            for(Int j=1; j<=order_; j++) {
                auto row = matrix.row(j-1);
                // Manually add b_{-1}
                row[0] = j;
                // b coeffs
                for(Int i=0; i<numXdot_; i++) {
                    // Treat b_0 differently for j=1
                    if (i==0 && j==1) {
                        row[1+i] = j;
                    } else {
                        row[1+i] = j*pow(normalizedTimePoint[i], j-1);
                    }
                }
                rhs[j-1] = 1.0;
            }
            
            // Solve 
            if (!solve(n_)) {
                return false;
            }

            // Unpack
            b1_ = rhs[0];
            for(Int i=1; i<=numXdot_; i++) {
                b_[i-1] = rhs[i];
            }
            break;
        }
        break;

    case Method::BDF:
        // Handle single step methods without equations (constant coeffs)
        switch (order_) {
        case 1:
            // Backward Euler
            a_[0] = 1.0;
            b1_ = 1.0;
            break;
        default: 
            // Multistep BDF methods
            // Fill matrix and RHS
            // 0:
            //   sum_{i=0}^{numX-1} a_i = 1
            // j = 1..order
            //   sum_{i=1}^{numX-1} (t_{k-i}/h_k)^j a_i + j (t_{k+1}/h_k)^(j-1) b_{-1} = 1
            // Unknowns: a_0, a_1, ..., a_{numX-1}, b_{-1}
            // First equation
            auto row0 = matrix.row(0);
            for(Int i=0; i<numX_; i++) {
                row0[i] = 1.0;
            }
            row0[numX_] = 0.0;
            rhs[0] = 1.0;
            // Remaining order_ equations
            for(Int j=1; j<=order_; j++) {
                auto row = matrix.row(j);
                // a coeffs
                row[0] = 0;
                for(Int i=1; i<numX_; i++) {
                    row[i] = pow(normalizedTimePoint[i], j);
                }
                // b_{-1} coeff
                row[numX_] = j;
                // RHS
                rhs[j] = 1.0;
            }

            // Solve
            if (!solve(n_)) {
                return false;
            }
            
            // Unpack
            for(Int i=0; i<numX_; i++) {
                a_[i] = rhs[i]; 
            }
            b1_ = rhs[numX_];
            break;
        }
        break;

    case Method::AdamsBashforth:
        // Handle single step methods without equations (constant coeffs)
        // First equation is always a_0 = 1.0, so we don't need it
        a_[0] = 1.0;
        switch (order_) {
        case 1:
            // Forward Euler
            b_[0] = 1.0;
            break;
        default:
            // Multistep Adams-Bashforth methods
            // Fill matrix and RHS
            // t_{k}=0.0, a_0 term vanishes
            // j = 1..order:
            //   sum_{i=0}^{numXdot-1} j (t_{k-i}/h_k)^(j-1) b_i = 1
            // Unknowns: b_0, b_1, ...
            for(Int j=1; j<=order_; j++) {
                auto row = matrix.row(j-1);
                // b coeffs
                for(Int i=0; i<numXdot_; i++) {
                    // Treat b_0 differently for j=1
                    if (i==0 && j==1) {
                        row[i] = j;
                    } else {
                        row[i] = j*pow(normalizedTimePoint[i], j-1);
                    }
                }
                // RHS
                rhs[j-1] = 1.0;
            }

            // Solve
            if (!solve(n_)) {
                return false;
            }
            
            // Unpack
            for(Int i=0; i<numXdot_; i++) {
                b_[i] = rhs[i];
            }
            break;
        }
        break;
    
    case Method::PolynomialExtrapolation:
        // Multistep Adams-Bashforth methods
        // Fill matrix and RHS
        // 0:
        //   sum_{i=0}^{numX-1} a_i = 1
        // j = 1..order:
        //   sum_{i=1}^{numX-1} (t_{k-i}/h_k)^j a_i = 1
        // Unknowns: a_0, a_1, ...
        // First equation
        auto row0 = matrix.row(0);
        for(Int i=0; i<numX_; i++) {
            row0[i] = 1.0;
        }
        rhs[0] = 1.0;
        // Remaining order_ equations
        for(Int j=1; j<=order_; j++) {
            auto row = matrix.row(j);
            // a coeffs
            for(Int i=0; i<numX_; i++) {
                row[i] = pow(normalizedTimePoint[i], j);
            }
            // RHS
            rhs[j] = 1.0;
        }

        // Solve
        if (!solve(n_)) {
            return false;
        }

        // Unpack
        for(Int i=0; i<numX_; i++) {
            a_[i] = rhs[i];
        }
        break;
    }
    
    // Compute error coefficient without the j! in denominator
    // For a method of order j-1 LTE is determined by coefficient 
    // 
    //        1         numX-1      t_{k-i} - t_k        numXdot-1  t_{k-i} - t_k
    // C_j = --- ( -1 +  sum   a_i (-------------)^j + j   sum     (-------------)^(j-1) )
    //       j!          i=0             h_k               i=-1           h_k
    //
    // LTE at t_{k+1} = C_j x^(j)(t_k) h_k^j
    // 
    // x^(j)(t) is the j-th derivative of x(t) wrt. t
    err_ = -1.0;
    for(Int i=0; i<numX_; i++) {
        err_ += a_[i]*std::pow(normalizedTimePoint[i], order_+1);
    }
    for(Int i=0; i<numXdot_; i++) {
        err_ += (order_+1)*b_[i]*std::pow(normalizedTimePoint[i], order_);
    }
    err_ += (order_+1)*b1_;

    return true;
}

bool IntegratorCoeffs::computeSensitivities(double newStep) {
    // normalizedTimePoint (theta_i) is reused as-is from the preceding
    // compute() call for this newStep - not recomputed here, so this can be
    // called without access to the pastSteps history compute() used (the
    // caller may no longer have it in the right shape, e.g. once a new step
    // has since been appended to a shared history buffer).

    // Coeffs are 0.0 by default
    aSens_.assign(numX_, 0.0);
    b1Sens_ = 0.0;
    bSens_.assign(numXdot_, 0.0);

    rhs.resize(n_);

    // Differentiating the order conditions wrt hk (past step sizes held
    // fixed) yields a linear system for the primed coefficients with the
    // exact same matrix as compute() - only the RHS changes. See
    // theory/numint.md, "Sensitivity of the coefficients to h_k". compute()
    // must already have been called for this (pastSteps, newStep): wherever
    // it solved a linear system, matrix/rowPerm_ still hold the LU
    // decomposition from that solve() call, reused here via luSolve() with
    // only the RHS rebuilt - no new factorization, matrix is never rebuilt.
    // Wherever compute() instead took a constant-coefficient shortcut
    // (backward Euler, trapezoidal, BDF1, forward Euler), those coefficients
    // don't depend on hk at all, so the sensitivities are zero - already the
    // default set above - and there is no factorization to reuse.
    switch (method_) {
    case Method::AdamsMoulton:
        switch (order_) {
        case 1:
        case 2:
            // Backward Euler / trapezoidal: constant coefficients
            break;
        default: {
            // Unknowns: b_{-1}', b_0', ..., b_{numXdot_-1}'
            for(Int j=1; j<=order_; j++) {
                double s = 0.0;
                for(Int i=1; i<numXdot_; i++) {
                    s += b_[i]*pow(normalizedTimePoint[i], j-1);
                }
                rhs[j-1] = (j/newStep)*(j-1)*s;
            }

            if (!solveSensitivity()) {
                return false;
            }

            b1Sens_ = rhs[0];
            for(Int i=1; i<=numXdot_; i++) {
                bSens_[i-1] = rhs[i];
            }
            break;
        }
        }
        break;

    case Method::BDF:
        switch (order_) {
        case 1:
            // Backward Euler: constant coefficients
            break;
        default: {
            // Unknowns: a_0', a_1', ..., a_{numX_-1}', b_{-1}'
            rhs[0] = 0.0;
            for(Int j=1; j<=order_; j++) {
                // BDF has no b_i for i>=0, so the b-part of the RHS is always empty
                double s = 0.0;
                for(Int i=1; i<numX_; i++) {
                    s += a_[i]*pow(normalizedTimePoint[i], j);
                }
                rhs[j] = (j/newStep)*s;
            }

            if (!solveSensitivity()) {
                return false;
            }

            for(Int i=0; i<numX_; i++) {
                aSens_[i] = rhs[i];
            }
            b1Sens_ = rhs[numX_];
            break;
        }
        }
        break;

    case Method::AdamsBashforth:
        switch (order_) {
        case 1:
            // Forward Euler: constant coefficient
            break;
        default: {
            // Unknowns: b_0', b_1', ..., b_{numXdot_-1}'
            for(Int j=1; j<=order_; j++) {
                double s = 0.0;
                for(Int i=1; i<numXdot_; i++) {
                    s += b_[i]*pow(normalizedTimePoint[i], j-1);
                }
                rhs[j-1] = (j/newStep)*(j-1)*s;
            }

            if (!solveSensitivity()) {
                return false;
            }

            for(Int i=0; i<numXdot_; i++) {
                bSens_[i] = rhs[i];
            }
            break;
        }
        }
        break;

    case Method::PolynomialExtrapolation: {
        // No constant-coefficient shortcut for this method; always solved
        // Unknowns: a_0', a_1', ..., a_{numX_-1}'
        rhs[0] = 0.0;
        for(Int j=1; j<=order_; j++) {
            double s = 0.0;
            for(Int i=1; i<numX_; i++) {
                s += a_[i]*pow(normalizedTimePoint[i], j);
            }
            rhs[j] = (j/newStep)*s;
        }

        if (!solveSensitivity()) {
            return false;
        }

        for(Int i=0; i<numX_; i++) {
            aSens_[i] = rhs[i];
        }
        break;
    }
    }

    return true;
}

bool IntegratorCoeffs::solve(Int n) {
    rowPerm_.resize(n);
    VectorView rowPermView(rowPerm_);
    if (!matrix.factor(rowPermView)) {
        return false;
    }
    auto vv = VectorView(rhs.data(), n, 1);
    return matrix.luSolve(vv, rowPermView);
}

bool IntegratorCoeffs::solveSensitivity() {
    VectorView rowPermView(rowPerm_);
    auto vv = VectorView(rhs.data(), n_, 1);
    return matrix.luSolve(vv, rowPermView);
}

bool IntegratorCoeffs::scaleDifferentiator(double hk) {
    if (hk==0.0) {
        return false;
    }
    hk_ = hk;
    aScaled_ = a_;
    bScaled_ = b_;
    
    // Scale only implicit algorithm coeffs
    leading_ = 1 / (hk*b1_);
    for(auto& aIt : aScaled_) {
        aIt *= -leading_;
    }
    for(auto& bIt : bScaled_) {
        bIt /= -b1_;
    }

    return true;
}

bool IntegratorCoeffs::scaleDifferentiatorSensitivities(double hk) {
    if (hk==0.0) {
        return false;
    }
    aScaledSens_ = aSens_;
    bScaledSens_ = bSens_;

    // Common factor 1/hk + bbar_{-1}'/bbar_{-1}, shared by a_{-1}' and a_i'
    // (theory/numint.md, "Sensitivity of a_{-1}, a_i, b_i to h_k")
    double leadingLocal = 1 / (hk*b1_);
    double factor = 1/hk + b1Sens_/b1_;

    leadingSens_ = -leadingLocal*factor;
    for(Int i=0; i<aScaledSens_.size(); i++) {
        aScaledSens_[i] = leadingLocal*(a_[i]*factor - aSens_[i]);
    }
    for(Int i=0; i<bScaledSens_.size(); i++) {
        bScaledSens_[i] = (b_[i]*(b1Sens_/b1_) - bSens_[i]) / b1_;
    }

    return true;
}

bool IntegratorCoeffs::scalePredictor(double hk) {
    if (implicit_) {
        return false;
    }
    aScaled_ = a_;
    bScaled_ = b_;
    
    // Scale only implicit algorithm coeffs
    for(auto& bIt : bScaled_) {
        bIt *= hk;
    }

    return true;
}
 
void IntegratorCoeffs::dump(std::ostream& os, bool scaled) {
    switch (method_) {
        case Method::AdamsMoulton:
            os << "AM ";
            break;
        case Method::AdamsBashforth:
            os << "AB ";
            break;
        case Method::BDF:
            os << "BDF ";
            break;
        case Method::PolynomialExtrapolation:
            os << "Poly ";
            break;
    }
    for(Int i=0; i<numX_; i++) {
        if (scaled) {
            os << "a" << i << "/(hk b_{-1})=" << aScaled_[i] << " ";
        } else {
            os << "a" << i << "=" << a_[i] << " ";
        }
    }
    if (implicit_) {
        if (scaled) {
            os << "1/(hk b_{-1})=" << leading_ << " ";
        } else {
            os << "b_{-1}=" << b1_ << " ";
        }
    }
    for(Int i=0; i<numXdot_; i++) {
        if (scaled) {
            os << "b" << i << "/b_{-1}=" << bScaled_[i] << " ";
        } else {
            os << "b" << i << "=" << b_[i] << " ";
        }
    }
}

bool IntegratorCoeffs::test() {
    // Object
    IntegratorCoeffs ic;

    // Test outcome
    bool ok = true;

    // Expected values are for order=3
    int order = 3;

    // Past steps
    CircularBuffer<double> pastSteps(order);
    for(int i=0; i<order; i++) {
        pastSteps.add(0.1);
    }
    
    // AM3, uniform step
    ic.setMethod(Method::AdamsMoulton, order);
    if (ic.compute(pastSteps, 0.1)) {
        ic.dump(std::cout);
        auto errExpect = (1.0/24)*ffactorial(order+1);
        std::cout << " C=" << ic.err_ << "\n";
        std::cout << "Expected: " << 1.0 << " " << (5.0/12) << " " << (8.0/12) << " " << (-1.0/12) 
                << " " << errExpect << "\n";
        std::cout << "\n";
        if (std::abs(ic.err_-errExpect)>1e-12) {
            ok = false;
            std::cout << "AM failed\n";
        }
    } else {
        std::cout << "AM failed\n";
        ok = false;
    }

    // BDF3, uniform step
    ic.setMethod(Method::BDF, order);
    if (ic.compute(pastSteps, 0.1)) {
        ic.dump(std::cout);
        auto errExpect = (3.0/22)*ffactorial(order+1);
        std::cout << " C=" << ic.err_ << "\n";
        std::cout << "Expected: " << " " << (18.0/11) << " " << (-9.0/11) << " " << (2.0/11) << " " << (6.0/11) 
                << " " << errExpect << "\n";
        std::cout << "\n";
        if (std::abs(ic.err_-errExpect)>1e-12) {
            ok = false;
            std::cout << "BDF failed\n";
        }
    } else {
        std::cout << "BDF failed\n";
        ok = false;
    }
    
    // AB3, uniform step
    ic.setMethod(Method::AdamsBashforth, order);
    if (ic.compute(pastSteps, 0.1)) {
        ic.dump(std::cout);
        auto errExpect = (-3.0/8)*ffactorial(order+1);
        std::cout << " C=" << ic.err_ << "\n";
        std::cout << "Expected: " << " " << (1) << " " << (23.0/12) << " " << (-16.0/12) << " " << (5.0/12) 
                << " " << errExpect << "\n";
        std::cout << "\n";
        if (std::abs(ic.err_-errExpect)>1e-12) {
            ok = false;
            std::cout << "AB failed\n";
        }
    } else {
        std::cout << "AB failed\n";
        ok = false;
    }

    // Polynomial extrapolation
    ic.setMethod(Method::PolynomialExtrapolation, order);
    if (ic.compute(pastSteps, 0.1)) {
        ic.dump(std::cout);
        auto errExpect = (-1.0)*ffactorial(order+1);
        std::cout << " C=" << ic.err_ << "\n";
        std::cout << "Expected: " << " " << (4) << " " << (-6) << " " << (4) << " " << (-1) 
                << " " << errExpect << "\n";
        std::cout << "\n";
        if (std::abs(ic.err_-errExpect)>1e-12) {
            ok = false;
            std::cout << "Polynomial extrapolation failed\n";
        }
    } else {
        std::cout << "Polynomial extrapolation failed\n";
        ok = false;
    }
    // Sensitivities of the scaled differentiator coefficients (a_{-1}, a_i,
    // b_i) to hk, verified against central finite differences of
    // scaleDifferentiator() itself (past step sizes held fixed). Implicit
    // methods only - scaleDifferentiator() divides by b1_, which is 0 for
    // explicit methods.
    auto checkDiffSens = [&](Method method, Int ord, const char* label) {
        CircularBuffer<double> steps(ord);
        for(int i=0; i<ord; i++) {
            steps.add(0.1);
        }
        double hk = 0.1;
        double eps = 1e-6;

        IntegratorCoeffs c, cP, cM;
        c.setMethod(method, ord);
        cP.setMethod(method, ord);
        cM.setMethod(method, ord);

        if (!c.compute(steps, hk) || !c.computeSensitivities(hk) ||
            !c.scaleDifferentiator(hk) || !c.scaleDifferentiatorSensitivities(hk)) {
            std::cout << label << " sensitivity test: compute FAILED\n";
            ok = false;
            return;
        }
        cP.compute(steps, hk+eps);
        cP.scaleDifferentiator(hk+eps);
        cM.compute(steps, hk-eps);
        cM.scaleDifferentiator(hk-eps);

        double maxDiff = 0;
        for(Int i=0; i<c.aScaledSens().size(); i++) {
            double fd = (cP.aScaled()[i]-cM.aScaled()[i])/(2*eps);
            maxDiff = std::max(maxDiff, std::abs(fd-c.aScaledSens()[i]));
        }
        for(Int i=0; i<c.bScaledSens().size(); i++) {
            double fd = (cP.bScaled()[i]-cM.bScaled()[i])/(2*eps);
            maxDiff = std::max(maxDiff, std::abs(fd-c.bScaledSens()[i]));
        }
        {
            double fd = (cP.leadingCoeff()-cM.leadingCoeff())/(2*eps);
            maxDiff = std::max(maxDiff, std::abs(fd-c.leadingCoeffSens()));
        }

        std::cout << label << " scaled differentiator sensitivity maxDiff vs FD = " << maxDiff << "\n";
        if (maxDiff > 1e-5) {
            ok = false;
            std::cout << label << " sensitivity FAILED\n";
        }
    };

    checkDiffSens(Method::AdamsMoulton, 1, "AM1 (Backward Euler)");
    checkDiffSens(Method::AdamsMoulton, 2, "AM2 (Trapezoidal)");
    checkDiffSens(Method::AdamsMoulton, 3, "AM3");
    checkDiffSens(Method::BDF, 1, "BDF1");
    checkDiffSens(Method::BDF, 2, "BDF2");
    checkDiffSens(Method::BDF, 3, "BDF3");

    std::cout << "Integrator coeffs test " << (ok ? "OK" : "FAILED") << "\n";
    return ok;
}

}
