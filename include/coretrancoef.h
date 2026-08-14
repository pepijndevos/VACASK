#ifndef __CORETRANCOEF_DEFINED
#define __CORETRANCOEF_DEFINED

#include "value.h"
#include "ansupport.h"
#include "densematrix.h"
#include "common.h"
#include <algorithm>


namespace NAMESPACE {

class IntegratorCoeffs {
public:
    enum class Method { AdamsMoulton, BDF, AdamsBashforth, PolynomialExtrapolation };

    IntegratorCoeffs(Method method=Method::AdamsMoulton, Int order=1); 

    // Method
    Method method() const { return method_; };
    
    // Order
    Int order() const { return order_; };

    // xmu for trapezoidal algorithm
    double xmu() const { return xmu_; }; 

    // Change method
    bool setMethod(Method method, Int order, double xmu=0.5);

    // Change order
    bool setOrder(Int order);

    // Set xmu for trapezoidal algorithm
    bool setXmu(double xmu=0.5) { xmu_ = xmu; return xmu>=0 && xmu<=0.5; };

    // Compute coefficients
    bool compute(CircularBuffer<double>& pastSteps, double newStep);

    // Compute a_, b_, and b1_ coefficient sensitivities to newStep
    // compute() must already have been called for this newStep (with
    // whatever pastSteps it used) - reuses its normalizedTimePoint, and,
    // where compute() solved a linear system, its LU factorization too
    bool computeSensitivities(double newStep);

    // Coefficients for past values
    const std::vector<double>& a() const { return a_; };

    // Coefficients for past derivatives
    const std::vector<double>& b() const { return b_; };

    // Coefficient for new derivative
    double b1() const { return b1_; };

    // Sensitivities of a_ to newStep (past step sizes held fixed)
    const std::vector<double>& aSens() const { return aSens_; };

    // Sensitivities of b_ to newStep (past step sizes held fixed)
    const std::vector<double>& bSens() const { return bSens_; };

    // Sensitivity of b1_ to newStep (past step sizes held fixed)
    double b1Sens() const { return b1Sens_; };

    // Compute scaled coefficients
    bool scaleDifferentiator(double hk);
    bool scalePredictor(double hk);

    // Compute sensitivities of the scaled differentiator coefficients to hk
    // Requires a_, b_, b1_ and their sensitivities (aSens_, bSens_, b1Sens_)
    // to already have been computed for this hk, via compute() and
    // computeSensitivities()
    bool scaleDifferentiatorSensitivities(double hk);

    // Scaled coefficients
    const std::vector<double> aScaled() const { return aScaled_; };
    const std::vector<double> bScaled() const { return bScaled_; };
    double leadingCoeff() const { return leading_; };

    // Sensitivities of the scaled differentiator coefficients to hk
    const std::vector<double>& aScaledSens() const { return aScaledSens_; };
    const std::vector<double>& bScaledSens() const { return bScaledSens_; };
    double leadingCoeffSens() const { return leadingSens_; };

    // Minimal number of past points needed by the predictor
    size_t minimalPredictorHistory() {
        return numX_;
    };

    // Minimal number of past points needed by the differentiator
    size_t minimalDifferentiatorHistory() {
        return std::max(numX_, numXdot_);
    };

    // Prepare fast array pointers on which predict() will operate
    bool preparePredictorHistory(VectorRepository<double>& repo, Int historyOffset=1) {
        DBGCHECK(numXdot_>0, "Predictors using past derivatives are not supported.");
        predictorHistory.clear();
        auto n = minimalPredictorHistory();
        for(decltype(n) i=0; i<n; i++) {
            predictorHistory.push_back(repo.data(historyOffset+i));
        }
        return true;
    };

    // Prepare fast array pointers on which differentiate() will operate
    // Also prepare fast pointer to future states
    bool prepareDifferentiatorHistory(VectorRepository<double>& repo, Int historyOffset=1) {
        DBGCHECK(!implicit_, "Explicit algorithms cannot be used for computing future derivative.");
        differentiatorHistory.clear();
        auto n = minimalDifferentiatorHistory();
        for(decltype(n) i=0; i<n; i++) {
            differentiatorHistory.push_back(repo.data(historyOffset+i));
        }
        return true;
    };

    // Differentiate state at tk+hk based on
    // - future value
    // - value history (state) and
    // - derivative history (state+1)
    // To be used with implicit integration algorithms
    double differentiate(double futureValue, GlobalStorageIndex state) {
        // Contribution of future value
        double deriv = leading_ * futureValue;
        // Contribution of past values
        for(Int i=0; i<aScaled_.size(); i++) {
            deriv += aScaled_[i] * differentiatorHistory[i][state];
        }
        // Contribution of past derivatives
        for(Int i=0; i<bScaled_.size(); i++) {
            deriv += bScaled_[i] * differentiatorHistory[i][state+1];
        }
        return deriv;
    };

    // Aggregated-vector variant of differentiate(): same derivative-form
    // reconstruction (numint.md, "Derivative at the new timepoint"), but
    // operating directly on a pair of history ring buffers instead of via
    // GlobalStorageIndex pointers into a device-level states repository.
    // qHist.at(-1) is read as the future value (already written by the
    // caller); qHist.at(0..) / qDotHist.at(0..) are read as past value /
    // past derivative history. Writes straight into 'out' - pass
    // qDotHist.at(-1) to fill the qdot ring buffer's own future slot with
    // no extra copy, then advance() both buffers.
    void differentiate(
        const CircularBuffer<Vector<double>>& qHist,
        const CircularBuffer<Vector<double>>& qDotHist,
        Vector<double>& out
    ) const {
        const Vector<double>& future = qHist.at(-1);
        auto len = future.size();
        out.assign(len, 0.0);
        for(size_t u=0; u<len; u++) {
            double deriv = leading_ * future[u];
            for(Int i=0; i<aScaled_.size(); i++) {
                deriv += aScaled_[i] * qHist.at(i)[u];
            }
            for(Int i=0; i<bScaled_.size(); i++) {
                deriv += bScaled_[i] * qDotHist.at(i)[u];
            }
            out[u] = deriv;
        }
    };

    // Predict value based on value history
    // To be used with explicit algorithms (predictors)
    // No need to zero prediction before this function is called
    void predict(Vector<double>& prediction) {
        auto n = prediction.size();
        VectorView<double> pv(prediction);
        pv.writeScaled(VectorView<double>(predictorHistory[0], n, 1), aScaled_[0]);
        for(Int j=1; j<a_.size(); j++) {
            pv.addScaled(VectorView<double>(predictorHistory[j], n, 1), aScaled_[j]);
        }
    };

    // Error coefficient multiplied by (order+1)!
    // Note that all return values remain reasonable, 
    // except for polynomial extrapolation where they grow with (order+1)!
    double errorCoeff() const { return err_; };

    // Required history length
    Int pastStatesNeeded() const { return std::max(numX_, numXdot_); }

    void dump(std::ostream& os, bool scaled=false);

    static double ffactorial(int n) {
        static std::vector<double> cache = { 1, 1, 2, 6, 24, 120, 720, 5040, 40320 };
        if (n>=cache.size()) {
            for(int i=cache.size(); i<=n; i++) {
                cache.push_back(cache[i-1]*i);
            }
        }
        return cache[n];
    };

    static bool test();
    
private:
    bool size();
    bool solve(Int n);
    bool solveSensitivity();

    Method method_;
    Int order_;
    double xmu_;
    Int numX_;
    Int numXdot_;
    bool implicit_;
    bool multistep_;
    
    // Number of equations, matrix (ordered by rows), and RHS
    Int n_;
    DenseMatrix<double> matrix; // row1, row2, ... - holds the LU decomposition after solve()
    std::vector<double> rhs;
    std::vector<int> rowPerm_; // Row permutation from factor(), valid after solve() (LAPACK ipiv storage type)
    
    // New timepoint: 
    //   t_{k+1} = h_k 
    // Old timepoints:
    //   0: t_k = 0.0
    //   1: t_{k-i} = h_{k-1} + h_{k-2} + ... + h_{k-i}
    //   ...

    // Old timepoints, normalized by h_k
    //   t_k/h_k, t_{k-1}/h_k, t_{k-2}/h_k, ...
    std::vector<double> normalizedTimePoint; 

    // Computed coefficients
    std::vector<double> a_; // Coeffs for x(t_{k-i}), i>=0
    std::vector<double> b_; // Coeffs for xdot(t_{k-i}), i>=0
    double b1_;             // Coeff for xdot(t_{k+1})
    double err_;            // Error coefficient multiplied by (order+1)!

    // Sensitivities of a_, b_, b1_ to newStep (a_i', b_i', b_{-1}')
    std::vector<double> aSens_;
    std::vector<double> bSens_;
    double b1Sens_;
    std::vector<double> aScaled_; // a_ / (h_k b_{-1})
    std::vector<double> bScaled_; // b_ / b_{-1}
    double hk_;
    double leading_;

    // Sensitivities of aScaled_, bScaled_, leading_ to h_k (a_i', b_i', a_{-1}')
    std::vector<double> aScaledSens_;
    std::vector<double> bScaledSens_;
    double leadingSens_;

    CircularBuffer<double>* pastSteps_;

    // History - fast pointers
    std::vector<double*> predictorHistory;
    std::vector<double*> differentiatorHistory;
};

}

#endif
