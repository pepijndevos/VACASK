#include "cmd.h"
#include "processutils.h"
#include "osdifile.h"
#include "simulator.h"
#include "an.h"
#include "platform.h"
#include "simulator.h"
#include "progressbar.h"
#include "common.h"
#include <type_traits>
#include <chrono>


namespace NAMESPACE {

static Id idInstance = Id::createStatic("instance");
static Id idModel = Id::createStatic("model");
static Id idVariables = Id::createStatic("variables");
static Id idOptions = Id::createStatic("options");
static Id idSaves = Id::createStatic("saves");
static Id idAlways = Id::createStatic("always");
static Id idOn = Id::createStatic("on");
static Id idNever = Id::createStatic("never");
static Id idExcept = Id::createStatic("except");
static Id idAnalysis = Id::createStatic("analysis");
static Id idCircuit = Id::createStatic("circuit");
static Id idChanges = Id::createStatic("changes");
static Id idEol = Id::createStatic("eol");
static Id idSeparator = Id::createStatic("separator");

static const std::string defaultTopDefName = "__topdef__";
static const std::string defaultTopInstName = "__topinst__";

CommandInterpreter::CommandInterpreter(ParserTables& tables, PTControl& control, Circuit& circuit) 
    : at_(0), printProgress_(true), runPostprocess_(true), 
      abortOnMatch(false), tables_(tables), control_(control), circuit_(circuit) {
    abortCommands.insert(idAnalysis);
    clearVariables();
}

CommandInterpreter::~CommandInterpreter() {
}

bool CommandInterpreter::clearVariables(Status& s) {
    circuit_.clearVariables();
    auto& pythonBinary = Platform::pythonExecutable();
    if (pythonBinary.size()>0) {
        auto [ok, var_changed] = circuit_.setVariable("PYTHON", pythonBinary, s);
        return ok;
    }
    return true;
}

bool CommandInterpreter::addAbort(Id cmd) {
    if (cmd==idAnalysis || commandDescriptors.contains(cmd)) {
        abortCommands.insert(cmd);
        return true;
    } else {
        return false;
    }
}
void CommandInterpreter::clearAborts() {
    abortCommands.clear();
}

void CommandInterpreter::setAbortOnMatch(bool b) {
    abortOnMatch = b;
}

bool CommandInterpreter::mustAbort(Id cmd) {
    auto found = abortCommands.contains(cmd);
    if (abortOnMatch && found) {
        return true;
    } else if (!abortOnMatch && !found) {
        return true;
    }
    return false;
}

bool CommandInterpreter::defaultElaboration(Status& s) {
    if (circuit_.checkFlags(Circuit::Flags::Elaborated)) {
        return true;
    }

    // Elaborate default toplevel definition
    return elaborate({}, defaultTopDefName, defaultTopInstName, s); 
}

bool CommandInterpreter::minimalElaboration(Status& s) {
    if (circuit_.checkFlags(Circuit::Flags::Elaborated)) { 
        // Already elaborated, elaborate changes only
        auto [ok, hierarchyChanged, mappingChanged] = circuit_.elaborateChanges(&userOptions_, nullptr, s);
        return ok;
    } else {
        // Not elaborated yet, default elaboration
        return elaborate({}, defaultTopDefName, defaultTopInstName, s); 
    }
}

bool CommandInterpreter::elaborate(const std::vector<Id>& names, const std::string& topDefName, const std::string& topInstName, Status& s) {
    // Elaborate circuit
    // TODO: for now ignore devReq and Abort, Finish, Stop
    IStruct<SimulatorOptions> opt;
    RpnEvaluationNetlistContext ctx;
    if (auto [ok, changed] = opt.setParameters(userOptions_, circuit_.variableEvaluator(), ctx, Parameterized::Write::All, s); !ok) {
        return false;
    }
    circuit_.setOptions(opt);
    // Elaborate needs options, otherwise it sets all options to default
    return circuit_.elaborate(names, topDefName, topInstName, nullptr, s); 
}

InterpreterExitStatus CommandInterpreter::run(size_t from, Status& s) {
    for(at_ = from; at_<control_.size(); at_++) {
        auto& entry = control_[at_];
        if (std::holds_alternative<PTAnalysis>(entry)) {
            if (!minimalElaboration(s)) {
                return InterpreterExitStatus::Error;
            }
            
            // Create analysis
            auto& ptAn = std::get<PTAnalysis>(entry);
            if (printProgress_ && circuit_.paramEvaluator().mcData()==nullptr) {
                Simulator::dbg() << "Running analysis '"+std::string(ptAn.name())+"'.\n";
            }
            Status tmps;
            auto* an = Analysis::create(ptAn, circuit_, tmps);
            if (!an) {
                if (!mustAbort(idAnalysis)) {
                    Simulator::err() << tmps.message() << "\n";
                    continue;
                } else {
                    s.set(tmps);
                    return InterpreterExitStatus::Error;
                }
            }

            // Set file name prefix
            an->setFileNamePrefix(analysisNamePrefix_);

            // Add options (expressions)
            an->add(userOptions_);

            // Add saves
            for(auto& s : commonSaves_) {
                an->add(s);
            }
            
            // Analysis status message
            tmps.clear();
            // Progress reporter
            AnalysisProgress progress(2, Simulator::dbg(), 0.1);
            // Mark start
            progress.begin();
            // Install progress reporter, but only if we are not in a MC loop
            if (printProgress_ && progress.enabled() && circuit_.paramEvaluator().mcData()==nullptr) {
                // Install progress reporter
                an->add(&progress);
            }
            // Run analysis
            auto [ret, can_resume] = an->run(tmps);
            // Mark end time
            progress.end();
            // Print final report
            if (printProgress_&& circuit_.paramEvaluator().mcData()==nullptr) {
                if (progress.enabled()) {
                    // Progress reporter enabled
                    // Report for one final time, force it
                    progress.report(true);
                    Simulator::dbg() << "\n" << std::flush;
                } else {
                    // Progress reporter disabled
                    Simulator::dbg() << "  Elapsed time: "<< progress.time() << "\n";
                }
            }
            if (can_resume) {
                Simulator::dbg() << "Analysis can be resumed.\n" << std::flush;
            }
            
            // Error reporting
            if (!ret) {
                delete an;
                if (!mustAbort(idAnalysis)) {
                    Simulator::err() << tmps.message() << "\n";
                } else {
                    s.set(tmps);
                    return InterpreterExitStatus::Error;
                }
            } else {
                delete an;
            }
        } else {
            auto& cmd = std::get<PTCommand>(entry);
            auto it = commandDescriptors.find(cmd.name());
            if (it==commandDescriptors.end()) {
                s.set(Status::NotFound, "Command not found.");
                s.extend(cmd.location());
                return InterpreterExitStatus::HardFault;
            }
            auto& desc = it->second;
                        
            if (cmd.keywords().size()<desc.minKw) {
                s.set(Status::BadArguments, "Too few keywords given. Expecting at least "+std::to_string(desc.minKw)+".");
                s.extend(cmd.location());
                return InterpreterExitStatus::HardFault;
            }
            if (cmd.keywords().size()>desc.maxKw) {
                s.set(Status::BadArguments, "Too many keywords given. Expecting at most "+std::to_string(desc.maxKw)+".");
                s.extend(cmd.location());
                return InterpreterExitStatus::HardFault;
            }
            if (cmd.expressions().size()<desc.minExpr) {
                s.set(Status::BadArguments, "Too few expressions specified. Expecting at least "+std::to_string(desc.minExpr)+".");
                s.extend(cmd.location());
                return InterpreterExitStatus::HardFault;
            }
            if (cmd.expressions().size()>desc.maxExpr) {
                s.set(Status::BadArguments, "Too many expressions specified. Expecting at most "+std::to_string(desc.maxExpr)+".");
                s.extend(cmd.location());
                return InterpreterExitStatus::HardFault;
            }
            if (desc.limitArgs) {
                for(auto& it : cmd.args().values()) {
                    if (!desc.allowedArgs.contains(it.name())) {
                        s.set(Status::BadArguments, "Command has no keyword argument named '"+std::string(it.name())+"'.");
                        s.extend(it.location());
                        return InterpreterExitStatus::HardFault;
                    }
                }
                for(auto& it : cmd.args().expressions()) {
                    if (!desc.allowedArgs.contains(it.name())) {
                        s.set(Status::BadArguments, "Command has no keyword argument named '"+std::string(it.name())+"'.");
                        s.extend(it.location());
                        return InterpreterExitStatus::HardFault;
                    }
                }
            }
            
            Status tmps;
            auto retStatus = desc.func(*this, cmd, tmps);
            // Exit on hard faults
            if (retStatus==InterpreterExitStatus::HardFault) {
                s.set(tmps);
                s.extend(cmd.location());
                return retStatus;
            }
            // Exit requested by endmc
            if (retStatus==InterpreterExitStatus::RequestMCExit) {
                // Here we should look for last statement of the loop.
                // Because endmc is the statement that triggered the exit and 
                // also the last statement, we do nothing else. 
                return retStatus;
            }
            // Other faults can be masked
            if (retStatus!=InterpreterExitStatus::OK) {
                s.extend(cmd.location());

                if (!mustAbort(cmd.name())) {
                    Simulator::err() << tmps.message() << "\n";
                } else {
                    s.set(tmps);
                    return InterpreterExitStatus::Error;
                }
            }
        }
    }
    return InterpreterExitStatus::EndReached;
}

void CommandInterpreter::addUserOption(const PTParameterValue& pv) {
    userOptions_.add(pv);
}

void CommandInterpreter::addUserOption(const PTParameterExpression& pe) {
    userOptions_.add(pe);
}


template<typename T> bool evaluateExpressions(RpnEvaluator& e, RpnEvaluationNetlistContext& ctx, const PTCommand& cmd, std::vector<T>& out, Status& s) {
    out.clear();
    size_t i=0;
    for(auto& it : cmd.expressions()) {
        Value v;
        if (!e.evaluate(it, v, ctx, s)) {
            return false;
        }
        Value::Type t;
        if constexpr(std::is_same<T, Id>::value) {
            t = Value::Type::String;
        } else if constexpr(std::is_same<T, Value>::value) {
            t = Value::Type::Value;
        } else {
            t = Value::typeCode<T>();
        }
        if constexpr(!std::is_same<T, Value>::value) {
            if (!v.convertInPlace(t, s)) {
                s.extend("Expression "+std::to_string(i)+" does not evaluate to a value of type '"+Value::typeCodeToName(t)+"'.");
                return false;
            }
        }
        if constexpr(std::is_same<T, Id>::value) {
            out.push_back(v.val<String>());
        } else if constexpr(std::is_same<T, Value>::value) {
            out.push_back(std::move(v));
        } else {
            out.push_back(v.val<T>());
        }
        i++;
    }
    return true;
}

void CommandInterpreter::dumpSaves(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');    
    int cnt = 0;
    for(auto&s : commonSaves_) {
        if (cnt==0) {
            os << pfx;
        }
        os << s << " ";
        cnt++;
        if (cnt==10) {
            os << "\n"; 
            cnt = 0;
        }
    }
    if (cnt==0 && commonSaves_.size()>0) {
        os << "\n"; 
    }
}

void CommandInterpreter::dumpOptionsMap(int indent, std::ostream& os) const {
    std::string pfx = std::string(indent, ' ');    
    for(auto& it : userOptions_) {
        if (std::holds_alternative<const PTParameterValue*>(it.second)) {
            auto p = std::get<const PTParameterValue*>(it.second);
            os << pfx << std::string(p->name()) << " = " << p->val() << "\n";
        } else {
            auto p = std::get<const PTParameterExpression*>(it.second);
            os << pfx << std::string(p->name()) << " = " << p->rpn().str() << "\n";
        }
    }
}

bool evaluateArgs(const PTCommand& cmd, RpnEvaluator& evaluator, RpnEvaluationNetlistContext& ctx, std::unordered_map<Id, Value>& out, Status& s=Status::ignore) {
    out.clear();
    for(auto& it : cmd.args().values()) {
        out[it.name()] = it.val();
    }
    for(auto& it : cmd.args().expressions()) {
        Value v;
        if (!evaluator.evaluate(it.rpn(), v, ctx, s)) {
            return false;
        }
        out[it.name()] = std::move(v);
    }
    return true;
}


InterpreterExitStatus cmd_postprocess(CommandInterpreter& interpreter, PTCommand& cmd, Status& s) {
    if (!interpreter.postprocessingAllowed()) {
        return InterpreterExitStatus::OK;
    }

    std::string prog;
    std::vector<std::string> args;

    bool first = true;
    RpnEvaluationNetlistContext ctx;
    for(auto& it : cmd.expressions()) {
        Value v;
        if (!interpreter.variableEvaluator().evaluate(it, v, ctx, s)) {
            return InterpreterExitStatus::Error;
        }
        if (v.type()!=Value::Type::String) {
            if (first) {
                s.set(Status::BadArguments, "Program name must be a string.");
            } else {
                s.set(Status::BadArguments, "Program arguments must be strings.");
            }
            return InterpreterExitStatus::Error;
        }
        if (first) {
            prog = v.val<String>();
            first = false;
        } else {
            args.push_back(v.val<String>());
        }
    }

    auto [ok, out, err] = runProcess(prog, args, &(Platform::pythonPath()), false, Simulator::fileDebug(), s);
    return ok ? InterpreterExitStatus::OK : InterpreterExitStatus::Error;
}

InterpreterExitStatus cmd_abort(CommandInterpreter& interpreter, PTCommand& cmd, Status& s) {
    auto kw = cmd.keywords()[0].name();
    interpreter.clearAborts();
    if (kw==idAlways) {
        // Abort if command is not matched
        interpreter.setAbortOnMatch(false);
        return InterpreterExitStatus::OK;
    } else if (kw==idExcept) {
        // Abort on all commands except the listed ones
        interpreter.setAbortOnMatch(false);
    } else if (kw==idNever) {
        // Never abort
        interpreter.setAbortOnMatch(true);
        return InterpreterExitStatus::OK;
    } else if (kw==idOn) {
        // Abort on listed commands
        interpreter.setAbortOnMatch(true);
    } else {
        s.set(Status::BadArguments, "Unknown keyword '"+std::string(kw)+"'.");
        return InterpreterExitStatus::HardFault;
    }

    // On, except
    auto it=cmd.keywords().begin();
    for(++it; it!=cmd.keywords().end(); ++it) {
        if (!interpreter.addAbort(it->name())) {
            // Failed to add (not a command)
            s.set(Status::BadArguments, "Unknown command '"+std::string(it->name())+"'.");
            return InterpreterExitStatus::HardFault;
        }
    }

    // Two cases left: on, except
    interpreter.setAbortOnMatch(kw==idOn);

    return InterpreterExitStatus::OK;
}

InterpreterExitStatus cmd_clear(CommandInterpreter& interpreter, PTCommand& cmd, Status& s) {
    if (cmd.keywords().size()==0) {
        // Clear all
        interpreter.clearVariables();
        interpreter.clearSaves(); 
        interpreter.clearUserOptions();
    }
    for(auto& it : cmd.keywords()) {
        if (it.name()==idVariables) {
            interpreter.clearVariables();
        } else if (it.name()==idSaves) {
            interpreter.clearSaves(); 
        } else if (it.name()==idOptions) { 
            interpreter.clearUserOptions();
        } else {
            s.set(Status::BadArguments, "Unknown keyword '"+std::string(it.name())+"'.");
            return InterpreterExitStatus::HardFault;
        }
    }
    return InterpreterExitStatus::OK;
}

InterpreterExitStatus cmd_var(CommandInterpreter& interpreter, PTCommand& cmd, Status& s) {
    auto& circuit = interpreter.circuit();

    // Computed value storage
    auto& ex = cmd.args().expressions();
    auto n = cmd.args().expressionCount();
    std::vector<Value> ve(n);

    // Go through keywords, evaluate expressions
    RpnEvaluationNetlistContext ctx;
    for(decltype(n) i=0; i<n; i++) { 
        if (!interpreter.variableEvaluator().evaluate(ex[i].rpn(), ve[i], ctx, s)) {
            return InterpreterExitStatus::Error;
        }
    }

    // Write values
    for(auto& it : cmd.args().values()) { 
        auto [ok, var_changed] = circuit.setVariable(it.name(), it.val(), s);
        if (!ok) {
            return InterpreterExitStatus::Error;
        }
    }

    // Write computed expressions
    for(decltype(n) i=0; i<n; i++) { 
        auto [ok, var_changed] = circuit.setVariable(ex[i].name(), ve[i], s);
        if (!ok) {
            return InterpreterExitStatus::Error;
        }
    }
    
    return InterpreterExitStatus::OK;
}

InterpreterExitStatus cmd_save(CommandInterpreter& interpreter, PTCommand& cmd, Status& s) {
    interpreter.addSaves(cmd.saves());
    return InterpreterExitStatus::OK;
}

InterpreterExitStatus cmd_options(CommandInterpreter& interpreter, PTCommand& cmd, Status& s) {
     // Go through keywords, store values
    auto& cx = interpreter.variableEvaluator().contextStack();
    for(auto& it : cmd.args().values()) { 
        interpreter.addUserOption(it);
    }
    // Go through keywords, evaluate and store expressions
    for(auto& it : cmd.args().expressions()) { 
        interpreter.addUserOption(it);
    }
    return InterpreterExitStatus::OK;
}

InterpreterExitStatus cmd_alter(CommandInterpreter& interpreter, PTCommand& cmd, Status& s) {
    auto& circuit = interpreter.circuit();

    // If circuit is not elaborated, peform default elaboration, otherwise do nothing
    if (!interpreter.defaultElaboration(s)) {
        return InterpreterExitStatus::HardFault;
    }

    auto& ev = circuit.variableEvaluator();
    
    std::string prog;
    
    Parameterized* obj;
    auto what = cmd.keywords()[0].name(); 
    if (what==idModel || what==idInstance) {
    } else {
        s.set(Status::BadArguments, "Unknown entity type '"+std::string(what)+"'.");
        return InterpreterExitStatus::HardFault;
    }

    std::vector<std::string> objNames;
    RpnEvaluationNetlistContext ctx;
    for(auto& it : cmd.expressions()) {
        Value v;
        if (!ev.evaluate(it, v, ctx, s)) {
            return InterpreterExitStatus::Error;
        }
        if (v.type()!=Value::Type::String) {
            s.set(Status::BadArguments, "Entity name must be a string.");
            return InterpreterExitStatus::Error;
        }
        objNames.push_back(v.val<String>());
    }

    if (what==idModel) {
        for(auto& modelName : objNames) {
            if (!circuit.setModelParameters(modelName, cmd.args(), s)) {
                return InterpreterExitStatus::Error;
            }
        }
    } else if (what==idInstance) {
        for(auto& instanceName : objNames) {
            if (!circuit.setInstanceParameters(instanceName, cmd.args(), s)) {
                return InterpreterExitStatus::Error;
            }
        }
    }
    
    return InterpreterExitStatus::OK;
}

InterpreterExitStatus cmd_elaborate(CommandInterpreter& interpreter, PTCommand& cmd, Status& s) {
    auto& circuit = interpreter.circuit();

    // Get keyword
    auto what = cmd.keywords()[0].name();
    if (what==idCircuit) {
        // Get definition names as ids
        std::vector<Id> names;
        RpnEvaluationNetlistContext ctx;
        if (!evaluateExpressions(interpreter.variableEvaluator(), ctx, cmd, names, s)) {
            return InterpreterExitStatus::HardFault;
        }
        
        // Get topdef and topinst
        std::unordered_map<Id, Value> args;
        if (!evaluateArgs(cmd, interpreter.variableEvaluator(), ctx, args, s)) {
            return InterpreterExitStatus::HardFault;
        }

        std::string topDefName; 
        auto it1 = args.find("topdef");
        if (it1!=args.end()) {
            if (it1->second.type()==Value::Type::String) {
                topDefName = it1->second.val<String>();
            } else {
                s.set(Status::BadArguments, "topdef must be a string.");
                return InterpreterExitStatus::HardFault;
            }
        } else {
            topDefName = defaultTopDefName; 
        }

        std::string topInstName; 
        auto it2 = args.find("topinst");
        if (it2!=args.end()) {
            if (it2->second.type()==Value::Type::String) {
                topInstName = it2->second.val<String>();
            } else {
                s.set(Status::BadArguments, "topinst must be a string.");
                return InterpreterExitStatus::HardFault;
            }
        } else {
            topInstName = defaultTopInstName;
        }

        // Elaborate circuit
        return interpreter.elaborate(names, topDefName, topInstName, s) ? InterpreterExitStatus::OK : InterpreterExitStatus::HardFault;
    } else if (what==idChanges) {
        if (cmd.expressions().size()>0 || cmd.args().count()>0) {
            s.set(Status::BadArguments, "Elaboration of changes takes no expressions nor arguments.");
            return InterpreterExitStatus::HardFault;
        }
        // Peform minimal elaboration in case circuit is not elaborated yet
        return interpreter.minimalElaboration(s) ? InterpreterExitStatus::OK : InterpreterExitStatus::HardFault;
    } else {
        s.set(Status::NotFound, "Unknown keyword '"+std::string(what)+"'.");
        return InterpreterExitStatus::HardFault;
    }
    return InterpreterExitStatus::OK;
}

InterpreterExitStatus cmd_print(CommandInterpreter& interpreter, PTCommand& cmd, Status& s) {
    auto& circuit = interpreter.circuit();

    // Default elaboration if not elaborated yet, otherwise elaborate changes
    if (!interpreter.minimalElaboration(s)) {
        return InterpreterExitStatus::HardFault;
    }

    auto trailingNewline = true;
    if (cmd.keywords().size()>0) {
        auto what = cmd.keywords()[0].name();
        if (what=="device_files") {
            Simulator::out() << "Device files:\n";
            for(auto f : OsdiFile::files()) {
                Simulator::out() << "  " << f->fileName() << "\n";
            }
        } else if (what=="device_file") {
            // Evaluate arguments list
            std::vector<String> sVec;
            RpnEvaluationNetlistContext ctx;
            if (!evaluateExpressions(interpreter.variableEvaluator(), ctx, cmd, sVec, s)) {
                return InterpreterExitStatus::Error;
            }
            // Go through strings
            for(auto& pat : sVec) {
                // Scan all device files, match substring in device file name
                for(auto f : OsdiFile::files()) {
                    if (f->fileName().find(pat)==std::string::npos) {
                        // Skip
                        continue;
                    }
                    Simulator::out() << "Device file: " << f->fileName() << "\n";
                    f->dump(2, Simulator::out());
                }
            }
        } else if (what=="counts") {
            Simulator::out() << "Circuit complexity:\n";
            circuit.dumpDeviceCounts(2, Simulator::out());
        } else if (what=="devices") {
            Simulator::out() << "Devices:\n";
            circuit.dumpDevices(2, Simulator::out());
        } else if (what=="models") {
            Simulator::out() << "Models:\n";
            circuit.dumpModels(2, Simulator::out());
        } else if (what=="variables") {
            // Prints current state of circuit's variables
            Simulator::out() << "Variables:\n";
            interpreter.variableEvaluator().contextStack().at(0).dump(2, Simulator::out());
        } else if (what=="saves") {  
            Simulator::out() << "Saves:\n"; 
            interpreter.dumpSaves(2, Simulator::out()); 
        } else if (what=="options") {
            Simulator::out() << "Options:\n";
            interpreter.dumpOptionsMap(2, Simulator::out()); 
        } else if (what=="options_state") {
            // Prints current state of circuit's options
            Simulator::out() << "Options state:\n";
            circuit.dumpOptions(2, Simulator::out());
        } else if (what=="hierarchy") {
            Simulator::out() << "Hierarchy:\n";
            circuit.dumpHierarchy(2, Simulator::out());
        } else if (what=="nodes") {
            Simulator::out() << "Nodes:\n";
            circuit.dumpNodes(2, Simulator::out());
        } else if (what=="unknowns") {
            Simulator::out() << "Unknowns:\n";
            circuit.dumpUnknowns(2, Simulator::out());
        } else if (what=="tolerances") {
            CommonData commons;
            commons.fromOptions(circuit.simulatorOptions().core());
            if (!circuit.setStaticTolerances(commons, s)) {
                s.extend("Failed to retrieve tolerances.");
                return InterpreterExitStatus::Error;
            }
            Simulator::out() << "Tolerances for unknowns/residuals:\n";
            circuit.dumpTolerances(2, commons, Simulator::out());
        } else if (what=="sparsity") {
            Simulator::out() << "Sparsity:\n";
            circuit.dumpSparsity(2, Simulator::out());
        } else if (what=="instance" || what=="model" || what=="device") {
            std::vector<Id> idVec;
            RpnEvaluationNetlistContext ctx;
            if (!evaluateExpressions(interpreter.variableEvaluator(), ctx, cmd, idVec, s)) {
                return InterpreterExitStatus::Error;
            }
            for(auto id : idVec) {
                if (what=="instance") {
                    auto obj = circuit.findInstance(id);
                    if (!obj) {
                        s.set(Status::NotFound, "Instance '"+std::string(id)+"' not found.");
                        return InterpreterExitStatus::Error;
                    }
                    obj->dump(0, circuit, Simulator::out());
                } else if (what=="model") {
                    auto obj = circuit.findModel(id);
                    if (!obj) {
                        s.set(Status::NotFound, "Model '"+std::string(id)+"' not found.");
                        return InterpreterExitStatus::Error;
                    }
                    obj->dump(0, Simulator::out());
                } else {
                    auto obj = circuit.findDevice(id);
                    if (!obj) {
                        s.set(Status::NotFound, "Device '"+std::string(id)+"' not found.");
                        return InterpreterExitStatus::Error;
                    }
                    obj->dump(0, Simulator::out());
                }
            }
        } else if (what=="stats") {
            auto n = circuit.unknownCount();
            auto nnodes = circuit.nodeCount();
            auto nnz = circuit.sparsityMap().size();
            
            Simulator::out() << "System stats:\n";
            Simulator::out() << "  Low-level instances:             " << (circuit.instanceCount()-circuit.subcircuitInstanceCount()) << "\n";
            Simulator::out() << "  Subcircuit instances:            " << circuit.subcircuitInstanceCount() << "\n";
            Simulator::out() << "  Number of nodes:                 " << nnodes << "\n";
            Simulator::out() << "  Number of unknowns:              " << n << "\n";
            Simulator::out() << "  Initial number of nonzeros:      " << nnz << "\n";
            Simulator::out() << "  Initial sparsity:                " << (1.0*nnz/n/n) << "\n";

            Simulator::out() << "\n";

            Simulator::out() << "Clock resolution [ns]:             " << Accounting::resolution()*1e9 << "\n";

            Simulator::out() << "\n";

            Simulator::out() << "Stats:\n";
            interpreter.tables().accounting().dumpTotal(2, Simulator::out());
            interpreter.tables().accounting().dumpDevTimes(2, Simulator::out(), circuit);
        } else if (what=="rpn") {
            // Print RPN of expressions (one per line) for testing purposes
            for(auto& it : cmd.expressions()) {
                auto x = it.str();
                Simulator::out() << it.str() << "\n"; 
            } 
            trailingNewline = false;
        } else {
            s.set(Status::NotFound, "Unknown keyword '"+std::string(what)+"'.");
            return InterpreterExitStatus::HardFault;
        }
    } else {
        // Expressions
        std::string eolString = "\n";
        std::string separatorString = " ";
        if (cmd.args().count()>0) {
            std::unordered_map<Id, Value> args;
            RpnEvaluationNetlistContext ctx;
            if (!evaluateArgs(cmd, interpreter.variableEvaluator(), ctx, args, s)) {
                return InterpreterExitStatus::Error;
            }
            for (const auto& [id, value] : args) {
                // use id and value
                if (id == idEol) {
                    if (value.type()!=Value::Type::String) {
                        s.set(Status::BadArguments, "eol must be a string.");
                        return InterpreterExitStatus::HardFault;
                    }
                    eolString = value.val<const String>();
                } else if (id == idSeparator) {
                    if (value.type()!=Value::Type::String) {
                        s.set(Status::BadArguments, "separator must be a string.");
                        return InterpreterExitStatus::HardFault;
                    }
                    separatorString = value.val<const String>();
                } else {
                    s.set(Status::BadArguments, "Unknown keyword argument '"+std::string(id)+"'.");
                    return InterpreterExitStatus::HardFault;
                }
            }
        }
        std::vector<Value> values;
        RpnEvaluationNetlistContext ctx;
        if (!evaluateExpressions(interpreter.variableEvaluator(), ctx, cmd, values, s)) {
            return InterpreterExitStatus::Error;
        }
        for(auto& v : values) {
            if (v.type()==Value::Type::String) {
                // String scalars are printed without quotes
                Simulator::out() << v.val<String>();
            } else {
                Simulator::out() << v;
            }
            Simulator::out() << separatorString;
        }
        Simulator::out() << eolString;
        trailingNewline = false;
    }
    if (trailingNewline) {
        Simulator::out() << "\n";
    }
    return InterpreterExitStatus::OK;
}

InterpreterExitStatus cmd_mc(CommandInterpreter& interpreter, PTCommand& cmd, Status& s) {
    // Are we inside a mc loop
    if (interpreter.circuit().paramEvaluator().mcData()!=nullptr) {
        s.set(Status::Syntax, "Nested Monte Carlo loops are not allowed.");
        return InterpreterExitStatus::HardFault;
    }

    // Need command index to know where to start the mc loop
    auto loopStart = interpreter.at() + 1;

    // Get mc name
    auto mcName = cmd.keywords()[0].name();

    // Check uniqueness
    if (!interpreter.isUniqueMc(mcName)) {
        s.set(Status::Syntax, "Monte Carlo loops must have unique names.");
        return InterpreterExitStatus::HardFault;
    }

    // Evaluate arguments
    std::unordered_map<Id, Value> args;
    RpnEvaluationNetlistContext ctx;
    if (!evaluateArgs(cmd, interpreter.variableEvaluator(), ctx, args, s)) {
        return InterpreterExitStatus::Error;
    }

    // Prepare MC data
    MCData mcData;

    // Set seed
    auto it1 = args.find("seed");
    if (it1!=args.end()) {
        auto seed = it1->second;
        if (!seed.convertInPlace(Value::Type::Int, s)) {
            s.extend("Seed must be an integer.");
            return InterpreterExitStatus::HardFault;
        }
        mcData.setSeed(seed.val<IntegerValue>());
    } else {
        // Default seed
        mcData.setSeed(0);
    }
    
    // Get number of samples
    auto it2 = args.find("samples");
    if (it2==args.end()) {
        s.extend("Need number of samples.");
        return InterpreterExitStatus::HardFault;
    }
    auto nsv = it2->second;
    if (!nsv.convertInPlace(Value::Type::Int, s)) {
        s.extend("Seed must be an integer.");
        return InterpreterExitStatus::HardFault;
    }
    auto nsamples = nsv.val<IntegerValue>();
    if (nsamples<=0) {
        s.extend("Number of samples must be greater than zero.");
        return InterpreterExitStatus::HardFault;
    }

    // Put paramEvaluator into MC mode
    interpreter.circuit().paramEvaluator().setMCData(&mcData);

    if (interpreter.printProgress()) {
        Simulator::dbg() << "Running MC analysis '"+std::string(mcName)+"'.\n";
    }

    // Progress reporter
    AnalysisProgress progress(2, Simulator::dbg(), 0.1);
    // Mark start
    progress.begin();

    progress.setValueFormat(ProgressReporter::ValueFormat::Fixed, 0);
    progress.setValueDecoration("sample# ", "");
    progress.initProgress(nsamples+1, 0);
    progress.report();
            

    // Main loop
    bool abort = false;
    for(decltype(nsamples) i=0; i<nsamples; i++) {
        // Set variables
        interpreter.circuit().setVariable("MCSAMPLE", i+1);

        // Set analysis name prefix
        interpreter.setAnalysisNamePrefix(std::string(mcName)+"."+std::to_string(i+1)+".");
    
        // Generate new sample if this is not the first iteration
        if (i>0) {
            mcData.advance();
        }

        // Propagate parameters. In first iteration this populates the list of generators 
        // and implicitly generates a sample. 
        // Force global propagation by setting VariablesChanged flag
        interpreter.circuit().setFlags(Circuit::Flags::VariablesChanged);
        // Elaborate changes
        if (!interpreter.minimalElaboration(s)) {
            abort = true;
            s.set(Status::Syntax, "Elaboration failed, Monte Carlo loop aborted.");
            break;
        }

        // if (i==0) {
        //     mcData.dump(0, Simulator::out());
        // }

        // Run loop body
        auto exitStatus = interpreter.run(loopStart, s);
        // Hard faults abort
        if (exitStatus==InterpreterExitStatus::HardFault) {
            abort = true;
            break;
        }
        // Errors abort. If needed they were ignored in the nested interpreter loop. 
        // Exit status other than RequestMCExit mean that we reached the end without an endmc
        if (exitStatus==InterpreterExitStatus::EndReached) {
            abort = true;
            s.set(Status::Syntax, "Monte Carlo loop without endmc.");
            break;
        } else if (exitStatus!=InterpreterExitStatus::RequestMCExit) {
            // Loop finished prematurely
            abort = true;
            break;
        }

        progress.setProgress(i+1, i+1);
        progress.report();
    }
    progress.end();

    // Print final report
    if (interpreter.printProgress()) {
        if (progress.enabled()) {
            // Progress reporter enabled
            // Report for one final time, force it
            progress.report(true);
            Simulator::dbg() << "\n" << std::flush;
        } else {
            // Progress reporter disabled
            Simulator::dbg() << "  Elapsed time: "<< progress.time() << "\n";
        }
    }
    
    if (abort) {
        interpreter.circuit().paramEvaluator().setMCData(nullptr);
        return InterpreterExitStatus::HardFault;
    }

    // Interpreter pointer is now at endmc command
    interpreter.circuit().paramEvaluator().setMCData(nullptr);
    return InterpreterExitStatus::OK;
}

InterpreterExitStatus cmd_endmc(CommandInterpreter& interpreter, PTCommand& cmd, Status& s) {
    if (interpreter.circuit().paramEvaluator().mcData()==nullptr) {
        s.set(Status::Syntax, "endmc outside Monte Carlo loop.");
        return InterpreterExitStatus::HardFault;
    }
    return InterpreterExitStatus::RequestMCExit;
}

std::unordered_map<Id, CommandInterpreter::CmdDesc> CommandInterpreter::commandDescriptors = {
    //                                    keywords           expressions        keyword arguments
    //                                    min max            min max            limit  allowed names
    { Id::createStatic("abort"),        { 1,  CmdDesc::many, 0,  0,             true,  {},          cmd_abort } }, 
    { Id::createStatic("clear"),        { 0,  CmdDesc::many, 0,  0,             true,  {},          cmd_clear } }, 
    { Id::createStatic("save"),         { 0,  0,             0,  0,             true,  {},          cmd_save } }, 
    { Id::createStatic("var"),          { 0,  0,             0,  0,             false, {},          cmd_var } }, 
    { Id::createStatic("options"),      { 0,  0,             0,  0,             false, {},          cmd_options } }, 
    { Id::createStatic("alter"),        { 1,  1,             0,  CmdDesc::many, false, {},          cmd_alter } }, 
    { Id::createStatic("elaborate"),    { 1,  1,             0,  CmdDesc::many, true,  {"topdef", "topinst"}, 
                                                                                                    cmd_elaborate } }, 
    { Id::createStatic("print"),        { 0,  1,             0,  CmdDesc::many, true,  {"eol", "separator"},      
                                                                                                    cmd_print } }, 
    { Id::createStatic("postprocess"),  { 0,  0,             1,  CmdDesc::many, true,  {},          cmd_postprocess } }, 
    { Id::createStatic("mc"),           { 1,  1,             0,  CmdDesc::many, true,  {"samples", "seed"},      
                                                                                                    cmd_mc } }, 
    { Id::createStatic("endmc"),        { 0,  0,             0,  CmdDesc::many, true,  {},          cmd_endmc } }, 
};

// mc mc1 samples=100 seed=1

/*
// Default abort mode: except analysis 
//
// abort all - abort on all errors
// abort on cmd1 cmd2 ... - on error abort only selected commands
// abort never - on error do not abort
// abort except cmd1 cmd2 ... - on error abort all except selected commands
//   analysis is treated as a command
// 
// Variables listed in a var command are first all evaluated and then stored. 
// var var1=... var2=...
//
// Options are stored. They are applied at (*). 
// options name1=... name2=...
//
// clear options reverts to default options. 
// The options (values and expressions) set with options are stored. 
//
// (*) Options and variable changes are applied before print and alter, 
// and at elaboration. 
// 
// clear - clear everything
// clear [var|save|options] - clear selected things (can specify more than one)
//
// save directive1 directive2 ...
// 
// If no circuit is elaborated at print, alter, elaborate changes, or analysis call 
// default elaboration is performed (elaborates only the default toplevel circuit). 
//
// elaborate circut("def1", "def2", ...)
//
// Elaborate changes
// elaborate changes
// 
// alter instance("name1", "name2", ...) p1=... p2=... ...
// alter model("name1", "name2", ...) p1=... p2=... ...
//
// TODO:
//   solution write("name") file=...
//   solution read("name") file=...
//
// [sweep ...] analysis ...  
//
// postprocess ("program", "arg1", ...)
*/

/* 
Roadmap

Paramset rules (Verilog-A)

Paramset has Np parameters that can be set by an instance. 
Instance specifies Ni parameters and connects to Nconnected ports. 
Each parameter has a default value and an optional valid range. 
Parameters specified on an instance of paramset are called overrides. 
Paramset can also have local parameters (can depend on instance parameters). 
Local parameters have an optional valid range specified. 

Phase 1 - paramset selection - create a list of candidates
Use following selection rules. 
- all specified instance parameters must be paramset parameters
- parameters of the paramset must be within their respective ranges
- local parameters of the paramset must be within their respective ranges
- the underlying device must have all the ports to which an instance is connected
  (port count >= specified terminal count)

If number of candidates is 1, we are done. Otherwise enter phase 2. 

A paramset can be queried for following constants
- number of instance parameters Np (already implemented)
- number of local parameters with specified ranges Nls (TODO)
- number of ports: Nports (already implemented)

Phase 2 - compute properties for each candidate paramset
- N1 - number of un-overridden parameters: Np-Ni
- N2 - number of local parameters with specified ranges: Nls
- N3 - number of unconnected ports: Nports-Nconnected

Phase 3 - eliminate candidates, all operations are O(Ncandidates)
- sort according to N1 (lowest first), eliminate all, but top candidates
- sort according to N2 (highest first), eliminate all, but top candidates
- sort according to N3 (lowest first), eliminate all, but top candidates

If there is more than 1 candidate left, signal an error. 

Paramset declaration of group of Verilog-A paramsets with the same name 
(paramset overrides) on netlist. 

paramset <paramset_name>

Each Verilog-A paramset is loaded as a separate device. 
For each Verilog-A paramset one model is created. 
The models are added to a paramset group named <paramset_name>. 

Paramset group defined on netlist level (binning)

Such paramsets have no local parameters with specified ranges (Nls=0).
Paramsets in a group can belong to different devices. 

Defining a paramset, parenthesis is optional

paramset <paramset_group_name> model <model_name> (
  selector_expression1
  selector_expression2  
  ...
)

Selectors are expressions that evaluate to 0 (false) or !=0 (true). 

Selector expressions can call par("name") and modpar("name")
to access instance and model parameters. 
The context stack contains the following contexts
  1. constants
  2. circuit variables
  3. default toplevel instance's context
  4. ancestor toplevel instance's context (if not the same as 3)
  5. parent instance's context

Assume N2=0. 

For a model to pass phase1 its selector expressions must all be true. 

At the end of phase1 all Verilog-A paramsets are eliminated if 
at least one candidate is a paramset defined on netlist level. 

On instance parameter change check to which paramset the instance belongs. 

On model parameter change check to which paramset all of its instances belong. 

If an instance switches to a different paramset there are two options:
- paramset belongs to the same device
  mark instance for rebinning, store new paramset pointer
  the instance's model will be changed, 
  the entity list (instances) for the two models will be rebuilt, 
  the instance will be marked as NeedsSetup
- paramset belongs to a different device
  mark instance for recreation, store new paramset pointer
  similar as subhierarchy change, except that
  the instance will be rebuilt, 
  the parent's subinstance list will be updated, 
  the instance will be marked as NeedsSetup
  
Toplevel instances cannot be instances of a paramset. 

Each instance has a pointer to a paramset group. 
If instance is not a paramset instance the pointer is nullptr. 
*/

}
