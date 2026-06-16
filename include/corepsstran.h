#ifndef __COREPSSTRAN_DEFINED
#define __COREPSSTRAN_DEFINED

// corepsstran.h
//
// PssTranCore - TranCore subclass for PSS-SHOOT inline sensitivity integration.
//
// Reference:
//   T. Djurhuus and V. Krozer, "A Novel Phase-Noise Module for the
//   QUCS Circuit Simulator. Part I: the Periodic Steady-State",
//   arXiv:2512.10373v1, December 2025.
//
// The sensitivity matrix Phi(t) satisfies the linearised DAE:
//
//   C(t) * dPhi/dt + G(t) * Phi = 0,   Phi(t0) = I
//
// Discretised with the same BDF LMS scheme as the transient integrator:
//
//   Alr_k * Phi_k = C_k * sum_{i=0}^{p-1} asc[i] * Phi_{k-i-1}
//
// or in AM LMS scheme:
//
//   (C_k + h_k*b0*G_k) * Phi_k = C_{k-1} * Phi_{k-1} - h_k * sum_{i=1}^{p} (b_i/b0) * G_{k-i} * Phi_{k-i}
//
// Multiplying through by alpha_k = 1/(h_k*b0) = ic.leadingCoeff():
//
//   Alr_k * Phi_k = alpha_k * C_{k-1} * Phi_{k-1}
//                 - sum_{i=0}^{p-1} bsc[i] * G_{k-i-1} * Phi_{k-i-1}
//
// where bsc[i] = ic.bScaled()[i] = b_{i+1}/b0 (no h_k factor).
//
// where Alr_k = G_k + alpha_k * C_k is the already-evaluated transient
// Jacobian (identical to the NR Jacobian at the converged timestep).
//
// Integration strategy:
//
//   onTimestepAccepted() is called by TranCore at each accepted step with
//   jacobian holding the factored Alr_k = G_k + alpha_k * C_k.
//
//   1. Alr_k: copied from jacobian.data() directly — no re-evaluation.
//
//   2. C_k: evaluated with a single evalAndLoad pass (needed for the RHS).
//
//   3. Alr_k is refactored from the copied Ax values (klu_refactor reuses
//      the symbolic factorisation stored in lastAlr_ from its rebuild()).
//
//   4. RHS[:,j] = C_k * phiSum[:,j]  for j = 0..n-1, where
//      phiSum = sum_{si} asc[si] * phiHist[si].
//
//   5. Single block solve: lastAlr_ * PhiT_new = RHS  (klu_solve, nrhs=n).
//      RHS is column-major so KLU can solve all n columns in one call.
//
//   6. Rotate phiHist (circular buffer, depth <= p).
//
// After the shoot completes integrateSensitivity() returns phiCurrent_
// as PhiT and evaluates PsiT = alpha_last * Alr_last^{-1} * g(xT).
//
// Memory:
//   phiHist: at most p n×n dense matrices = O(p * n^2) doubles.
//   lastAlr_, scratchC_: two sparse matrices matching jacobian's sparsity.
//   No per-step trajectory (G, C, pastTimesteps snapshots) is stored.

#include "ansupport.h"
#include "coretran.h"
#include "densematrix.h"
#include "klumatrix.h"
#include "common.h"
#include <deque>

namespace NAMESPACE {

class PssTranCore : public TranCore {
public:
    enum class PssTranError {
        OK, 
        EvalCFailed, 
        AlrFactorizationFailed, 
        CPhiProductFailed, 
        GPhiProductFailed, 
        BlockAlrSolveFailed, 
        CPsiProductFailed, 
        GPsiProductFailed, 
        PsiSolveFailed, 
        NoAcceptedSteps, 
        NoTrajectory, 
        ScratchRebuild, 
        ScratchRefactorFailed, 
        CTProductFailed, 
        GTProductFailed, 
        TSolveBlockFailed, 
    };
    PssTranCore(
        OutputDescriptorResolver& parentResolver,
        TranParameters& params,
        OperatingPointCore& opCore,
        Circuit& circuit,
        CommonData& commons,
        KluRealMatrix& jacobian,
        VectorRepository<double>& opSolution,
        VectorRepository<double>& solution,
        VectorRepository<double>& states
    );

    PssTranCore           (const PssTranCore&)  = delete;
    PssTranCore           (      PssTranCore&&) = delete;
    PssTranCore& operator=(const PssTranCore&)  = delete;
    PssTranCore& operator=(      PssTranCore&&) = delete;

    // Format error, return false on error - this function is not cheap (works with strings)
    bool formatError(Status& s=Status::ignore) const; 

    bool initializeOutputs(Id name, Status& s = Status::ignore);

    // Hides TranCore::rebuild().  Calls TranCore::rebuild() then rebuilds
    // lastAlr_ and scratchC_ with the circuit sparsity pattern.
    bool rebuild(Status& s = Status::ignore);

    // Reset the sensitivity state before a new shooting iteration.
    // Sets phiCurrent_ = I and clears phiHist_.
    // Must be called by PssCore before each run().
    bool clearTrajectory(double T0);

    // Populate preprocessedIc with all circuit unknowns from x0 so that
    // TranCore's UIC branch (nrSolver.setForces then solution = unknownValue_)
    // starts the shoot from x0 rather than zeros.
    // Must be called before run().
    void setShootIC(const Vector<double>& x0);

    // Return the inline-computed PhiT (= phiCurrent_).
    // Must be called immediately after run() completes, while the circuit
    // state still holds xT and lastAlr_ holds the factored Alr at xT.
    bool integrateSensitivity(
        DenseMatrix<double>& PhiT
    );

    // Return the inline-computed PhiT (= phiCurrent_) and evaluate PsiT.
    // Must be called immediately after run() completes, while the circuit
    // state still holds xT and lastAlr_ holds the factored Alr at xT.
    bool integrateAugmentedSensitivity(
        DenseMatrix<double>& PhiT,
        Vector<double>&      PsiT,
        Vector<double>&      x_laststep
    );

    // Enable trajectory capture for the next shoot (call before final runShoot)
    void enableTrajectoryCapture();

    // Backwards-integrate the adjoint monodromy matrix
    bool integrateAdjointMonodromy(DenseMatrix<double>& Omega);

protected:
    // Called by TranCore at every accepted timestep with jacobian holding
    // the factored Alr_k = G_k + alpha_k * C_k from the NR solve.
    // Evaluates C_k, advances phiCurrent_ through one BDF LMS step using
    // a block solve, and rotates phiHist_.
    virtual bool onTimestepAccepted(double tSolve, double hk, Int order);

    // Clear error
    void clearError() { TranCore::clearError(); lastPssTranError = PssTranError::OK; }; 

    void setError(PssTranError e) { lastPssTranError = e; lastTranError = TranCore::TranError::OK; };
    PssTranError lastPssTranError;
    double pssErrorTime;
    UnknownIndex pssErrorColumn;
    Int pssErrorIndexK;
    Int pssErrorIndexI;

private:
    // Record of data needed for backward adjoint integration at each accepted step
    struct StepRecord {
        Vector<double> aData;    // raw A_k values (size nnz)
        Vector<double> cData;    // raw C_k values (size nnz)
        Vector<double> gData;    // G_k = A_k - alpha_k * C_k  (size nnz)
        Vector<double> gammaC;   // alpha * a[i]    — coefficient for C term (size order)
        Vector<double> gammaG;   // -(b[i] / b1)   — coefficient for G term (size order)
        Int            order;
    };


    // Current sensitivity matrix Phi(t), column-major n×n.
    // Initialised to I in clearTrajectory(), updated at every accepted step.
    // Equals PhiT after the shoot completes.
    DenseMatrix<double> phiCurrent_;

    // Current period sensitivity vector Psi(t)
    Vector<double> psiCurrent_;

    // Circular history of past Phi matrices, depth <= p (BDF order).
    // phiHist_[0] = Phi at the previous accepted step, etc.
    // Stored column-major to match phiCurrent_.
    std::deque<DenseMatrix<double>> phiHist_;

    // Circular history of past Psi matrices
    std::deque<Vector<double>> psiHist_;

    // Factored Alr = G + alpha*C from the most-recent accepted step.
    // Rebuilt in onTimestepAccepted(), reused in integrateSensitivity()
    // for the PsiT computation.  Same sparsity as jacobian.
    KluRealMatrix lastAlr_;

    // Scratch matrix for the unscaled reactive Jacobian C_k.
    // Filled by a single evalAndLoad pass in onTimestepAccepted().
    // Used to form the Phi RHS columns.  Same sparsity as jacobian.
    KluRealMatrix scratchC_;

    // Circular history of past reactive Jacobians C_k.
    std::deque<Vector<double>> cHistData_;

    // Circular history of past resistive Jacobians G_k = A_k - alpha_k * C_k.
    std::deque<Vector<double>> gHistData_;

    // Circular history of past reactive residuals q_k
    std::deque<Vector<double>> qHistData_;

    // Current period guess (needed for psi integration)
    double T0_;

    // Leading LMS coefficient alpha and leading quadrature weight b1
    // from the most-recent accepted step.  Together they give 1/h = alpha*b1,
    // which is the correct velocity scale for PsiT regardless of method.
    double lastAlpha_;
    double lastB1_;

    // True once onTimestepAccepted() has successfully processed at least
    // one step after the last clearTrajectory() call.
    bool phiValid_;

    // False until prevCData_ has been populated for Adams-Moulton.
    bool prevCValid_;

    // Trajectory buffer populated during final shoot for adjoint monodromy integration
    std::vector<StepRecord> trajectory_;

    // If true, gamma, C_k and A_k values are stored during the transient run. Needed for adjoint monodromy integration.
    bool captureTrajectory_;
};

} // namespace NAMESPACE

#endif // __COREPSSTRAN_DEFINED
