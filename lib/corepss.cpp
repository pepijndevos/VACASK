// corepss.cpp
//
// PssCore implementation.
// See corepss.h for design description and algorithm outline.

#include "corepss.h"
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
    registerMember(Tper);
    registerMember(Tstab);
    registerMember(MaxItr);
    registerMember(EpsMax);
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
    params_(params),
    jac_(jacobian),
    solution_(solution),
    states_(states),
    opCore_(opCore),
    stabilTran_(stabilTran),
    pssTran_(pssTran),
    outfile_(nullptr),
    T0_converged_(0.0)
{
}

PssCore::~PssCore() {
    delete outfile_;
}


// ----------------------------------------------------------------
// AnalysisCore interface - boilerplate, delegates to pssTran_
// ----------------------------------------------------------------

bool PssCore::addCoreOutputDescriptors() {
    // Delegates to pssTran_ for time-domain output descriptors (node voltages,
    // branch currents along the converged shooting trajectory).
    // Frequency-domain spectrum output can be added here in the future.
    return pssTran_.addCoreOutputDescriptors();
}

bool PssCore::addDefaultOutputDescriptors() {
    return pssTran_.addDefaultOutputDescriptors();
}

bool PssCore::resolveOutputDescriptors(bool strict, Status& s) {
    return pssTran_.resolveOutputDescriptors(strict);
}

bool PssCore::rebuild(Status& s) {
    if (!jac_.rebuild(circuit.sparsityMap(), circuit.unknownCount())) {
        jac_.formatError(s);
        return false;
    }
    // Rebuild stabilTran_ first: its rebuild populates IC force slot 2
    // of opCore_ which opCore_.rebuild() depends on.
    if (!stabilTran_.rebuild(s)) {
        return false;
    }
    if (!opCore_.rebuild(s)) {
        return false;
    }
    if (!pssTran_.rebuild(s)) {
        return false;
    }
    if (params_.writestab) {
        params_.stabilParams.write = 1;
        stabilTran_.addCoreOutputDescriptors();
        stabilTran_.addDefaultOutputDescriptors();
        stabilTran_.resolveOutputDescriptors(false);
    }
    return true;
}

bool PssCore::initializeOutputs(Id name, Status& s) {
    name_ = name;
    if (!params_.write || Simulator::noOutput()) {
        return true;
    }
    return pssTran_.initializeOutputs(name, s);
}

bool PssCore::finalizeOutputs(Status& s) {
    return pssTran_.finalizeOutputs(s);
}

bool PssCore::deleteOutputs(Id name, Status& s) {
    return pssTran_.deleteOutputs(name, s);
}

bool PssCore::formatError(Status& s) const {
    switch (lastPssError_) {
        case PssError::NoConvergence:
            s.set(Status::Analysis,
                  "PSS failed to converge in " +
                  std::to_string(params_.MaxItr) + " iterations.");
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
    clearError();

    Status s;

    if (params_.Tper <= 0) {
        s.set(Status::Analysis, "PSS: Tper must be greater than zero.");
        return false;
    }
    if (params_.Tstab < 10.0 * params_.Tper) {
        Simulator::wrn() << "PSS: Tstab < 10 * Tper. "
                            "Oscillator may not have settled.\n";
    }

    // Stabilisation transient. On return, solution_.vector() = x0.
    if (!runStabilisation(s)) {
        setError(PssError::StabilisationFailed);
        return false;
    }

    Vector<double> x0 = solution_.vector();
    double T0 = params_.Tper;

    {
        std::stringstream ss;
        ss << std::scientific << std::setprecision(6) << T0;
        Simulator::dbg() << "PSS: Newton loop starting, T0 initial = "
                         + ss.str() + " s\n";
    }

    DenseMatrix<double> PhiT;
    Vector<double>      PsiT;

    for (int l = 0; l <= params_.MaxItr; l++) {

        // Load x0 as the shooting initial condition and clear the
        // trajectory so onTimestepAccepted() starts fresh.
        solution_.vector() = x0;
        pssTran_.setShootIC(x0);
        pssTran_.clearTrajectory();

        {
            std::stringstream ss;
            ss << std::scientific << std::setprecision(4);
            ss << "PSS: shoot l=" << l << " x0=[";
            auto n = circuit.unknownCount();
            for (decltype(n) i = 1; i <= n; i++) ss << " " << x0[i];
            ss << " ]\n";
            Simulator::dbg() << ss.str();
        }

        // Shoot one period T0 from x0. On return solution_ = xT.
        if (!runShoot(T0, s)) {
            setError(PssError::ShootFailed);
            return false;
        }

        Vector<double> xT = solution_.vector();

        {
            std::stringstream ss;
            ss << std::scientific << std::setprecision(4);
            ss << "PSS: shoot l=" << l << " xT=[";
            auto n = circuit.unknownCount();
            for (decltype(n) i = 1; i <= n; i++) ss << " " << xT[i];
            ss << " ]\n";
            Simulator::dbg() << ss.str();
        }
        
        {
            std::stringstream ss;
            ss << std::scientific << std::setprecision(4);
            ss << "PSS: shoot l=" << l << " x(T-1)=[";
            auto n = circuit.unknownCount();
            for (decltype(n) i = 1; i <= n; i++) ss << " " << solution_.pastVector()[i];
            ss << " ]\n";
            Simulator::dbg() << ss.str();
        }

        {
            std::stringstream ss;
            ss << std::scientific << std::setprecision(4);
            ss << "PSS: shoot l=" << l << " x(T-1) - x(T)=[";
            auto n = circuit.unknownCount();
            for (decltype(n) i = 1; i <= n; i++) ss << " " << solution_.pastVector()[i] - xT[i];
            ss << " ]\n";
            Simulator::dbg() << ss.str();
        }
        

        // Shooting residual Fp = x0 - xT.
        auto n = circuit.unknownCount();
        Vector<double> Fp(n + 1, 0.0);
        double eps = 0.0;
        for (decltype(n) i = 1; i <= n; i++) {
            Fp[i] = x0[i] - xT[i];
            eps = std::max(eps, std::abs(Fp[i]));
        }
        
        {
            std::stringstream ss;
            ss << std::scientific << std::setprecision(4);
            ss << "PSS: shoot l=" << l << " Fp=[";
            auto n = circuit.unknownCount();
            for (decltype(n) i = 1; i <= n; i++) ss << " " << Fp[i];
            ss << " ]\n";
            ss << "  epsilon=" << eps << "\n";
            Simulator::dbg() << ss.str();
        }

        if (eps < params_.EpsMax) {
            T0_converged_ = T0;
            x0_converged_ = x0;
            Simulator::dbg() << "PSS: converged in " + std::to_string(l)
                             + " iterations, f0="
                             + std::to_string(1.0 / T0) + " Hz\n";
            return true;
        }

        if (!runSensitivity(PhiT, PsiT, s)) {
            setError(PssError::SensitivityFailed);
            return false;
        }

        {
            auto n = circuit.unknownCount();
            std::stringstream ss;
            ss << std::scientific << std::setprecision(4);
            ss << "PSS: PsiT=[ ";
            for (decltype(n) i = 1; i <= n; i++) ss << PsiT[i] << " ";
            ss << "]\n";
            ss << "PSS: PhiT=\n";
            for (decltype(n) i = 0; i < n; i++) {
                ss << "  [ ";
                for (decltype(n) j = 0; j < n; j++) ss << PhiT.at(i, j) << " ";
                ss << "]\n";
            }
            Simulator::dbg() << ss.str();
        }

        if (!solveNewtonStep(x0, T0, xT, PhiT, PsiT, s)) {
            setError(PssError::LinearSolveFailed);
            return false;
        }

        {
            std::stringstream ss;
            ss << std::scientific << std::setprecision(6);
            ss << "PSS: updated T0=" << T0 << " s  f0=" << 1.0/T0 << " Hz\n";
            ss << "PSS: updated x0=[ ";
            auto n = circuit.unknownCount();
            for (decltype(n) i = 1; i <= n; i++) ss << x0[i] << " ";
            ss << "]\n";
            Simulator::dbg() << ss.str();
        }
    }

    setError(PssError::NoConvergence);
    return false;
}

CoreCoroutine PssCore::coroutine(bool continuePrevious) {
    clearError();
    if (!run(continuePrevious)) {
        co_yield CoreState::Aborted;
    }
    co_yield CoreState::Finished;
}


// ----------------------------------------------------------------
// Stabilisation transient
// ----------------------------------------------------------------

bool PssCore::runStabilisation(Status& s) {
    params_.stabilParams.step    = params_.Tper / 100.0;
    params_.stabilParams.stop    = params_.Tstab;
    params_.stabilParams.maxstep = params_.Tper / 10.0;
    params_.stabilParams.start   = 0.0;
    params_.stabilParams.write   = params_.writestab;

    // icmode and ic were forwarded to stabilParams in Pss::preMapping().
    // If no IC was given, run the DC operating point now.
    bool hasIc = (params_.ic.type() == Value::Type::ValueVec);
    if (!hasIc) {
        params_.stabilParams.icmode = TranCore::icmodeOp;
        if (!opCore_.run(false)) {
            opCore_.formatError(s);
            return false;
        }
    }

    bool writeStab = params_.writestab && !Simulator::noOutput();
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
    params_.stabilParams.stop    = T0;
    params_.stabilParams.step    = T0 / 200.0;
    params_.stabilParams.maxstep = T0 / 20.0;
    params_.stabilParams.start   = 0.0;
    params_.stabilParams.icmode  = TranCore::icmodeUic;
    params_.stabilParams.write   = params_.write;

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
    Vector<double> x_laststep(n, 0.0);
    for (int i=0; i < n; i++)
        x_laststep[i] = solution_.pastVector()[i+1] - solution_.vector()[i+1];
    if (!pssTran_.integrateSensitivity(PhiT, PsiT, x_laststep)) {
        s.set(Status::Analysis, "PSS sensitivity integration failed.");
        return false;
    }
    return true;
}


// ----------------------------------------------------------------
// Augmented Newton step
// ----------------------------------------------------------------

bool PssCore::solveNewtonStep(
    Vector<double>&       x0,
    double&               T0,
    const Vector<double>& xT,
    DenseMatrix<double>&  PhiT,
    const Vector<double>& PsiT,
    Status& s
) {
    auto n = circuit.unknownCount();

    // Shooting residual.
    Vector<double> Fp(n + 1, 0.0);
    for (decltype(n) i = 1; i <= n; i++) {
        Fp[i] = x0[i] - xT[i];
    }

    // Phase constraint vector.
    Vector<double> alpha(n + 1, 0.0);
    computePhaseConstraint(x0, T0, PsiT, alpha);
    {
        std::stringstream ss;
        ss << std::scientific << std::setprecision(4);
        ss << "PSS: alpha=\n";
        ss << "  [ ";
        for (decltype(n) i = 0; i <= n; i++) {
            ss << alpha[i] << " ";
        }
        ss << "]\n";
        Simulator::dbg() << ss.str();
    }

    // Augmented (n+1) x (n+1) Newton system:
    //
    //   Jaug = | I - PhiT   PsiT |
    //          | alpha^T    0    |
    //
    //   Frhs = | Fp              |
    //          | alpha^T * x0    |

    DenseMatrix<double> Jaug(n + 1, n + 1);
    DenseMatrix<double> RHS(n + 1, 1);

    // Top-left block: I - PhiT
    for (decltype(n) i = 0; i < n; i++) {
        for (decltype(n) j = 0; j < n; j++) {
            Jaug.at(i, j) = (i == j ? 1.0 : 0.0) - PhiT.at(i, j);
        }
    }
    // Top-right column: PsiT
    for (decltype(n) i = 0; i < n; i++) {
        Jaug.at(i, n) = PsiT[i + 1];
    }
    // Bottom-left row: alpha^T
    for (decltype(n) j = 0; j < n; j++) {
        Jaug.at(n, j) = alpha[j + 1];
    }
    // Bottom-right: 0
    Jaug.at(n, n) = 0.0;

    // RHS
    for (decltype(n) i = 0; i < n; i++) {
        RHS.at(i, 0) = Fp[i + 1];
    }
    // Phase constraint row: alpha^T * dx0 = 0.
    // This constrains the Newton correction to be perpendicular to alpha
    // (no phase shift), leaving the phase wherever it happens to be.
    // Using a nonzero RHS here would attempt a simultaneous phase correction
    // that dwarfs the shooting residual and destroys convergence.
    RHS.at(n, 0) = 0.0;

    {
        std::stringstream ss;
        ss << std::scientific << std::setprecision(4);
        ss << "PSS: Jaug=\n";
        for (decltype(n) i = 0; i <= n; i++) {
            ss << "  [ ";
            for (decltype(n) j = 0; j <= n; j++) ss << Jaug.at(i, j) << " ";
            ss << "]  rhs=" << RHS.at(i, 0) << "\n";
        }
        Simulator::dbg() << ss.str();
    }

    // Solve in place via dense LU factorisation.
    auto rhsView = RHS.column(0);
    if (!Jaug.destructiveSolve(rhsView)) {
        s.set(Status::Analysis, "PSS: augmented Newton system is singular.");
        return false;
    }
    // rhsView now holds [dx0; dT0].
    /*
    // Step-size limiting: scale down the full Newton step if any component
    // changes more than 50% relative to its current magnitude.
    double scale = 1.0;
    for (decltype(n) i = 0; i < n; i++) {
        double rel = std::abs(rhsView[i]) / (std::abs(x0[i + 1]) + 1e-12);
        if (rel > 0.5) scale = std::min(scale, 0.5 / rel);
    }
    {
        double relT = std::abs(rhsView[n]) / T0;
        if (relT > 0.5) scale = std::min(scale, 0.5 / relT);
    }
    
    {
        std::stringstream ss;
        ss << std::scientific << std::setprecision(4);
        ss << "PSS: Newton step scale=" << scale
           << "  dx0=[ ";
        for (decltype(n) i = 0; i < n; i++) ss << rhsView[i] << " ";
        ss << "]  dT0=" << rhsView[n] << " s\n";
        Simulator::dbg() << ss.str();
    }
    */

    for (decltype(n) i = 0; i < n; i++) {
        x0[i + 1] -= rhsView[i];
    }
    T0 -= rhsView[n];

    if (T0 <= 0) {
        s.set(Status::Analysis,
              "PSS: period estimate is negative after Newton step. "
              "Check Tper initial guess.");
        return false;
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

    {
        std::stringstream ss;
        ss << std::scientific << std::setprecision(4);
        ss << "PSS: computePhaseConstraint\n" << "\tPsiT=[";
        for (decltype(n) i = 0; i < PsiT.size(); i++) ss << PsiT[i] << " ";
        ss << "]  norm=" << norm << " s\n";
        Simulator::dbg() << ss.str();
    }

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
