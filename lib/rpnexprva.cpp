#include "rpnexpr.h"
#include "context.h"
#include "common.h"
#include <cctype>


namespace NAMESPACE {

static std::string sanitizeVariable(const std::string& s) {
    std::string result = s;
    for (char& c : result) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c!='_' && c!='$') {
            c = '_';
        }
    }
    return result;
}

// An entry that may be a v() node argument: a bare identifier
// or an integer literal (e.g. 0 for ground)
static bool isNodeArg(const Rpn::Entry& ent) {
    return ent.type()==Rpn::TIdentifier ||
           (ent.type()==Rpn::TValue && ent.get<Value>().type()==Value::Type::Int);
}

static Id  intParamId = Id::createStatic("$intparam");
static Id  realParamId = Id::createStatic("$realparam");
static Id  tempId = Id::createStatic("$temp");
static Id  scaleId = Id::createStatic("$scale");
static Id  abstimeId = Id::createStatic("$abstime");
static Id  vId = Id::createStatic("v");
static Id  iId = Id::createStatic("i");

// VACASK operator tokens and precedence were verified to already match
// Verilog-AMS LRM Table 4-3 (unary tighter than **, everything else in the
// same relative order), so the same opMap used by str() applies here too.
std::tuple<bool, std::string> Rpn::verilogA(Status& s) const {
    // Second tuple element is true iff the entry is a bare (renamed) identifier
    // other than $temp/$scale, i.e. it never needs to be wrapped in parentheses
    // when used as an operand. Everything else is wrapped defensively.
    // Third tuple element is the index in expr of the RPN element that created
    // this stack entry.
    std::vector<std::tuple<std::string, bool, size_t>> sstack;
    // RPN identifier to Verilog-A parameter map
    // Format: __p<number>_<identifier>
    // Maps original identifier to tuple (Verilog-A name, type, consecutive number)
    std::unordered_map<Id, std::tuple<std::string, Value::Type, size_t>> paramMap;
    // Node identifier to Verilog-A terminal (node) map
    // Format: __v<number>_<identifier>
    // Maps original identifier to tuple (Verilog-A name, consecutive number)
    std::unordered_map<Id, std::tuple<std::string, size_t>> nodeMap;
    // Controlling current instance map
    // Format: __i<number>_<identifier>
    // Maps original identifier to tuple (Verilog-A name, consecutive number)
    std::unordered_map<Id, std::tuple<std::string, size_t>> flowMap;

    OpCode code;
    for(size_t idx=0; idx<expr.size(); idx++) {
        auto& e = expr[idx];
        // Handle v(a), v(a,b) by checking for pattern
        // identifier/integer, function v
        // identifier/integer, identifier/integer, function v
        // Handle it and then skip beyond function v
        if (isNodeArg(e)) {
            size_t nArgs = 0;
            if (idx+1<expr.size() && expr[idx+1].type()==TFunctionCall &&
                expr[idx+1].get<FunctionCall>().name==vId &&
                expr[idx+1].get<FunctionCall>().arity==1) {
                nArgs = 1;
            } else if (idx+2<expr.size() && isNodeArg(expr[idx+1]) &&
                       expr[idx+2].type()==TFunctionCall &&
                       expr[idx+2].get<FunctionCall>().name==vId &&
                       expr[idx+2].get<FunctionCall>().arity==2) {
                nArgs = 2;
            }
            if (nArgs>0) {
                std::string nodeText[2];
                for (size_t i=0; i<nArgs; i++) {
                    size_t p = idx+i;
                    const Entry& argEntry = expr.at(p);
                    std::string rawName;
                    Id nodeKey;
                    if (argEntry.type()==TIdentifier) {
                        Id origId = argEntry.get<Identifier>().name;
                        rawName = std::string(origId);
                        nodeKey = origId;
                    } else {
                        // Node given as an integer (e.g. 0 for ground)
                        rawName = argEntry.get<Value>().str();
                        nodeKey = Id(rawName);
                    }
                    auto nit = nodeMap.find(nodeKey);
                    if (nit!=nodeMap.end()) {
                        nodeText[i] = std::get<0>(nit->second);
                    } else {
                        nodeText[i] = std::string("__v")+std::to_string(p)+"_"+sanitizeVariable(rawName);
                        nodeMap.emplace(nodeKey, std::make_tuple(nodeText[i], p));
                    }
                }
                std::string txt = "V("+nodeText[0];
                if (nArgs==2) {
                    txt += ","+nodeText[1];
                }
                txt += ")";
                size_t fnIdx = idx+nArgs;
                sstack.push_back({std::move(txt), false, fnIdx});
                idx = fnIdx;
                continue;
            }
        }
        // Handle i(a) where a is an identifier
        // Use lookahead, just like with v(), handle and skip on match
        if (e.type()==TIdentifier &&
            idx+1<expr.size() && expr[idx+1].type()==TFunctionCall &&
            expr[idx+1].get<FunctionCall>().name==iId &&
            expr[idx+1].get<FunctionCall>().arity==1) {
            Id origId = e.get<Identifier>().name;
            auto fit = flowMap.find(origId);
            std::string flowText;
            if (fit!=flowMap.end()) {
                flowText = std::get<0>(fit->second);
            } else {
                flowText = std::string("__i")+std::to_string(idx)+"_"+sanitizeVariable(origId);
                flowMap.emplace(origId, std::make_tuple(flowText, idx));
            }
            size_t fnIdx = idx+1;
            sstack.push_back({"V("+flowText+")", false, fnIdx});
            idx = fnIdx;
            continue;
        }
        switch (e.type()) {
            case Rpn::TValue: {
                const Value& v = e.get<Value>();
                if (v.type()!=Value::Type::Real && v.type()!=Value::Type::Int) {
                    s.set(Status::Unsupported, "Only real and integer literals have a Verilog-A equivalent.");
                    s.extend(location(e));
                    return std::make_tuple(false, "");
                }
                sstack.push_back({v.str(), false, idx});
                continue;
            }
            case Rpn::TIdentifier: {
                // $temp translates to ($temperature-273.15)
                // $scale translates to $simparam("scale", 1)
                // $abstime translates to $abstime
                Id name = e.get<Identifier>().name;
                if (name==tempId) {
                    sstack.push_back({"($temperature-273.15)", false, idx});
                    continue;
                }
                if (name==scaleId) {
                    sstack.push_back({"$simparam(\"scale\", 1)", false, idx});
                    continue;
                }
                if (name==abstimeId) {
                    sstack.push_back({"$abstime", false, idx});
                    continue;
                }
                // Builtin VACASK constants (e.g. M_PI, P_Q) 
                // are replaced with numeric literals because Verilog-A constants 
                // may have a slightly different value, depending on the included 
                // files. 
                const Value* constVal = ContextStack::getConstant(name);
                if (constVal) {
                    sstack.push_back({constVal->str(), false, idx});
                    continue;
                }
                // All other identifiers
                auto pit = paramMap.find(name);
                std::string vaName;
                if (pit!=paramMap.end()) {
                    vaName = std::get<0>(pit->second);
                } else {
                    vaName = std::string("__p")+std::to_string(idx)+"_"+sanitizeVariable(name);
                    paramMap.emplace(name, std::make_tuple(vaName, Value::Type::Real, idx));
                }
                sstack.push_back({std::move(vaName), true, idx});
                continue;
            }
            case Rpn::TOp: {
                code = e.get<Op>().code;
                auto it = opMap.find(code);
                const char* opStr = "";
                if (it!=opMap.end()) {
                    opStr = std::get<0>(it->second);
                }
                switch (code) {
                    case OpBitNot:
                    case OpBitShiftL:
                    case OpBitShiftR:
                    case OpBitAnd:
                    case OpBitOr:
                    case OpBitExor:
                        // No Verilog-A equivalent for bitwise/shift operators
                        s.set(Status::Unsupported, "Bitwise and shift operators have no Verilog-A equivalent.");
                        s.extend(location(e));
                        return std::make_tuple(false, "");
                    case OpUMinus:
                    case OpNot: {
                        // Unary prefix operators
                        auto [ ex, exIsId, exIdx ] = std::move(sstack.back());
                        sstack.pop_back();
                        sstack.push_back({
                            std::string(opStr) + (exIsId ? ex : parenthesize(ex)),
                            false, idx
                        });
                        break;
                    }
                    case OpPlus:
                    case OpMinus:
                    case OpTimes:
                    case OpDivide:
                    case OpPower:
                    case OpLess:
                    case OpLessEq:
                    case OpGreater:
                    case OpGreaterEq:
                    case OpEqual:
                    case OpNotEqual:
                    case OpAnd:
                    case OpOr: {
                        // Binary infix operators
                        std::string ex1, ex2;
                        bool ex1IsId, ex2IsId;
                        size_t ex1Idx, ex2Idx;
                        std::tie(ex2, ex2IsId, ex2Idx) = std::move(sstack.back());
                        sstack.pop_back();
                        std::tie(ex1, ex1IsId, ex1Idx) = std::move(sstack.back());
                        sstack.pop_back();
                        sstack.push_back({
                            (ex1IsId ? ex1 : parenthesize(ex1)) +
                            opStr +
                            (ex2IsId ? ex2 : parenthesize(ex2)),
                            false, idx
                        });
                        break;
                    }
                    case OpQuestion: {
                        // Ternary operator
                        std::string ex1, ex2, ex3;
                        bool ex1IsId, ex2IsId, ex3IsId;
                        size_t ex1Idx, ex2Idx, ex3Idx;
                        std::tie(ex3, ex3IsId, ex3Idx) = std::move(sstack.back());
                        sstack.pop_back();
                        std::tie(ex2, ex2IsId, ex2Idx) = std::move(sstack.back());
                        sstack.pop_back();
                        std::tie(ex1, ex1IsId, ex1Idx) = std::move(sstack.back());
                        sstack.pop_back();
                        sstack.push_back({
                            (ex1IsId ? ex1 : parenthesize(ex1)) +
                            opStr +
                            (ex2IsId ? ex2 : parenthesize(ex2)) +
                            ":" +
                            (ex3IsId ? ex3 : parenthesize(ex3)),
                            false, idx
                        });
                        break;
                    }
                    case OpSelect:
                        // No Verilog-A equivalent for vector/list indexing
                        s.set(Status::Unsupported, "Selection operator has no Verilog-A equivalent.");
                        s.extend(location(e));
                        return std::make_tuple(false, "");
                }
                break;
            }
            case TFunctionCall: {
                // f()
                // f(x1)
                // f(x1, x2, ..., xn)
                // int() and real() perform conversion
                // $intparam() and $realparam() take only identifier as argument
                // and indicate the corresponding Verilog-A parameter type.
                // v(a)/v(a,b) and i(a) are handled earlier via lookahead, before this switch
                auto name = e.get<FunctionCall>().name;
                auto n = e.get<FunctionCall>().arity;
                if (name==intParamId || name==realParamId) {
                    if (n!=1 || !std::get<1>(sstack.back())) {
                        s.set(Status::Unsupported, std::string(name)+"() requires a single identifier argument.");
                        s.extend(location(e));
                        return std::make_tuple(false, "");
                    }
                    Id argName = expr.at(std::get<2>(sstack.back())).get<Identifier>().name;
                    Value::Type newType = (name==intParamId) ? Value::Type::Int : Value::Type::Real;
                    std::get<1>(paramMap.at(argName)) = newType;
                    // Result of $intparam()/$realparam() is the identifier itself
                    continue;
                }
                std::string txt = std::string(name)+"(";
                auto j = sstack.size()-n;
                for(decltype(n) i=0; i<n; i++, j++) {
                    if (i>0 && n>1)  {
                        txt += ", ";
                    }
                    txt += std::get<0>(sstack.at(j));
                }
                txt+=")";
                sstack.resize(sstack.size()-n);
                sstack.push_back({std::move(txt), false, idx});
                break;
            }
            case TPackVec:
            case TPackList:
                // No Verilog-A equivalent for vector/list literals
                s.set(Status::Unsupported, "Vector/list literals have no Verilog-A equivalent.");
                s.extend(location(e));
                return std::make_tuple(false, "");
            case TMakeBoolean:
            case TJump:
            case TBranch:
                // Do not format if hidden
                break;
            default:
                s.set(Status::Unsupported, "Expression construct has no Verilog-A equivalent.");
                s.extend(location(e));
                return std::make_tuple(false, "");
        }
    }
    return std::make_tuple(true, std::get<0>(sstack.back()));
}

}
