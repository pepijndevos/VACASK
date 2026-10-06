#ifndef __COREHBNR_DEFINED
#define __COREHBNR_DEFINED

#include "nrsolver.h"
#include "cscblkmatrix.h"
#include "common.h"


namespace NAMESPACE {

SIMPLE_ERRORCLASS(HbNrForcesError, "Failed to apply forces.");

SIMPLE_ERRORCLASS(HbNrLoadForces, "Failed to load forces.");

// Build the HB convergence report. Every input it needs is passed in
// explicitly, so the text can be produced without an HBNRSolver instance.
std::string formatHbConvergence(
    bool preventedConvergence, bool iterationConverged,
    bool deltaCheckValid, double maxDelta, bool deltaWithinTol, Id maxDeltaNode,
    size_t maxDeltaFreqIndex, double maxDeltaFreq
);

// Carries a full snapshot of the convergence state so the report can be
// rendered later, off the error stack, via formatHbConvergence().
ERRORCLASS(HbNrConvergenceReport)
    bool preventedConvergence;
    bool iterationConverged;
    bool deltaCheckValid;
    double maxDelta;
    bool deltaWithinTol;
    Id maxDeltaNode;
    size_t maxDeltaFreqIndex;
    double maxDeltaFreq;

    HbNrConvergenceReport(
        bool preventedConvergence, bool iterationConverged,
        bool deltaCheckValid, double maxDelta, bool deltaWithinTol, Id maxDeltaNode,
        size_t maxDeltaFreqIndex, double maxDeltaFreq
    ) : preventedConvergence(preventedConvergence), iterationConverged(iterationConverged),
        deltaCheckValid(deltaCheckValid), maxDelta(maxDelta), deltaWithinTol(deltaWithinTol),
        maxDeltaNode(maxDeltaNode), maxDeltaFreqIndex(maxDeltaFreqIndex), maxDeltaFreq(maxDeltaFreq) {}

    std::string format() const {
        return formatHbConvergence(
            preventedConvergence, iterationConverged,
            deltaCheckValid, maxDelta, deltaWithinTol, maxDeltaNode,
            maxDeltaFreqIndex, maxDeltaFreq
        );
    }
END_ERRORCLASS(HbNrConvergenceReport);


class HBNRSolver : public NRSolver {
public:
    // No forces. 
    HBNRSolver(
        Circuit& circuit,
        CommonData& commons,
        CSCBlockSparseRealMatrix& jacColoc,
        CSCBlockSparseRealMatrix& bsjac, 
        VectorRepository<double>& solution, 
        Vector<Complex>& solutionFD,
        const Vector<double>& timepoints,  
        const Spurs& spurs, 
        DenseMatrix<Real>& APFT, 
        DenseMatrix<Real>& IAPFT, 
        DenseMatrix<Real>& OmegaGamma, 
        DenseMatrix<Real>& GammaInvColumnMajor, 
        DelayLines& delayLines, 
        DelayMatrixBindings<DenseMatrixView<double>>& delayBindings, 
        NRSettings& settings
    ); 

    // Format convergence
    virtual std::string formatConvergence() const override;

    // Push a HbNrConvergenceReport snapshot onto the error stack
    virtual void pushConvergenceReport(ErrorConsumer& errors) override;

    // Set forces based on an annotated solution
    bool setForces(Int ndx, const AnnotatedSolution& solution, bool abortOnError, ErrorConsumer& errors);
    
    bool rebuild(size_t nSolComp, ErrorConsumer& errors) override;
    bool initialize(bool continuePrevious, ErrorConsumer& errors) override;
    bool preIteration(bool continuePrevious) override;
    bool postSolve(bool continuePrevious) override;
    bool postConvergenceCheck(bool continuePrevious) override;
    bool postIteration(bool continuePrevious) override;
    bool postRun(bool continuePrevious) override;

    bool evaluate(bool continuePrevious, ErrorConsumer& errors);

    // Convert the solution vector to a complex spectrum in solutionFD
    void updateSolutionFD();

    std::tuple<bool, bool> buildSystem(bool continuePrevious, ErrorConsumer& errors) override;
    std::tuple<bool, bool> checkResidual() override;
    std::tuple<bool, bool> checkDelta() override;

    EvalSetup& evalSetup() { return evalSetup_; };
    LoadSetup& loadSetup() { return loadSetup_; };

    void dumpSolution(std::ostream& os, double* solution, const char* prefix="") override;
    
protected:
    bool loadForces(bool loadJacobian=true); 

    bool evalAndLoadWrapper(EvalSetup& evalSetup, LoadSetup& loadSetup, ErrorConsumer& errors);

    CommonData& commons;
    
    Vector<double*> diagPtrs;
    
    EvalSetup evalSetup_;
    LoadSetup loadSetup_;

    // Jacobian entries at colocation points
    CSCBlockSparseRealMatrix& jacColoc;

    // HB Jacobian
    CSCBlockSparseRealMatrix& bsjac;
    
    // References without a bucket
    const Vector<double>& timepoints;
    const Spurs& spurs_;
    DenseMatrix<double>& Gamma;
    DenseMatrix<double>& GammaInv;
    DenseMatrix<double>& OmegaGamma;
    DenseMatrix<double>& GammaInvColumnMajor;
    Vector<Complex>& solutionFD;
    Circuit& circuit;

    // Per-unknown vectors. Like solution/delta these carry a bucket one block
    // (nt components) wide, so circuit unknown u (1-based, ground = 0) is at u*nt.
    Vector<Real> solutionTD;

    DenseMatrix<Real> blockTmp;

    // Internal structures computed at t_k
    // These structures have a bucket because they communicate with 
    // evalAndLoad() which requires vectors to have a bucket. 
    VectorRepository<double> oldSolutionAtTk;
    Vector<double> resistiveResidualAtTk;
    Vector<double> reactiveResidualAtTk;
    Vector<double> maxResidualContributionAtTk_; // maximal residual contribution for all equations at a given timepoint
                                                 // filled with maximal resistive contribution in evalAndLoadWrapper()
    // No bucket, just a dummy
    Vector<double> dummyStates;

    // Time-domain residuals at all timepoints, per unknown (bucketed, u*nt)
    Vector<double> resistiveResidual;
    Vector<double> reactiveResidual;
    
    // Convergence check auxiliary results
    bool deltaCheckValid;
    double maxDelta;
    double maxNormDelta;
    Node* maxDeltaNode;
    size_t maxDeltaFreqIndex;
    bool deltaWithinTol;

    DelayLines& delayLines_;
    DelayMatrixBindings<DenseMatrixView<double>>& delayBindings_;
};

}

#endif
