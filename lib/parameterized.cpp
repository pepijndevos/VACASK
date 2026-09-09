#include "parameterized.h"
#include "simulator.h"
#include "common.h"

namespace NAMESPACE {

Parameterized::Parameterized() {
}

std::tuple<Value::Type,bool> Parameterized::parameterType(Id name, Status& s) const {
    auto [ndx, found] = parameterIndex(name);
    if (!found) {
        s.set(Status::NotFound, std::string("Parameter '")+std::string(name)+"' not found.");
        return std::make_tuple(Value::Type::Int, false);
    }
    return parameterType(ndx, s);
}

bool Parameterized::getParameter(Id name, Value& v, Status& s) const {
    auto [ndx, found] = parameterIndex(name);
    if (!found) {
        s.set(Status::NotFound, std::string("Parameter '")+std::string(name)+"' not found.");
        return false;
    }
    return getParameter(ndx, v, s);
}

std::tuple<bool,bool> Parameterized::setParameter(Id name, const Value& v, Status& s) {
    auto [ndx, found] = parameterIndex(name);
    if (!found) {
        s.set(Status::NotFound, std::string("Parameter '")+std::string(name)+"' not found.");
        return std::make_tuple(false, false);
    }
    return setParameter(ndx, v, s);
}

std::tuple<bool,bool> Parameterized::parameterGiven(ParameterIndex ndx, Status& s) {
    // By default parameter is always given (structure default value is used)
    return std::make_tuple(true, true);
}

std::tuple<bool,bool> Parameterized::parameterGiven(Id name, Status& s) {
    auto [ndx, found] = parameterIndex(name);
    if (!found) {
        s.set(Status::NotFound, std::string("Parameter '")+std::string(name)+"' not found.");
        return std::make_tuple(false, false);
    }
    return parameterGiven(ndx, s);
}

// Return value: true means that we are allowed to skip this parameter evaluation/setting
// Performs an extra name lookup for each parameter
bool Parameterized::skipUnknownParameter(Id name, UnknownParam unknown, const Loc& loc) const {
    // In case of policy=error we do not skipđ
    // Not skipping will trigger the error in the second lookup
    if (unknown==UnknownParam::Error) {
        return false;
    }
    // Look up name, if found we do not skip
    if (auto [ndx, found] = parameterIndex(name); found) {
        return false;
    }
    // Not found, print message if policy=warn
    if (unknown==UnknownParam::Warn) {
        // Same wording and source as the error, except that it is reported as a warning
        Simulator::wrn() << "Warning, parameter '" << std::string(name) << "' not found. Ignored.\n";
        if (loc) {
            Simulator::wrn() << loc.toString() << "\n";
        }
    }
    // Not found, can skip because policy=error already returned false
    return true;
}

std::tuple<bool,bool> Parameterized::setParameters(const std::vector<PTParameterValue>& params, Status& s, UnknownParam unknown) {
    bool changed = false;
    for(auto it=params.cbegin(); it!=params.cend(); ++it) {
        if (skipUnknownParameter(it->name(), unknown, it->location())) {
            continue;
        }
        auto [ok, ch] = setParameter(it->name(), it->val(), s);
        changed = changed | ch;
        if (!ok) {
            s.extend(it->location());
            return std::make_tuple(false, changed);
        }
    }
    return std::make_tuple(true, changed);
}

std::tuple<bool,bool> Parameterized::setParameters(const std::vector<PTParameterExpression>& params, RpnEvaluator& eval, RpnEvaluationNetlistContext& ctx, Status& s, UnknownParam unknown) {
    // Assume the context is already set up in evaluator
    bool changed = false;
    for(auto it=params.cbegin(); it!=params.cend(); ++it) {
        // Drop before evaluating: a parameter that is not going anywhere must not
        // be able to fail the run through its expression either
        if (skipUnknownParameter(it->name(), unknown, it->location())) {
            continue;
        }
        Value res;
        ctx.setParameterId(it->name());
        if (!eval.evaluate(it->rpn(), res, ctx, s)) {
            return std::make_tuple(false, changed);
        }
        auto [ok, ch] = setParameter(it->name(), res, s);
        changed = changed | ch;
        if (!ok) {
            s.extend(it->location());
            return std::make_tuple(false, changed);
        }
    }
    return std::make_tuple(true, changed);
}

std::tuple<bool,bool> Parameterized::setParameters(const PTParameters& params, RpnEvaluator& eval, RpnEvaluationNetlistContext& ctx, Status& s, UnknownParam unknown) {
    auto [ok1, changed] = setParameters(params.values(), s, unknown);
    if (!ok1) {
        return std::make_tuple(false, changed);
    }
    auto [ok2, ch] = setParameters(params.expressions(), eval, ctx, s, unknown);
    changed = changed | ch;
    if (!ok2) {
        return std::make_tuple(false, changed);
    }
    return std::make_tuple(true, changed);
}

std::tuple<bool,bool> Parameterized::setParameters(const PTParameterMap& params, RpnEvaluator& eval, RpnEvaluationNetlistContext& ctx, Write what, Status& s) {
    // Go through parameter map, set values
    bool changed = false;
    for(auto& it : params) {
        if (
            (
                std::holds_alternative<const PTParameterValue*>(it.second) ||
                std::holds_alternative<std::unique_ptr<const PTParameterValue>>(it.second)
            ) && (what==Write::All || what==Write::Values)
        ) {
            // A fixed value
            auto pv = (std::holds_alternative<const PTParameterValue*>(it.second))?
                std::get<const PTParameterValue*>(it.second) : std::get<std::unique_ptr<const PTParameterValue>>(it.second).get();
            auto [ok, ch] = setParameter(it.first, pv->val(), s);
            changed |= ch;
            if (!ok) {
                s.extend(pv->location());
                return std::make_tuple(false, changed);
            }
        } else if (
            (
                std::holds_alternative<const PTParameterExpression*>(it.second) ||
                std::holds_alternative<std::unique_ptr<const PTParameterExpression>>(it.second)
            ) && (what==Write::All || what==Write::Expressions)
        ) {
            // An expression
            auto pe = (std::holds_alternative<const PTParameterExpression*>(it.second))?
                std::get<const PTParameterExpression*>(it.second) 
                : std::get<std::unique_ptr<const PTParameterExpression>>(it.second).get();
            Value res;
            ctx.setParameterId(it.first);
            if (!eval.evaluate(pe->rpn(), res, ctx, s)) {
                return std::make_tuple(false, changed);
            }
            auto [ok, ch] = setParameter(it.first, res, s);
            changed = changed | ch;
            if (!ok) {
                s.extend(pe->location());
                return std::make_tuple(false, changed);
            }
        }
    }
    return std::make_tuple(true, changed);
}

void Parameterized::dump(std::ostream& os, const char* prefix) const {
    for(ParameterIndex i=0; i<parameterCount(); i++) {
        if (i>0) {
            os << "\n";
        }
        Value v;
        getParameter(i, v);
        os << prefix << std::string(parameterName(i)) << " = " << v;
    }
}

}
