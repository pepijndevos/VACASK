#include "rpnexpr.h"
#include "common.h"


namespace NAMESPACE {

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
    size_t idx = 0;
    for(auto e=expr.cbegin(); e!=expr.cend(); ++e, ++idx) {
        switch (e->type()) {
            case Rpn::TValue: {
                const Value& v = e->get<Value>();
                if (v.type()!=Value::Type::Real && v.type()!=Value::Type::Int) {
                    s.set(Status::Unsupported, "Only real and integer literals have a Verilog-A equivalent.");
                    s.extend(location(*e));
                    return std::make_tuple(false, "");
                }
                sstack.push_back({v.str(), false, idx});
                continue;
            }
            case Rpn::TIdentifier: {
                // $temp translates to ($temperature-273.15)
                // $scale translates to $simparam("scale", 1)
                // $abstime translates to $abstime
                Id name = e->get<Identifier>().name;
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
                auto pit = paramMap.find(name);
                std::string vaName;
                if (pit!=paramMap.end()) {
                    vaName = std::get<0>(pit->second);
                } else {
                    size_t num = paramMap.size();
                    vaName = std::string("__p")+std::to_string(num)+"_"+std::string(name);
                    paramMap.emplace(name, std::make_tuple(vaName, Value::Type::Real, num));
                }
                sstack.push_back({std::move(vaName), true, idx});
                continue;
            }
            case Rpn::TOp: {
                code = e->get<Op>().code;
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
                        s.extend(location(*e));
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
                        s.extend(location(*e));
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
                // v(a) and v(a,b) take a node name (identifier or integer) argument
                // and translate to V(A) / V(A,B), a and b are added to the node map
                // i(a) takes a single identifier argument (controlling instance name)
                // and translates to V(A), a is added to the flow map
                auto name = e->get<FunctionCall>().name;
                auto n = e->get<FunctionCall>().arity;
                if (name==intParamId || name==realParamId) {
                    if (n!=1 || !std::get<1>(sstack.back())) {
                        s.set(Status::Unsupported, std::string(name)+"() requires a single identifier argument.");
                        s.extend(location(*e));
                        return std::make_tuple(false, "");
                    }
                    Id argName = expr.at(std::get<2>(sstack.back())).get<Identifier>().name;
                    Value::Type newType = (name==intParamId) ? Value::Type::Int : Value::Type::Real;
                    std::get<1>(paramMap.at(argName)) = newType;
                    // Result of $intparam()/$realparam() is the identifier itself
                    continue;
                }
                if (name==vId) {
                    if (n!=1 && n!=2) {
                        s.set(Status::Unsupported, "v() requires 1 or 2 arguments.");
                        s.extend(location(*e));
                        return std::make_tuple(false, "");
                    }
                    std::string nodeText[2];
                    auto j0 = sstack.size()-n;
                    for (decltype(n) i=0; i<n; i++) {
                        auto& [argStr, argIsId, argIdx] = sstack.at(j0+i);
                        std::string rawName;
                        Id nodeKey;
                        if (argIsId) {
                            // Node given as an identifier: no longer a parameter
                            Id origId = expr.at(argIdx).get<Identifier>().name;
                            rawName = std::string(origId);
                            nodeKey = origId;
                            paramMap.erase(origId);
                        } else {
                            // Node given as an integer (e.g. 0 for ground)
                            const Entry& argEntry = expr.at(argIdx);
                            if (argEntry.type()!=TValue || argEntry.get<Value>().type()!=Value::Type::Int) {
                                s.set(Status::Unsupported, "v() arguments must be an identifier or an integer node name.");
                                s.extend(location(*e));
                                return std::make_tuple(false, "");
                            }
                            rawName = argStr;
                            nodeKey = Id(rawName);
                        }
                        auto nit = nodeMap.find(nodeKey);
                        if (nit!=nodeMap.end()) {
                            nodeText[i] = std::get<0>(nit->second);
                        } else {
                            size_t num = nodeMap.size();
                            nodeText[i] = std::string("__v")+std::to_string(num)+"_"+rawName;
                            nodeMap.emplace(nodeKey, std::make_tuple(nodeText[i], num));
                        }
                    }
                    sstack.resize(j0);
                    std::string txt = "V("+nodeText[0];
                    if (n==2) {
                        txt += ","+nodeText[1];
                    }
                    txt += ")";
                    sstack.push_back({std::move(txt), false, idx});
                    continue;
                }
                if (name==iId) {
                    if (n!=1 || !std::get<1>(sstack.back())) {
                        s.set(Status::Unsupported, "i() requires a single identifier argument.");
                        s.extend(location(*e));
                        return std::make_tuple(false, "");
                    }
                    // Controlling instance name: no longer a parameter
                    Id origId = expr.at(std::get<2>(sstack.back())).get<Identifier>().name;
                    paramMap.erase(origId);
                    auto fit = flowMap.find(origId);
                    std::string flowText;
                    if (fit!=flowMap.end()) {
                        flowText = std::get<0>(fit->second);
                    } else {
                        size_t num = flowMap.size();
                        flowText = std::string("__i")+std::to_string(num)+"_"+std::string(origId);
                        flowMap.emplace(origId, std::make_tuple(flowText, num));
                    }
                    sstack.pop_back();
                    sstack.push_back({"V("+flowText+")", false, idx});
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
                s.extend(location(*e));
                return std::make_tuple(false, "");
            case TMakeBoolean:
            case TJump:
            case TBranch:
                // Do not format if hidden
                break;
            default:
                s.set(Status::Unsupported, "Expression construct has no Verilog-A equivalent.");
                s.extend(location(*e));
                return std::make_tuple(false, "");
        }
    }
    return std::make_tuple(true, std::get<0>(sstack.back()));
}

}
