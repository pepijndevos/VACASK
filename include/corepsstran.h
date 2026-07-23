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
// After the shoot completes, phiCurrent_ already holds PhiT directly. PsiT
// (the period sensitivity dxT/dT, theory/pss.md "Computing Psi_T") is a
// property of the LAST accepted step only, not something to accumulate over
// the whole shoot - so it is computed once, on demand, by computePsiT(),
// instead of at every onTimestepAccepted() call (which would cost every
// step of the shoot an extra n-sized solve for a value nothing reads until
// the shoot is over). computePsiT() differentiates the last step's LMS
// coefficients with respect to its own step size using the generic,
// any-order/any-method formulae of theory/numint.md ("Sensitivity of
// a_{-1}, a_i, b_i to h_k"), applied to a fresh copy of TranCore's own
// integCoeffs member (getIntegCoeffs()) taken at that point - it still
// holds exactly the last accepted step's coefficients, untouched since
// onTimestepAccepted() returned for that step.
//
// Memory:
//   phiHist: at most p n×n dense matrices = O(p * n^2) doubles.
//   lastAlr_, scratchC_: two sparse matrices matching jacobian's sparsity.
//   qHist_, qDotHist_: fixed-capacity ring buffers of q_k, qdot_k, sized
//   once in rebuild() for the worst-case order - no per-step (re)allocation.

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
        PsiSensitivityFailed,
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
    bool clearTrajectory();

    // Populate preprocessedIc with all circuit unknowns from x0 so that
    // TranCore's UIC branch (nrSolver.setForces then solution = unknownValue_)
    // starts the shoot from x0 rather than zeros.
    // Must be called before run().
    void setShootIC(const Vector<double>& x0);

    // Is sensitivity information valid
    bool phiValid() { 
        if (!phiValid_) {
            setError(PssTranError::NoAcceptedSteps);
            return false;
        }    
        return true; 
    };

    // Return reference to current Phi
    DenseMatrix<double>& phiCurrent() { return phiCurrent_; };

    // Return reference to Psi_T. Valid only after computePsiT() has been
    // called following a converged shoot - unlike phiCurrent_, this is not
    // kept up to date at every accepted step.
    Vector<double>& psiCurrent() { return psiCurrent_; };

    // Compute Psi_T = dxT/dT (theory/pss.md, "Computing Psi_T") from the
    // last accepted step of the shoot. Must be called after the shoot has
    // finished (phiValid() true) and before the next clearTrajectory().
    bool computePsiT();

    // State x1 at the first accepted timepoint of the shoot (index 0 is the
    // ground bucket, same convention as the solution/x0 vectors). Valid after
    // the first onTimestepAccepted() call since the last clearTrajectory().
    // Used by PssCore to estimate the phase vector alpha ~= (x1-x0)/h0.
    const Vector<double>& firstStepX() const { return firstStepX_; };

    // Step size h0 of the first accepted step of the shoot (t1-t0).
    double firstStepH() const { return firstStepH_; };

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

    // Intermediate vector, holds one column of a C/G * Phi product before
    // it is scaled and accumulated into phiCurrent_.
    Vector<double> rhs_colbuf;

    // Current sensitivity matrix Phi(t), column-major n×n.
    // Initialised to I in clearTrajectory(), updated at every accepted step.
    // Equals PhiT after the shoot completes.
    DenseMatrix<double> phiCurrent_;

    // Current period sensitivity vector Psi(t)
    Vector<double> psiCurrent_;

    // State at the first accepted timepoint of the shoot, and its step size.
    // Captured once per shoot (see onTimestepAccepted()), reset by clearTrajectory().
    Vector<double> firstStepX_;
    double         firstStepH_ {0.0};

    // Circular history of past Phi matrices, depth <= p (BDF order).
    // phiHist_[0] = Phi at the previous accepted step, etc.
    // Stored column-major to match phiCurrent_.
    std::deque<DenseMatrix<double>> phiHist_;

    // Factored Alr = G + alpha*C from the most-recent accepted step.
    // Rebuilt in onTimestepAccepted(), reused in computePsiT() as J_N.
    // Same sparsity as jacobian.
    KluRealMatrix lastAlr_;

    // Scratch matrix for the unscaled reactive Jacobian C_k.
    // Filled by a single evalAndLoad pass in onTimestepAccepted().
    // Used to form the Phi RHS columns.  Same sparsity as jacobian.
    KluRealMatrix scratchC_;

    // Circular history of past reactive Jacobians C_k.
    std::deque<Vector<double>> cHistData_;

    // Circular history of past resistive Jacobians G_k = A_k - alpha_k * C_k.
    std::deque<Vector<double>> gHistData_;

    // Ring buffers of past reactive residuals q_k and their derivatives
    // qdot_k. Fixed capacity (worst-case order + 2, sized in rebuild()) so
    // onTimestepAccepted() can write q_{k+1}/qdot_{k+1} straight into the
    // buffers' own "future" slot (at(-1)) with no extra copy, then just
    // advance() - see onTimestepAccepted(). qHist_.at(0) / qDotHist_.at(0)
    // after the shoot's last call are q_N / qdot_N; at(i+1) is q_{N-1-i} /
    // qdot_{N-1-i} - exactly the history computePsiT() needs.
    VectorRepository<double> qHist_;
    VectorRepository<double> qDotHist_;

    // Step size h_k of the most-recent accepted step. TranCore's own
    // integCoeffs member (read via getIntegCoeffs()) stays valid and
    // unchanged for that same step from the moment onTimestepAccepted()
    // returns until the next run() - nothing recomputes it in between - so
    // computePsiT() can copy it fresh, once, right when it needs to mutate
    // it (computeSensitivities()/scaleDifferentiatorSensitivities()); no
    // per-step copy or dedicated member for the coefficients themselves is
    // needed, only this step size (IntegratorCoeffs has no hk() accessor).
    double lastStepH_ {0.0};

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
