#include "coresweep.h"
#include "circuit.h"
#include "introspection.h"
#include "devbase.h"
#include <cmath>

namespace NAMESPACE {

// TODO: speed up parameter change propagation (do it only for changed instances/models)
// TODO: speed up topology rebuild

Id ScalarSweep::modeLin = Id::createStatic("lin");
Id ScalarSweep::modeDec = Id::createStatic("dec"); 
Id ScalarSweep::modeOct = Id::createStatic("oct"); 

ScalarSweep::ScalarSweep() 
    : at_(0), end(0) {
}

Int ScalarSweep::valueIndex() const {
    return at_;
}

void ScalarSweep::reset() {
    at_ = 0;
}

Int ScalarSweep::at() const {
    return at_;
}

Int ScalarSweep::count() const {
    return end;
}

bool ScalarSweep::advance() {
    at_++;
    return at_>=end;
}

std::string ScalarSweep::progress() const {
    return std::to_string(at_+1)+"/"+std::to_string(end);
}

bool ScalarSweep::setupSteppedSweep(Real from_, Real to_, Real step_, ErrorConsumer& errors) {
    from = from_;
    to = to_;
    step = step_;

    if (from<=to && step>0) {
        // Increasing
        double nStepsF = (to - from) / step;
        if (nStepsF-1>std::numeric_limits<Int>::max()) {
            errors.push(SweepStepTooSmall{});
            return false;
        }
        end = std::ceil(nStepsF);
        // Do not neccessarily include 'to' in sweep
        if (end<=0) {
            end = 1;
        }
        if (from+end*step <= to) {
            end += 1;
        }
    } else if (from>to && step<0) {
        // Decreasing
        double nStepsF = (to - from) / step;
        if (nStepsF-1>std::numeric_limits<Int>::max()) {
            errors.push(SweepStepTooSmall{});
            return false;
        }
        end = std::ceil(nStepsF);
        // Do not neccessarily include 'to' in sweep
        if (end<=0) {
            end = 1;
        }
        if (from+end*step >= to) {
            end += 1;
        }
    } else {
        // Error
        errors.push(SweepBadSteppedRange{});
        return false;
    }
    sweepType = SweepType::Stepped;
    return true;
}

bool ScalarSweep::setupValueSweep(const Value& values, ErrorConsumer& errors) {
    vals = &values;
    if (!values.isVector()) {
        errors.push(SweepValuesNotVector{});
        return false;
    }
    if (values.size()<=0) {
        errors.push(SweepValuesEmpty{});
        return false;
    }
    if (values.size()>std::numeric_limits<Int>::max()) {
        errors.push(SweepTooManyValues{});
        return false;
    }
    end = values.size();
    sweepType = SweepType::Value;
    return true;
}

bool ScalarSweep::setupLinearSweep(Real from_, Real to_, Int points, ErrorConsumer& errors) {
    from = from_;
    to = to_;
    if (points<0) {
        errors.push(SweepNegativeIntervals{});
        return false;
    }
    end = points+1;
    sweepType = SweepType::Lin;
    return true;
}

bool ScalarSweep::setupLogSweep(Real from_, Real to_, Real factor_, Int pointsPerFactor, ErrorConsumer& errors) {
    from = from_;
    to = to_;
    factor = factor_;
    if (pointsPerFactor<=0) {
        errors.push(SweepNonPositivePoints{});
        return false;
    }
    if (factor<=0) {
        errors.push(SweepNonPositiveFactor{});
        return false;
    }
    // Logarthmic steps (per decade)
    if (from<=0 || to<=0) {
        errors.push(SweepNonPositiveLogEndpoints{});
        return false;
    }
    // Compute number of points
    auto nStepsF = std::abs(pointsPerFactor * std::log(to / from)/std::log(factor));
    if (nStepsF-2>std::numeric_limits<Int>::max()) {
        errors.push(SweepTooManyPoints{});
        return false;
    }
    end = std::ceil(nStepsF) + 1;
    sweepType = SweepType::Log;
    return true;
}

bool ScalarSweep::compute(Value& v, ErrorConsumer& errors) const {
    switch (sweepType) {
        case SweepType::Stepped:
            v = from + step*at_;
            return true;
        case SweepType::Value: {
            Status st;
            if (!vals->getScalar(v, at_, st)) {
                errors.push(SweepValueReadDetail{st.message()});
                errors.push(SweepValueNotReadable{static_cast<long>(at_)});
                return false;
            }
            return true;
        }
        case SweepType::Lin:
            if (end<2) {
                v = from;
            } else {
                v = from + (to - from)/(end-1)*at_;
            }
            return true;
        case SweepType::Log:
            if (end<2) {
                v = from;
            } else {
                v = std::exp(
                    std::log(from)+
                    (std::log(to) - std::log(from)) / (end-1) * at_
                );
            }
            return true;
    }
    return false;
}


SweepSettings::SweepSettings() {
    location = Loc::bad;
}

// Introspection for options structure
template<> int Introspection<SweepSettings>::setup() {
    registerMember(instance);
    registerMember(model);
    registerMember(parameter);
    registerMember(option);
    registerMember(variable);
    registerMember(component);
    registerMember(from);
    registerMember(to);
    registerMember(step);
    registerMember(mode);
    registerMember(points);
    registerMember(values);
    registerMember(continuation);
    
    return 0;
}
instantiateIntrospection(SweepSettings);

ParameterSweeper::ParameterSweeper(Circuit& circuit, const std::vector<PTSweep>& ptSweeps) 
    : circuit(circuit), ptSweeps(ptSweeps), sweepPos(0) {
}

bool ParameterSweeper::setup(ErrorConsumer& errors) {
    // Number of sweeps
    auto n = ptSweeps.size();

    // Clear sweep settings
    settings.clear();

    // Resize settings
    settings.resize(n);

    // Clear family vector, scalar sweeps vector
    parameterFamily.clear();
    scalarSweeps.clear();

    // Resize scalar sweeps vector
    scalarSweeps.resize(n);
    
    // Sweep settings can depend on circuit variables. 
    // If during sweep a variable changes the change is applied to all inner sweeps 
    // relative to the sweep causing the variable change. 
    double extent = 1;
    RpnEvaluationNetlistContext ctx;
    for(decltype(n) i=0; i<n; i++) {
        auto& ptcomp = ptSweeps[i];
        auto& comp = settings[i];
        
        // Evaluate settings
        IStruct<SweepSettings> sw;
        sw.core().name = ptcomp.name();
        sw.core().location = ptcomp.location();
        Status st;
        auto [ok, changed] = sw.setParameters(ptSweeps[i].parameters(), circuit.variableEvaluator(), ctx, st);
        if (!ok) {
            errors.push(SweepSettingsEvalDetail{st.message()});
            errors.push(SweepSettingsEvalError{ptcomp.name()});
            errors.push(SweepLocation{ptcomp.location()});
            return false;
        }
        comp = std::move(sw.core());

        // Vector component sweeps not supported yet
        if (comp.component>=0) {
            errors.push(SweepVectorComponentUnsupported{comp.name});
            errors.push(SweepLocation{comp.location});
            return false;
        }

        // Check if we have anything to sweep
        int specCount = 0;
        if (comp.variable) {
            // variable given, sweep circuit variable
            specCount++;
            parameterFamily.push_back(ParameterFamily::Variable);
        }
        if (comp.option) {
            // option given, sweep circuit option
            specCount++;
            parameterFamily.push_back(ParameterFamily::Option);
        }
        if (comp.model) {
            // model given, sweep model parameter
            specCount++;
            parameterFamily.push_back(ParameterFamily::Model);
        }
        if (comp.instance) {
            // instance given, sweep instance parameter
            specCount++;
            parameterFamily.push_back(ParameterFamily::Instance);
        }
        if (specCount<=0) {
            // By default we sweep a toplevel instance parameter
            // TODO: implement, currently it results in an error. 
            parameterFamily.push_back(ParameterFamily::Instance);
        }
        if (specCount>1) {
            // Error
            errors.push(SweepMultipleFamilies{comp.name});
            errors.push(SweepLocation{comp.location});
            return false;
        }

        // Setup ScalarSweep
        if (!scalarSweeps[i].setup(comp, errors)) {
            errors.push(SweepSetupFailed{comp.name});
            errors.push(SweepLocation{comp.location});
            return false;
        }
        extent *= scalarSweeps[i].count();
    }
    initProgress(extent, 0);
    return true;
}

bool ParameterSweeper::update(int advancedSweepIndex, ErrorConsumer& errors) {
    // Loop from advancedSweepIndex+1 to end of sweeps
    RpnEvaluationNetlistContext ctx;
    for(Int i=advancedSweepIndex+1; i<settings.size(); i++) {
        // Recompute expressions, update settings structure
        IStruct<SweepSettings> sw;
        sw.core() = settings[i];
        Status st;
        auto [ok, changed] = sw.setParameters(ptSweeps[i].parameters().expressions(), circuit.variableEvaluator(), ctx, st);
        if (!ok) {
            errors.push(SweepSettingsEvalDetail{st.message()});
            return false;
        }
        settings[i] = sw.core();

        // Set up scalar sweep
        auto& swp = settings[i];
        if (!scalarSweeps[i].setup(settings[i], errors)) {
            errors.push(SweepUpdateFailed{swp.name});
            errors.push(SweepLocation{swp.location});
            return false;
        }
    }
    return true;
}

bool ParameterSweeper::bind(Circuit& circuit, IStruct<SimulatorOptions>& opt, ErrorConsumer& errors) {
    circuit_ = &circuit;
    parameterizedObject.clear();
    parameterIndex.clear();
    auto n = settings.size();
    decltype(n) i=0;
    for(auto it=settings.cbegin(); it!=settings.cend(); ++it, i++) {
        if (parameterFamily[i] == ParameterFamily::Variable) {
            // Variables - need to get them via ContextStack because circuit returns only const references
            auto ptr = circuit_->getVariable(it->variable);
            if (!ptr) {
                errors.push(SweepVariableNotFound{it->name, it->variable});
                errors.push(SweepLocation{it->location});
                return false;
            }
            // Variables are always free
            // because they are the ones that are specified as constants
            parameterizedObject.push_back(nullptr);
            parameterIndex.push_back(0);
        } else if (parameterFamily[i] == ParameterFamily::Option) {
            // Simulator option
            auto [ndx, found] = circuit.simulatorOptions().parameterIndex(it->option);
            if (!found) {
                errors.push(SweepOptionNotFound{it->name, it->option});
                errors.push(SweepLocation{it->location});
                return false;
            }
            // Sweeping a simulator options overrides any expression for that option
            // that was specified outside analysis or with analysis
            parameterizedObject.push_back(&opt);
            parameterIndex.push_back(ndx);
        } else if (parameterFamily[i] == ParameterFamily::Model) {
            // Model parameter
            Model* modPtr;
            if (!it->model) {
                // Instance name not given
                errors.push(SweepInstanceNameMissing{it->name});
                errors.push(SweepLocation{it->location});
                return false;
            } else {
                modPtr = circuit.findModel(it->model);
                if (!modPtr) {
                    errors.push(SweepModelNotFound{it->name, it->model});
                    errors.push(SweepLocation{it->location});
                    return false;
                }
                if (!it->parameter) {
                    errors.push(SweepModelParameterMissing{it->name});
                    errors.push(SweepLocation{it->location});
                    return false;
                }
                auto [ndx, found] = modPtr->parameterIndex(it->parameter);
                if (!found) {
                    errors.push(SweepModelParameterNotFound{it->name, it->model, it->parameter});
                    errors.push(SweepLocation{it->location});
                    return false;
                }
                if (!modPtr->parameterIsFree(it->parameter)) {
                    errors.push(SweepModelParameterBound{it->name, it->model, it->parameter});
                    errors.push(SweepLocation{it->location});
                    return false;
                }
                parameterizedObject.push_back(modPtr);
                parameterIndex.push_back(ndx);
            }
        } else if (parameterFamily[i] == ParameterFamily::Instance) {
            // Instance parameter
            // If instance name not given, assume toplevel instance
            Instance* instPtr;
            if (!it->instance) {
                // Instance name not given
                errors.push(SweepInstanceNameMissing{it->name});
                errors.push(SweepLocation{it->location});
                return false;
            } else {
                // Find instance
                instPtr = circuit.findInstance(it->instance);
                if (!instPtr) {
                    errors.push(SweepInstanceNotFound{it->name, it->instance});
                    errors.push(SweepLocation{it->location});
                    return false;
                }
            }
            ParameterIndex paramNdx;
            if (!it->parameter) {
                // Parameter name not given, try principal parameter
                auto [ndx, hasPrincipal] = instPtr->principalParameterIndex();
                if (!hasPrincipal) {
                    errors.push(SweepNoPrincipalParameter{it->name, it->instance});
                    errors.push(SweepLocation{it->location});
                    return false;
                }
                paramNdx = ndx;
            } else {
                auto [ndx, found] = instPtr->parameterIndex(it->parameter);
                if (!found) {
                    errors.push(SweepInstanceParameterNotFound{it->name, it->instance, it->parameter});
                    errors.push(SweepLocation{it->location});
                    return false;
                }
                paramNdx = ndx;
            }
            auto paramName = instPtr->parameterName(paramNdx);
            if (!instPtr->parameterIsFree(paramName)) {
                errors.push(SweepInstanceParameterBound{it->name, it->instance, paramName});
                errors.push(SweepLocation{it->location});
                return false;
            }
            parameterizedObject.push_back(instPtr);
            parameterIndex.push_back(paramNdx);
        }
    }
    return true;
}

bool ParameterSweeper::storeState(ErrorConsumer& errors) {
    auto n = settings.size();
    decltype(n) i=0;
    storedValues.resize(n);
    for(auto it=settings.begin(); it!=settings.end(); ++it, i++) {
        if (parameterFamily[i]==ParameterSweeper::ParameterFamily::Variable) {
            storedValues[i] = *circuit_->getVariable(it->variable);
        } else {
            Status st;
            bool ok = parameterizedObject[i]->getParameter(parameterIndex[i], storedValues[i], st);
            if (!ok) {
                errors.push(SweepParameterReadFailed{it->name});
                errors.push(SweepLocation{it->location});
                return false;
            }
        }
    }
    return true;
}

void ParameterSweeper::reset() { 
    for(auto& it : scalarSweeps) {
        it.reset();
    }
    sweepPos = 0;
    setProgress(sweepPos, sweepPos);
}

std::tuple<bool, Int> ParameterSweeper::advance() {
    // Advance innermost sweep
    Int n = scalarSweeps.size();
    // Count up because i and n are unsigned
    for(decltype(n) i=0; i<n; i++) {
        // Start with innermost sweep
        auto ndx = n-1-i;
        auto finished = scalarSweeps[ndx].advance();
        if (finished) {
            // Reset, move on to outer sweep
            scalarSweeps[ndx].reset();
        } else {
            // No need to reset, done
            incrementedSweepIndex = ndx;
            // Increase counter for progress monitoring
            sweepPos++;
            setProgress(sweepPos, sweepPos);
            return std::make_tuple(false, ndx);
        }
    }
    // Increase counter for progress monitoring
    sweepPos++;
    setProgress(sweepPos, sweepPos);
    // If we reach this point, we have just reset the outermost sweep so we are done
    return std::make_tuple(true, 0);
}

std::string ParameterSweeper::progress() const {
    std::string txt="";
    auto n = scalarSweeps.size();
    for(decltype(n) i=0; i<n; i++) {
        if (i>0) {
            txt += ", ";
        }
        txt += scalarSweeps[i].progress();
        Value v;
        ErrorConsumer sink;
        if (scalarSweeps[i].compute(v, sink)) {
            txt += " ("+v.str()+")";
        }

    }
    return txt;
}

std::tuple<bool, bool> ParameterSweeper::write(ParameterFamily types, WriteValues what, ErrorConsumer& errors) {
    auto n = settings.size();
    bool changed = false;
    // Always write everything
    decltype(n) fromIndex = 0;
    for(decltype(n) i=fromIndex; i<n; i++) {
        // Skip everything we are not supposed to write
        if ((parameterFamily[i] & types) == 0) {
            continue;
        }
        auto it = &settings[i]; 
        Value* vPtr;
        Value v;
        if (what == WriteValues::StoredState) {
            // Get stored value
            vPtr = &(storedValues[i]);
        } else {
            // Compute value
            if (!scalarSweeps[i].compute(v, errors)) {
                return std::make_tuple(false, false);
            }
            vPtr = &v;
        }

        // Write
        if (parameterFamily[i]==ParameterSweeper::ParameterFamily::Variable) {
            Status st;
            auto [ok, ch] = circuit_->setVariable(it->variable, *vPtr, st);
            changed |= ch;
            if (!ok) {
                errors.push(SweepVariableWriteDetail{st.message()});
                return std::make_tuple(false, changed);
            }
        } else {
            Status st;
            auto [ok, ch] = parameterizedObject[i]->setParameter(parameterIndex[i], *vPtr, st);
            changed |= ch;
            if (!ok) {
                errors.push(SweepParameterWriteDetail{st.message()});
                return std::make_tuple(false, changed);
            }
        }
    }
    return std::make_tuple(true, changed);
}

Id ParameterSweeper::sweepName(Int ndx) const {
    return settings[ndx].name;
}
    
Int ParameterSweeper::valueIndex(Int ndx) const {
    return scalarSweeps[ndx].at();
}

bool ParameterSweeper::compute(Int ndx, Value& v, ErrorConsumer& errors) const {
    return scalarSweeps[ndx].compute(v, errors);
}

}
