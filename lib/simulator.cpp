#include <filesystem>
#include "simulator.h"
#include "anop.h"
#include "andcinc.h"
#include "andcxf.h"
#include "anac.h"
#include "anacxf.h"
#include "anacstb.h"
#include "anacsp.h"
#include "antran.h"
#include "annoise.h"
#include "anhb.h"
#include "anhbac.h" 
#include "anpss.h"
#include "solklu.h"
#ifdef SIM_HAVE_SUPERLU
#include "solsuperlu.h"
#endif
#include "libplatform.h"
#include "common.h"


namespace NAMESPACE { 

template<typename T> bool Simulator::registerAnalysis(Status& s) {
    Analysis::registerFactory(T::analysisId, T::create);
    return true;
}

class NullBuffer : public std::streambuf {
public:
    int overflow(int c) { return c; }
};

static NullBuffer nullBuff; 
std::ostream Simulator::nullStream(&nullBuff);

std::string Simulator::startupPath_;
std::ostream* Simulator::out_ = &std::cout;
std::ostream* Simulator::err_ = &std::cout;
std::ostream* Simulator::dbg_ = &std::cout;
std::ostream* Simulator::wrn_ = &std::cout;

std::vector<std::string> Simulator::modulePath_;
std::vector<std::string> Simulator::includePath_;

bool Simulator::fileDebug_ = false;
bool Simulator::noOutput_ = false;

bool Simulator::setupDone_ = false;
bool Simulator::setupOk_ = false;

void Simulator::setStreams(std::ostream& output, std::ostream& error, std::ostream& debug) {
    Simulator::out_ = &output;
    Simulator::err_ = &error;
    Simulator::dbg_ = &debug;
}

bool Simulator::setup(Status& s) {
    return setup("", "", s);
}

Id Simulator::defaultTdSolverId = Id();
Id Simulator::defaultSmsigSolverId = Id();
Id Simulator::defaultHbSolverId = Id();
Id Simulator::defaultQpsmsigSolverId = Id();

bool Simulator::setup(
    const std::string& moduleFilePathString, 
    const std::string& includeFilePathString, 
    Status& s
) {
    std::vector<std::string> modPathVec;
    std::vector<std::string> incPathVec;

    splitString(pathSeparator(), moduleFilePathString, modPathVec);
    splitString(pathSeparator(), includeFilePathString, incPathVec);

    modulePath_ = modPathVec;
    includePath_ = incPathVec;

    startupPath_ = std::filesystem::current_path().string();

    // Registration runs once; later calls only refresh the paths above and
    // report the first call's result.
    if (setupDone_) {
        return setupOk_;
    }

    bool ok = true;
    ok &= registerAnalysis<OperatingPoint>(s);
    ok &= registerAnalysis<DCIncremental>(s);
    ok &= registerAnalysis<DCXF>(s);
    ok &= registerAnalysis<AC>(s);
    ok &= registerAnalysis<ACXF>(s);
    ok &= registerAnalysis<ACStb>(s);
    ok &= registerAnalysis<ACSP>(s);
    ok &= registerAnalysis<Noise>(s);
    ok &= registerAnalysis<Tran>(s);
    ok &= registerAnalysis<HB>(s);
    ok &= registerAnalysis<HBAC>(s);
    ok &= registerAnalysis<Pss>(s);

    // Register real and complex klu solver here
    ok &= RealSparseSolver::registerSolver<KluRealSparseSolver>();
    ok &= ComplexSparseSolver::registerSolver<KluComplexSparseSolver>();

#ifdef SIM_HAVE_SUPERLU
    // Register real and complex SuperLU_DIST solver (build-time optional)
    ok &= RealSparseSolver::registerSolver<SuperLURealSparseSolver>();
    ok &= ComplexSparseSolver::registerSolver<SuperLUComplexSparseSolver>();

    Simulator::defaultHbSolverId      = SuperLURealSparseSolver::solverId;
    Simulator::defaultQpsmsigSolverId = SuperLURealSparseSolver::solverId;
#else 
    Simulator::defaultHbSolverId      = KluRealSparseSolver::solverId;
    Simulator::defaultQpsmsigSolverId = KluRealSparseSolver::solverId;
#endif

    // Default solver when no solver is specified
    Simulator::defaultTdSolverId      = KluRealSparseSolver::solverId;
    Simulator::defaultSmsigSolverId   = KluRealSparseSolver::solverId;
    
    

    setupDone_ = true;
    setupOk_ = ok;
    return ok;
}

void Simulator::prependModulePath(std::vector<std::string>&& strVec){
    for(auto item : modulePath_) {
        strVec.push_back(std::move(item));
    }
    modulePath_ = std::move(strVec);
}

void Simulator::appendModulePath(std::vector<std::string>&& strVec){
    for(auto item : strVec) {
        modulePath_.push_back(std::move(item));
    }
}

void Simulator::prependIncludePath(std::vector<std::string>&& strVec){
    for(auto item : includePath_) {
        strVec.push_back(std::move(item));
    }
    includePath_ = std::move(strVec);
}

void Simulator::appendIncludePath(std::vector<std::string>&& strVec){
    for(auto item : strVec) {
        includePath_.push_back(std::move(item));
    }
}

}
