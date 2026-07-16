#ifndef __CMD_DEFINED 
#define __CMD_DEFINED

#include <unordered_set>
#include <limits>
#include <variant>
#include <unordered_map>
#include "parseroutput.h"
#include "circuit.h"
#include "status.h"
#include "common.h"

namespace NAMESPACE {

class CommandInterpreter;

enum class InterpreterExitStatus { 
    OK,            // returned by commands on success
    Error,         // returned on an error that can be ignored
    HardFault,     // returned on an error that can't be masked
    RequestMCExit, // endmc command requests loop exit
    EndReached,    // end of commands reached
};

typedef InterpreterExitStatus (*CommandFuncPtr)(CommandInterpreter& interpreter, PTCommand& cmd, Status& s);

template <typename T> bool evaluateExpressions(RpnEvaluator& e, const PTCommand& cmd, std::vector<T>& out, Status& s=Status::ignore);

class CommandInterpreter {
public:
    typedef struct CmdDesc {
        static const size_t many = std::numeric_limits<size_t>::max();

        size_t minKw {0};
        size_t maxKw {0};
        size_t minExpr {0};
        size_t maxExpr {0};
        bool limitArgs {false};
        std::unordered_set<Id> allowedArgs;
        CommandFuncPtr func;
    } CmdDesc;

    CommandInterpreter(ParserTables& tables, PTControl& control, Circuit& circuit);
    ~CommandInterpreter();

    CommandInterpreter           (const CommandInterpreter&)  = delete;
    CommandInterpreter           (      CommandInterpreter&&) = delete;
    CommandInterpreter& operator=(const CommandInterpreter&)  = delete;
    CommandInterpreter& operator=(      CommandInterpreter&&) = delete;

    bool postprocessingAllowed() { return runPostprocess_; };

    void setPrintProgress(bool b) { printProgress_ = b; };
    void setRunPostprocess(bool b) { runPostprocess_ = b; };

    bool printProgress() const { return printProgress_; }; 
    bool runPostprocess() const { return runPostprocess_; }; 

    void clearSaves() { commonSaves_.clear(); };
    void addSaves(PTSaves& s) { commonSaves_.insert(commonSaves_.end(), s.saves().begin(), s.saves().end()); }; 

    void clearUserOptions() { userOptions_.clear(); };
    void addUserOption(const PTParameterValue& pv);
    void addUserOption(const PTParameterExpression& pe);
    
    // If circuit is not elaboarated, perform default elaboration, otherwise elaborate only changes
    bool minimalElaboration(Status& s);
    // If circuit is not elaboarated, perform default elaboration, otherwise do nothing
    bool defaultElaboration(Status& s);
    // Elaborate circuit from given toplevel definitions
    bool elaborate(const std::vector<Id>& names, const std::string& topDefName, const std::string& topInstName, Status& s=Status::ignore);
    
    InterpreterExitStatus run(size_t from=0, Status& s=Status::ignore);

    bool clearVariables(Status& s=Status::ignore);
    Circuit& circuit() { return circuit_; }; 
    ParserTables& tables() { return tables_; };
    RpnEvaluator& variableEvaluator() { return circuit_.variableEvaluator(); }; 

    bool addAbort(Id cmd);
    void clearAborts();
    void setAbortOnMatch(bool b);
    bool mustAbort(Id cmd);

    void dumpOptionsMap(int indent, std::ostream& os) const;
    void dumpSaves(int indent, std::ostream& os) const;

    size_t at() { return at_; };

    void reset() { mcNames.clear(); at_=0; };
    bool isUniqueMc(Id name) {
        if (mcNames.contains(name)) {
            return false;
        } else {
            mcNames.insert(name);
            return true;
        }
    };
    void setAnalysisNamePrefix(const std::string& pfx) { analysisNamePrefix_ = pfx; };
    
private:
    size_t at_;
    std::unordered_set<Id> mcNames;
    std::string analysisNamePrefix_;

    bool printProgress_;
    bool runPostprocess_;
    std::vector<PTSave> commonSaves_;
    PTParameterMap userOptions_;
    
    std::unordered_set<Id> abortCommands;
    bool abortOnMatch;

    static std::unordered_map<Id, CmdDesc> commandDescriptors;

    ParserTables& tables_;
    PTControl& control_;
    Circuit& circuit_;
};

}

#endif
