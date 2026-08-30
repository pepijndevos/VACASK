#ifndef __COREHBNR_DEFINED
#define __COREHBNR_DEFINED

#include "nrsolver.h"
#include "klubsmatrix.h"
#include "common.h"


namespace NAMESPACE {

SIMPLE_ERRORCLASS(HbNrForcesError, "Failed to apply forces.");

SIMPLE_ERRORCLASS(HbNrLoadForces, "Failed to load forces.");

class HBNRSolver : public NRSolver {
public:
    // No forces. 
    HBNRSolver(
        Circuit& circuit,
        CommonData& commons,
        KluBlockSparseRealMatrix& jacColoc,
        KluBlockSparseRealMatrix& bsjac, 
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

    // Set forces based on an annotated solution
    bool setForces(Int ndx, const AnnotatedSolution& solution, bool abortOnError, ErrorConsumer& errors);
    
    virtual bool rebuild(size_t nSolComp);
    virtual bool initialize(bool continuePrevious, ErrorConsumer& errors);
    virtual bool preIteration(bool continuePrevious);
    virtual bool postSolve(bool continuePrevious);
    virtual bool postConvergenceCheck(bool continuePrevious);
    virtual bool postIteration(bool continuePrevious);
    virtual bool postRun(bool continuePrevious); 
    
    bool evaluate(bool continuePrevious, ErrorConsumer& errors);
    
    virtual std::tuple<bool, bool> buildSystem(bool continuePrevious, ErrorConsumer& errors);
    virtual std::tuple<bool, bool> checkResidual();
    virtual std::tuple<bool, bool> checkDelta();
    
    EvalSetup& evalSetup() { return evalSetup_; };
    LoadSetup& loadSetup() { return loadSetup_; };

    virtual void dumpSolution(std::ostream& os, double* solution, const char* prefix="");
    
protected:
    bool loadForces(bool loadJacobian=true); 

    bool evalAndLoadWrapper(EvalSetup& evalSetup, LoadSetup& loadSetup, ErrorConsumer& errors);

    CommonData& commons;
    
    Vector<double*> diagPtrs;
    
    EvalSetup evalSetup_;
    LoadSetup loadSetup_;

    // Jacobian entries at colocation points
    KluBlockSparseRealMatrix& jacColoc;

    // HB Jacobian
    KluBlockSparseRealMatrix& bsjac;
    
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
