#include "rpnexpr.h"
#include "common.h"


namespace NAMESPACE {

static Id  intParamId = Id::createStatic("$intparam");
static Id  realParamId = Id::createStatic("$realparam");
static Id  tempId = Id::createStatic("$temp");
static Id  scaleId = Id::createStatic("$scale");
static Id  abstimeId = Id::createStatic("$abstime");

// VACASK operator tokens and precedence were verified to already match
// Verilog-AMS LRM Table 4-3 (unary tighter than **, everything else in the
// same relative order), so the same opMap used by str() applies here too.
std::tuple<bool, std::string> Rpn::verilogA(Status& s) const {
    // Second tuple element is true iff the entry is a bare (renamed) identifier
    // other than $temp/$scale, i.e. it never needs to be wrapped in parentheses
    // when used as an operand. Everything else is wrapped defensively.
    std::vector<std::tuple<std::string, bool, Id>> sstack;
    // RPN identifier to Verilog-A parameter map
    // Format: __p<number>_<identifier>
    std::unordered_map<Id, std::tuple<std::string, Value::Type>> paramMap;
    std::vector<Id> paramOrder;
    // Node identifier to Verilog-A terminal (node) map
    // Format: __v<number>_<identifier>
    std::unordered_map<Id, std::string> nodeMap;
    std::vector<Id> nodeOrder;
    // Controlling current instance map
    // Format: __i<number>_<identifier>
    std::unordered_map<Id, std::string> flowMap;
    std::vector<Id> ctlInstanceOrder;
    
    OpCode code;
    for(auto e=expr.cbegin(); e!=expr.cend(); ++e) {
        switch (e->type()) {
            case Rpn::TValue: {
                const Value& v = e->get<Value>();
                if (v.type()!=Value::Type::Real && v.type()!=Value::Type::Int) {
                    s.set(Status::Unsupported, "Only real and integer literals have a Verilog-A equivalent.");
                    s.extend(location(*e));
                    return std::make_tuple(false, "");
                }
                sstack.push_back({v.str(), false, Id::none});
                continue;
            }
            case Rpn::TIdentifier: {
                // $temp translates to ($temperature-273.15)
                // $scale translates to $simparam("scale", 1)
                // $abstime translates to $abstime
                Id name = e->get<Identifier>().name;
                if (name==tempId) {
                    sstack.push_back({"($temperature-273.15)", false, Id::none});
                    continue;
                }
                if (name==scaleId) {
                    sstack.push_back({"$simparam(\"scale\", 1)", false, Id::none});
                    continue;
                }
                if (name==abstimeId) {
                    sstack.push_back({"$abstime", false, Id::none});
                    continue;
                }
                auto pit = paramMap.find(name);
                std::string vaName;
                if (pit!=paramMap.end()) {
                    vaName = std::get<0>(pit->second);
                } else {
                    vaName = std::string("__p")+std::to_string(paramMap.size())+"_"+std::string(name);
                    paramMap.emplace(name, std::make_tuple(vaName, Value::Type::Real));
                    paramOrder.push_back(name);
                }
                sstack.push_back({std::move(vaName), true, name});
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
                        auto [ ex, exIsId, exId ] = std::move(sstack.back());
                        sstack.pop_back();
                        sstack.push_back({
                            std::string(opStr) + (exIsId ? ex : parenthesize(ex)),
                            false, Id::none
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
                        Id ex1Id, ex2Id;
                        std::tie(ex2, ex2IsId, ex2Id) = std::move(sstack.back());
                        sstack.pop_back();
                        std::tie(ex1, ex1IsId, ex1Id) = std::move(sstack.back());
                        sstack.pop_back();
                        sstack.push_back({
                            (ex1IsId ? ex1 : parenthesize(ex1)) +
                            opStr +
                            (ex2IsId ? ex2 : parenthesize(ex2)),
                            false, Id::none
                        });
                        break;
                    }
                    case OpQuestion: {
                        // Ternary operator
                        std::string ex1, ex2, ex3;
                        bool ex1IsId, ex2IsId, ex3IsId;
                        Id ex1Id, ex2Id, ex3Id;
                        std::tie(ex3, ex3IsId, ex3Id) = std::move(sstack.back());
                        sstack.pop_back();
                        std::tie(ex2, ex2IsId, ex2Id) = std::move(sstack.back());
                        sstack.pop_back();
                        std::tie(ex1, ex1IsId, ex1Id) = std::move(sstack.back());
                        sstack.pop_back();
                        sstack.push_back({
                            (ex1IsId ? ex1 : parenthesize(ex1)) +
                            opStr +
                            (ex2IsId ? ex2 : parenthesize(ex2)) +
                            ":" +
                            (ex3IsId ? ex3 : parenthesize(ex3)),
                            false, Id::none
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
                auto name = e->get<FunctionCall>().name;
                auto n = e->get<FunctionCall>().arity;
                if (name==intParamId || name==realParamId) {
                    if (n!=1 || !std::get<1>(sstack.back())) {
                        s.set(Status::Unsupported, std::string(name)+"() requires a single identifier argument.");
                        s.extend(location(*e));
                        return std::make_tuple(false, "");
                    }
                    Value::Type newType = (name==intParamId) ? Value::Type::Int : Value::Type::Real;
                    std::get<1>(paramMap.at(std::get<2>(sstack.back()))) = newType;
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
                sstack.push_back({std::move(txt), false, Id::none});
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
