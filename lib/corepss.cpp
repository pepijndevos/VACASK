#include "corepss.h"
#include "core.h"
#include "densematrix.h"
#include "simulator.h"
#include "common.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace NAMESPACE {

// ----------------------------------------------------------------
// PssParameters introspection
// ----------------------------------------------------------------

template<> int Introspection<PssParameters>::setup() {
    registerMember(driven);
    registerMember(Tper);
    registerMember(Tstab);
    registerMember(maxitr);
    registerMember(epsmax);
    registerMember(write);
    registerMember(writestab);
    registerMember(ic);
    registerNamedMember(opParams.nodeset, "nodeset");
    return 0;
}
instantiateIntrospection(PssParameters);


// ----------------------------------------------------------------
// PssCore output descriptor proxies
// ----------------------------------------------------------------

void PssCore::clearOutputDescriptors() {
    AnalysisCore::clearOutputDescriptors();
    opCore_.clearOutputDescriptors();
    stabilTran_.clearOutputDescriptors();
    pssTran_.clearOutputDescriptors();
}

bool PssCore::addOutputDescriptor(const OutputDescriptor& desc) {
    return pssTran_.addOutputDescriptor(desc);
}

bool PssCore::addAllUnknowns(const PTSave& save) {
    return pssTran_.addAllUnknowns(save);
}

bool PssCore::addAllNodes(const PTSave& save) {
    return pssTran_.addAllNodes(save);
}

bool PssCore::addNode(const PTSave& save) {
    return pssTran_.addNode(save);
}

bool PssCore::addFlow(const PTSave& save) {
    return pssTran_.addFlow(save);
}

bool PssCore::addInstanceOutvar(const PTSave& save) {
    return pssTran_.addInstanceOutvar(save);
}


PssCore::PssCore(
    OutputDescriptorResolver& parentResolver,
    PssParameters& params,
    Circuit& circuit,
    CommonData& commons,
    KluRealMatrix& jacobian,
    VectorRepository<double>& solution,
    VectorRepository<double>& states,
    OperatingPointCore& opCore,
    TranCore& stabilTran,
    PssTranCore& pssTran
) : AnalysisCore(parentResolver, circuit, commons),
    params(params),
    jacobian(jacobian),
    solution(solution),
    states(states),
    opCore_(opCore),
    stabilTran_(stabilTran),
    pssTran_(pssTran),
    outfile(nullptr),
    T0_converged_(0.0)
{
}

PssCore::~PssCore() {
    delete outfile;
}

bool PssCore::rebuild(Status& s) {
    if (params.writestab) {
        params.stabilParams.write = 1;
        stabilTran_.addCoreOutputDescriptors();
        stabilTran_.addDefaultOutputDescriptors();
        stabilTran_.resolveOutputDescriptors(false);
    }
    if (params.write) {
        params.shootParams.write = 1;
        pssTran_.addCoreOutputDescriptors();
        pssTran_.addDefaultOutputDescriptors();
        pssTran_.resolveOutputDescriptors(false);
        // Turn writing off for now, only write converged waveform
        params.shootParams.write = 0;
    }
    return true;
}

bool PssCore::initializeOutputs(Id name, Status& s) {
    // Store the name for use when the output file is opened after convergence.
    // Do NOT open the pssTran_ raw file here: it must be opened only once,
    // after the Newton loop converges, so that intermediate shooting iterations
    // do not pollute the output.
    name_ = name;
    return true;
}

bool PssCore::finalizeOutputs(Status& s) {
    return pssTran_.finalizeOutputs(s);
}

bool PssCore::deleteOutputs(Id name, Status& s) {
    if (!params.write || Simulator::noOutput()) {
        return true;
    }
    // Cannot assume outfile is available
    auto fname = std::string(name)+".raw";
    if (std::filesystem::exists(fname)) {
        std::filesystem::remove(fname);
    }
    return true;
}

bool PssCore::formatError(Status& s) const {
    switch (lastPssError) {
        case PssError::NoConvergence:
            s.set(Status::Analysis,
                  "PSS failed to converge in " +
                  std::to_string(params.maxitr) + " iterations.");
            return false;
        case PssError::StabilisationFailed:
            s.set(Status::Analysis, "PSS stabilisation transient failed.");
            return false;
        case PssError::ShootFailed:
            s.set(Status::Analysis, "PSS shooting transient failed.");
            return false;
        case PssError::SensitivityFailed:
            s.set(Status::Analysis, "PSS sensitivity integration failed.");
            return false;
        case PssError::LinearSolveFailed:
            s.set(Status::Analysis, "PSS Newton linear solve failed.");
            return false;
        case PssError::OutputError:
            s.set(Status::Analysis, "PSS output error.");
            return false;
        default:
            return true;
    }
}

void PssCore::dump(std::ostream& os) const {
    AnalysisCore::dump(os);
    os << "  Converged period: " << T0_converged_ << " s\n";
    os << "  Converged frequency: "
       << (T0_converged_ > 0 ? 1.0 / T0_converged_ : 0.0) << " Hz\n";
}


// ----------------------------------------------------------------
// Main Newton loop
// ----------------------------------------------------------------

bool PssCore::run(bool continuePrevious) {
    auto c = coroutine(continuePrevious);
    bool ok = true;
    while (!c.done()) {
        if (c.resume() == CoreState::Aborted) {
            ok = false;
            break;
        }
    }
    return ok;

}

CoreCoroutine PssCore::coroutine(bool continuePrevious) {
    // clearError();
    // if (!run(continuePrevious)) {
    //     co_yield CoreState::Aborted;
    // }
    // co_yield CoreState::Finished;
    clearError();

    auto& options = circuit.simulatorOptions().core();
    auto debug = options.pss_debug;

    auto n = circuit.unknownCount();

    /// TODO: Move to private class members. Size correctly during rebuild.
    DenseMatrix<double> PhiT;
    Vector<double>      PsiT;
    Vector<double>      Fp(n + 1, 0.0);
    Vector<double>      alpha(n + 1, 0.0);
    DenseMatrix<double> Jp(n + 1, n + 1);
    double              epsilon = params.epsmax + 1;  // Make initial epsilon big enough

    Int iterIndex = 0;
    Vector<double>  x0;
    Vector<double>  xT;
    double          T0;

    bool converged = false;

    Status s;
    std::stringstream ss;
    ss << std::scientific << std::setprecision(4);

    // Check parameters
    if(params.Tper <= 0) {
        setError(PssError::TperInvalid);
        co_yield CoreState::Aborted;
    }

    if (params.Tstab < 10.0 * params.Tper) {
        Simulator::wrn() << "PSS: Tstab < 10 * Tper. Oscillator may not have settled.\n";
    }

    // Stabilisation transient.
    if (!runStabilisation(s)) {
        setError(PssError::StabilisationFailed);
        co_yield CoreState::Aborted;
    }

    // x_0^(0) is the state of the circuit after running the stabilisation transient
    x0 = solution.vector();
    T0 = params.Tper;

    // Obtain x_T^(0) by running a transient simulation for T_0 seconds
    solution.vector() = x0;
    pssTran_.setShootIC(x0);
    pssTran_.clearTrajectory(T0);
    if (!runShoot(T0, s)) {
        setError(PssError::ShootFailed);
        co_yield CoreState::Aborted;
    }
    xT = solution.vector();

    // PSS-SHOOT main loop (outer NR)
    while (iterIndex <= params.maxitr && epsilon > params.epsmax) {

        if (debug>0){
            ss.str(""); 
            ss << "PSS: shoot l=" << iterIndex << "\n";
            ss << "\tx0=[";
            auto n = circuit.unknownCount();
            for (decltype(n) i = 1; i <= n; i++) ss << " " << x0[i];
            ss << " ]\n" << "\txT=[";
            for (decltype(n) i = 1; i <= n; i++) ss << " " << xT[i];
            ss << " ]\n";
            if (!params.driven) {
                ss << "\tT0=" << T0 << "\n";
            }
            Simulator::dbg() << ss.str();
        }

        // Obtain sensitivity matrices
        if (!runSensitivity(PhiT, PsiT, s)) {
            setError(PssError::SensitivityFailed);
            co_yield CoreState::Aborted;
        }
        if (debug>0) {
            ss.str(""); 
            auto n = circuit.unknownCount();
            ss << "\tPhiT=\n";
            for (decltype(n) i = 0; i < n; i++) {
                ss << "\t  [ ";
                for (decltype(n) j = 0; j < n; j++) ss << PhiT.at(i, j) << " ";
                ss << "]\n";
            }
            if(!params.driven) {
                ss << "\tPsiT=[ ";
                for (decltype(n) i = 1; i <= n; i++) ss << PsiT[i] << " ";
                ss << "]\n";
            }
            Simulator::dbg() << ss.str();
        }

        // Compute the residual
        for (decltype(n) i = 0; i < n; i++) {
            // skip the bucket at x[0]
            Fp[i] = x0[i + 1] - xT[i + 1];
        }

        // Jacobian: I - PhiT
        for (decltype(n) i = 0; i < n; i++) {
            for (decltype(n) j = 0; j < n; j++) {
                Jp.at(i, j) = (i == j ? 1.0 : 0.0) - PhiT.at(i, j);
            }
        }

        // Compute phase constraint for autonomous circuits
        /// TODO: rewrite computePhaseConstraint without weird approximations
        if (!params.driven){
            computePhaseConstraint(x0, T0, PsiT, alpha);
            if (debug>0){
                ss.str(""); 
                ss << "\talpha=[ ";
                for (decltype(n) i = 0; i <= n; i++) ss << alpha[i] << " ";
                ss << "]\n";
                Simulator::dbg() << ss.str();
            }
            // Add alpha^Tx_0 to the Fp map
            Fp[n] = 0;
            //for (decltype(n) i = 0; i <= n; i++) Fp[n] += alpha[i] * x0[i];
            // Augment the Jacobian
            // Right column: PsiT
            for (decltype(n) i = 0; i < n; i++) Jp.at(i, n) = -PsiT[i + 1];
            // Bottom row: alpha^T
            for (decltype(n) j = 0; j < n; j++) Jp.at(n, j) = alpha[j + 1];
        }
        // If the circuit is driven, make sure Jp is not singular by setting the corner to 1
        Jp.at(n, n) = params.driven ? 1.0 : 0.0;

        // Solve the Newton step
        VectorView<double> rhsView(Fp);
        if (!Jp.destructiveSolve(rhsView)) {
             setError(PssError::SingularJacobian);
             co_yield CoreState::Aborted;
        }
        // Update x0 and T0
        for (decltype(n) i = 0; i < n; i++) x0[i + 1] -= rhsView[i];
        T0 -= params.driven ? 0.0 : rhsView[n];

        // Get new xT
        solution.vector() = x0;
        pssTran_.setShootIC(x0);
        pssTran_.clearTrajectory(T0);
        if (!runShoot(T0, s)) {
            setError(PssError::ShootFailed);
            co_yield CoreState::Aborted;
        }
        xT = solution.vector();

        // Compute error using infinite-norm
        epsilon = 0;
        for (decltype(n) i = 1; i <= n; i++) {
            epsilon = std::max(epsilon, std::abs(x0[i] - xT[i]));
        }
        if (debug>0){
            ss.str(""); 
            ss << "\tepsilon= " << epsilon << "\n";
            Simulator::dbg() << ss.str();
        }

        if (epsilon < params.epsmax) {
            // Converged
            converged = true;
            break;
        } else {
            iterIndex++;
        }
    } // end PSS-SHOOT main loop
    
    if (!converged) {
        setError(PssError::NoConvergence);
        co_yield CoreState::Aborted;
    }

    Simulator::dbg() << "PSS analysis finshed.\n";
    if (debug>0) {
        ss.str("");
        ss << "Converged in " << iterIndex + 1 << " iterations.\n";
        ss << "Final epsilon = " << epsilon << "\n";
        Simulator::dbg() << ss.str();
    }

    // If write is enabled, write the obtained PSS solution
    if (params.write && !Simulator::noOutput()) {
        params.shootParams.write = 1;
        if (!pssTran_.initializeOutputs(name_, s)) {
            setError(PssError::OutputError);
            // This is not a PSS failure, Core is finished, not aborted
            co_yield CoreState::Finished;
        }
        solution.vector() = x0;
        pssTran_.setShootIC(x0);
        pssTran_.clearTrajectory(T0);
        if (!runShoot(T0, s)) {
            setError(PssError::ShootFailed);
            co_yield CoreState::Aborted;
        }
    }

    co_yield CoreState::Finished;
    
}


// ----------------------------------------------------------------
// Stabilisation transient
// ----------------------------------------------------------------

bool PssCore::runStabilisation(Status& s) {
    params.stabilParams.step    = params.Tper / 1000.0;
    params.stabilParams.stop    = params.Tstab;
    params.stabilParams.maxstep = params.Tper / 1000.0;
    params.stabilParams.start   = 0.0;
    params.stabilParams.write   = params.writestab;

    // icmode and ic were forwarded to stabilParams in Pss::preMapping().
    // If no IC was given, run the DC operating point now.
    bool hasIc = (params.ic.type() == Value::Type::ValueVec);
    if (!hasIc) {
        params.stabilParams.icmode = TranCore::icmodeOp;
        if (!opCore_.run(false)) {
            opCore_.formatError(s);
            return false;
        }
    }

    bool writeStab = params.writestab && !Simulator::noOutput();
    Id stabName = Id(std::string(name_) + "_stabtran");
    if (writeStab) {
        if (!stabilTran_.initializeOutputs(stabName, s)) {
            return false;
        }
    }
    if (!stabilTran_.run(false)) {
        stabilTran_.formatError(s);
        return false;
    }
    if (writeStab) {
        stabilTran_.finalizeOutputs(s);
    }
    return true;
}


// ----------------------------------------------------------------
// One-period shoot
// ----------------------------------------------------------------

bool PssCore::runShoot(double T0, Status& s) {
    params.shootParams.stop    = T0;
    params.shootParams.step    = T0 / 1e3;
    params.shootParams.maxstep = T0 / 1e3;
    params.shootParams.start   = 0.0;
    params.shootParams.icmode  = TranCore::icmodeUic;
    // write is left as-is: 0 during Newton iterations, 1 for the final output shoot.

    if (!pssTran_.run(false)) {
        pssTran_.formatError(s);
        return false;
    }
    return true;
}


// ----------------------------------------------------------------
// Sensitivity integration
// ----------------------------------------------------------------

bool PssCore::runSensitivity(
    DenseMatrix<double>& PhiT,
    Vector<double>&      PsiT,
    Status& s
) {
    auto n = circuit.unknownCount();
    if (!params.driven) {
        Vector<double> x_laststep(n, 0.0);
        for (int i=0; i < n; i++)
            x_laststep[i] = solution.pastVector()[i+1] - solution.vector()[i+1];
        if (!pssTran_.integrateAugmentedSensitivity(PhiT, PsiT, x_laststep)) {
            s.set(Status::Analysis, "PSS sensitivity integration failed.");
            return false;
        }
    } else {
        if (!pssTran_.integrateSensitivity(PhiT)) {
            s.set(Status::Analysis, "PSS sensitivity integration failed.");
            return false;
        }
    }

    return true;
}


// ----------------------------------------------------------------
// Phase constraint
// ----------------------------------------------------------------

void PssCore::computePhaseConstraint(
    const Vector<double>& x0,
    double T0,
    const Vector<double>& PsiT,
    Vector<double>& alpha
) {
    // alpha is the phase-pinning vector. The augmented Newton row
    //   alpha^T * dx0 = 0
    // constrains the correction dx0 to be perpendicular to alpha, so the
    // Newton step does not move along the phase direction (the null space of
    // (I - PhiT)). alpha must not be in the image of (I - PhiT), so it should
    // be proportional to xdot_s(t0), the circuit velocity at t0.
    //
    // From the circuit DAE: g(x) + C(x)*xdot = 0, so
    //   xdot_s(t0) = -C(x0)^{-1} * g(x0)
    //
    // PsiT = alphaLast * Alr^{-1} * g(xT) ≈ -xdot_s(t0) (same approximation
    // as the period sensitivity vector: C^{-1} ≈ alphaLast * Alr^{-1}).
    // Using PsiT as alpha avoids extra evalAndLoad calls and is consistent.
    // Normalised to unit length for numerical conditioning.

    auto n = circuit.unknownCount();
    alpha.assign(n + 1, 0.0);

    double norm = 0.0;
    for (decltype(n) i = 1; i <= n; i++) {
        norm += PsiT[i] * PsiT[i];
    }
    norm = std::sqrt(norm);

    if (norm > 0.0) {
        for (decltype(n) i = 1; i <= n; i++) {
            alpha[i] = PsiT[i] / norm;
        }
    } else {
        Simulator::dbg() << "PsiT is zero\n";
        // PsiT is zero (purely resistive circuit or degenerate first step).
        // Fall back to pinning the first unknown.
        if (n >= 1) {
            alpha[1] = 1.0;
        }
    }
}

} // namespace NAMESPACE
