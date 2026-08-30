#ifndef __NRSOLVER_DEFINED
#define __NRSOLVER_DEFINED

#include <stdexcept>
#include "ansupport.h"
#include "options.h"
#include "klumatrix.h"
#include "ansolution.h"
#include "status.h"
#include "acct.h"
#include "common.h"

#include "circuit.h"

namespace NAMESPACE {

// // After every topology change or variables change
// rebuild(); 
//
// run(continuePrevious) {
//   initialize()
//   if (not continuePrevious) {
//     zero old solution vector;
//   }
//   do {
//       zero new solution, Jacobian, delta/residual vector;
//       preIteration();
//       buildSystem();
//       load forces;
//       check matrix and rhs;
//       checkResidual();
//       factor and solve;
//       postSolve();
//       check solution;
//       checkDelta() if not in first iteration;
//       check iteration convergence;
//       check convergence;
//       postConvergenceCheck();
//       if (converged) {
//           exit loop;
//       }
//       compute new solution;
//       rotate solutions;
//       postIteration();
//   } while (iteration limit not reached);
// }

class Forces {
public:
    Forces();

    explicit Forces  (const Forces&)  = default; // Allow explicit copy constructor
    Forces           (      Forces&&) = default;
    Forces& operator=(const Forces&)  = delete;
    Forces& operator=(      Forces&&) = default;

    // Clear forces
    void clear();

    void dump(Circuit& circuit, std::ostream& os) const;

    bool empty() const { return unknownValue_.size()==0 && deltaValue_.size()==0; };
    
    Vector<double> unknownValue_;
    Vector<bool> unknownForced_;
    Vector<double> deltaValue_;
    Vector<std::tuple<UnknownIndex, UnknownIndex>> deltaIndices_;
};


typedef struct NRSettings {
    // Input
    int debug {0};
    Int itlim {100};
    Int itlimCont {50};
    Int convIter {1};
    bool residualCheck {true};
    Real dampingFactor {0.8};
    bool matrixCheck {};
    bool rhsCheck {};
    bool solutionCheck {};
} NRSettings;

enum class NRSolverFlags : uint8_t { 
    Abort = 1,  // Exit analysis immediately, even in the middle of computing a point
    Finish = 2, // Wait until current point is computed to the end, then exit simulation
                 // i.e. for multipoint analyses (sweep, frequency sweep, time sweep) 
                 // wait until current point is computed, then exit
                 // Do not exit sweep. 
    Stop = 4,   // Stop analysis to possibly continue it later
                 // Exit sweep. 
};
DEFINE_FLAG_OPERATORS(NRSolverFlags);


//
// Newton-Raphson solver errors
//
// None need a name resolver: they either carry no data or carry a plain
// iteration count / detail string. The offending node, when there is one, is
// reported by a separate error pushed by the matrix or the forces code.
//

SIMPLE_ERRORCLASS(NrEvalLoadError, "Evaluation/load error.");

SIMPLE_ERRORCLASS(NrNonFiniteSolution, "Solution component is not finite.");

SIMPLE_ERRORCLASS(NrBadRelRefSol, "Unsupported relrefsol value.");

SIMPLE_ERRORCLASS(NrBadRelRefRes, "Unsupported relrefres value.");

// The solver-specific convergence report (formatConvergence()) is pushed as a
// separate message before this one.
SIMPLE_ERRORCLASS(NrConvergenceError, "NR solver failed to converge.");

ERRORCLASS(NrLeftLoop)
    Int iteration;
    NrLeftLoop(Int iteration) : iteration(iteration) {}
    std::string format() const {
        return "Leaving core NR loop in iteration " + std::to_string(iteration) + ".";
    }
END_ERRORCLASS(NrLeftLoop);


class NRSolver : public FlagBase<NRSolverFlags> {
public:
    NRSolver(
        Accounting& acct,
        KluRealMatrixCore& jac, VectorRepository<double>& solution,
        NRSettings& settings,
        size_t bucketSize=0
    );

    // Return value: ok, prevent convergence
    virtual std::tuple<bool, bool> buildSystem(bool continuePrevious, ErrorConsumer& errors) = 0;

    // Return values: ok, residual ok
    virtual std::tuple<bool, bool> checkResidual() = 0;
    
    // Return values: ok, delta ok
    virtual std::tuple<bool, bool> checkDelta() = 0;

    // Rebuild internal structures that depend on topology
    virtual bool rebuild(size_t n);

    // Initialize run (upsize internal structures)
    // Called once at the beginning of NRSolver::run()
    // Pushes onto errors on failure
    virtual bool initialize(bool continuePrevious, ErrorConsumer& errors) = 0;

    // Pre-iteration tasks, called at the beginning of iteration
    // Must set lastError on failure
    virtual bool preIteration(bool continuePrevious) { return true; };

    // Post-solve tasks
    // Must set lastError on failure
    virtual bool postSolve(bool continuePrevious) { return true; };

    // Post convergence check tasks, called after convergence check regardless 
    // of its outcome. 
    // Convergence means that sufficient consecutive iterations converge. 
    // Must set lastError on failure
    virtual bool postConvergenceCheck(bool continuePrevious);

    // Post-iteration tasks, called at the end of iteration. 
    // At this point the solution has been rotated and the current solution
    // is the one that will be used for building the next NR system. 
    // Must set lastError on failure
    virtual bool postIteration(bool continuePrevious) { return true; };

    // Post run tasks, called before exit
    virtual bool postRun(bool continuePrevious) { return true; }; 

    // Resize forces repository
    void resizeForces(Int n);

    // Get forces from a forces slot
    Forces& forces(Int ndx);
    const Forces& forces(Int ndx) const;

    // Enable/disable forces slot. An out-of-range slot is a caller bug (the
    // slot count is fixed by resizeForces()), so it throws rather than reports.
    bool enableForces(Int ndx, bool enable) {
        if (ndx<0 || ndx>=forcesList.size()) {
            throw std::out_of_range("NRSolver::enableForces: forces slot index out of range");
        }
        forcesEnabled[ndx] = enable;
        return true;
    };

    // Set forces factor. Out-of-range slot: see enableForces().
    bool setForcesFactor(Int ndx, double factor) {
        if (ndx<0 || ndx>=forcesList.size()) {
            throw std::out_of_range("NRSolver::setForcesFactor: forces slot index out of range");
        }

        forcesFactor[ndx] = factor;
        return true;
    };

    // Do we have any forces
    bool haveForces() {
        auto nf = forcesList.size();
        for(decltype(nf) iForce=0; iForce<nf; iForce++) {
            if (forcesEnabled[iForce]) {
                return true;
            }
        }
        return false;
    };

    // Run solver, pushing any failure onto errors.
    // Return value: converged
    bool run(bool continuePrevious, ErrorConsumer& errors);

    // Return number of iterations spent in NR loop
    Int iterations() const { return iteration; }; 

    // Dump solution
    virtual void dumpSolution(std::ostream& os, double* solution, const char* prefix="") {};

    // Format convergence state
    virtual std::string formatConvergence() const { return ""; };

    // Push a solver-specific convergence report onto errors. Called when the
    // solver fails to converge with no other error recorded, right before the
    // generic "failed to converge" error. Default: nothing.
    virtual void pushConvergenceReport(ErrorConsumer& errors) {};

protected:
    // Bucket size
    size_t bucketSize_;
    
    // High precision requested
    bool highPrecision;

    // Passed from outside
    KluRealMatrixCore& jac;
    VectorRepository<double>& solution;
    NRSettings& settings;
    Accounting& acct;

    std::vector<Forces> forcesList;
    std::vector<bool> forcesEnabled;
    std::vector<double> forcesFactor;
    
    Vector<double> rowNorm;

    // Stores residual (before solving), and negative delta (after solving)
    Vector<double> delta;
    
    Int iteration;

    // Convergence check results
    // System build requested no convergence
    bool preventedConvergence;
    // Residual is within tolerances
    bool residualOk;
    // Solution change is within tolerances
    bool deltaOk;
    // Iteration converged (convergence not prevented, residual and delta are ok)
    bool iterationConverged;
    // Sufficient number of consecutive iterations converged (settings.convIter), solver done
    bool converged;
};

}

#endif
