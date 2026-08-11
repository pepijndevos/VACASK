#include "rpnexpr.h"
#include "context.h"
#include "common.h"
#include <cctype>


namespace NAMESPACE {

std::string Rpn::sanitizeVariable(const std::string& s, bool atBeginning) {
    std::string result = s;
    // If this is at the beginning of the name, $ is not allowed as the first character
    bool first = atBeginning;
    for (char& c : result) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && !(c=='$' && !first) && !(c=='_')) {
            c = '_';
        }
        first = false;
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

// Expression stack entry: (Verilog-A text, is-bare-identifier flag, source RPN index)
typedef std::vector<std::tuple<std::string, bool, size_t>> SStack;

// Translates a VACASK builtin function call into its Verilog-A equivalent.
// stack holds the already-translated arguments; argPos is the stack index
// of the first argument, nArgs is the argument count. Returns the formatted
// Verilog-A function call text.
typedef std::string (*VAFuncTranslator)(SStack& stack, size_t argPos, size_t nArgs);

// Compile-time string usable as a non-type template argument (C++20)
template<std::size_t N>
struct FixedString {
    char value[N] = {};
    constexpr FixedString(const char (&str)[N]) {
        for (std::size_t i=0; i<N; i++) {
            value[i] = str[i];
        }
    }
};

// Trivial translation: VACASK name(arg1, ..., argn) -> DestName(arg1, ..., argn)
template<FixedString DestName>
static std::string trivialTranslator(SStack& stack, size_t argPos, size_t nArgs) {
    std::string txt = std::string(DestName.value) + "(";
    for (size_t i=0; i<nArgs; i++) {
        if (i>0) {
            txt += ", ";
        }
        auto& [argText, argIsId, argIdx] = stack.at(argPos+i);
        txt += argText;
    }
    txt += ")";
    return txt;
}

// Map key: function name plus arity, since a name may be trivially
// translatable at one arity (e.g. min/max at 2 args) but not another
// (e.g. min/max at 1 arg, which aggregates a vector)
typedef std::pair<Id, size_t> VAFuncKey;

struct VAFuncKeyHash {
    size_t operator()(const VAFuncKey& k) const {
        return hash_val(k.first.id(), k.second);
    }
};

static std::unordered_map<VAFuncKey, VAFuncTranslator, VAFuncKeyHash> vaFuncMap = {
    // Same name, same semantics
    { { Id::createStatic("sin"),   1 }, trivialTranslator<"sin"> },
    { { Id::createStatic("cos"),   1 }, trivialTranslator<"cos"> },
    { { Id::createStatic("tan"),   1 }, trivialTranslator<"tan"> },
    { { Id::createStatic("asin"),  1 }, trivialTranslator<"asin"> },
    { { Id::createStatic("acos"),  1 }, trivialTranslator<"acos"> },
    { { Id::createStatic("atan"),  1 }, trivialTranslator<"atan"> },
    { { Id::createStatic("sinh"),  1 }, trivialTranslator<"sinh"> },
    { { Id::createStatic("cosh"),  1 }, trivialTranslator<"cosh"> },
    { { Id::createStatic("tanh"),  1 }, trivialTranslator<"tanh"> },
    { { Id::createStatic("asinh"), 1 }, trivialTranslator<"asinh"> },
    { { Id::createStatic("acosh"), 1 }, trivialTranslator<"acosh"> },
    { { Id::createStatic("atanh"), 1 }, trivialTranslator<"atanh"> },
    { { Id::createStatic("ln"),    1 }, trivialTranslator<"ln"> },
    { { Id::createStatic("exp"),   1 }, trivialTranslator<"exp"> },
    { { Id::createStatic("sqrt"),  1 }, trivialTranslator<"sqrt"> },
    { { Id::createStatic("abs"),   1 }, trivialTranslator<"abs"> },
    { { Id::createStatic("floor"), 1 }, trivialTranslator<"floor"> },
    { { Id::createStatic("ceil"),  1 }, trivialTranslator<"ceil"> },
    { { Id::createStatic("pow"),   2 }, trivialTranslator<"pow"> },
    { { Id::createStatic("hypot"), 2 }, trivialTranslator<"hypot"> },
    { { Id::createStatic("atan2"), 2 }, trivialTranslator<"atan2"> },
    // 2-argument scalar case only; the 1-argument vector-aggregate case has
    // no Verilog-A equivalent and simply won't be found in this map
    { { Id::createStatic("min"),   2 }, trivialTranslator<"min"> },
    { { Id::createStatic("max"),   2 }, trivialTranslator<"max"> },
    // Same semantics, different Verilog-A name: VACASK's "log" is natural
    // log (like "ln"), while Verilog-A's "log" is base-10 (like VACASK's
    // "log10") -- a same-name passthrough here would silently be wrong
    { { Id::createStatic("log"),   1 }, trivialTranslator<"ln"> },
    { { Id::createStatic("log10"), 1 }, trivialTranslator<"log"> },
    // Type conversion, renamed to the matching Verilog-A system function
    { { Id::createStatic("int"),   1 }, trivialTranslator<"$rtoi"> },
    { { Id::createStatic("real"),  1 }, trivialTranslator<"$itor"> },
};

// VACASK operator tokens and precedence were verified to already match
// Verilog-AMS LRM Table 4-3 (unary tighter than **, everything else in the
// same relative order), so the same opMap used by str() applies here too.
bool Rpn::verilogA(const std::string& discipline, const std::string& potAccess, const std::string& flowAccess, RPNBehavioralVA& behavData, Status& s) const {
    // Second tuple element is true iff the entry is a bare (renamed) identifier
    // other than $temp/$scale, i.e. it never needs to be wrapped in parentheses
    // when used as an operand. Everything else is wrapped defensively.
    // Third tuple element is the index in expr of the RPN element that created
    // this stack entry.
    std::vector<std::tuple<std::string, bool, size_t>> sstack;
    // Verilog-A name format: __p<number>_<identifier>
    // Maps original identifier to index in behavData.param
    std::unordered_map<Id, size_t> paramMap;
    // Verilog-A name format: __v<number>_<identifier>
    // Maps original identifier to index in behavData.node
    std::unordered_map<Id, size_t> nodeMap;
    // Verilog-A name format: __i<number>_<identifier>
    // Maps original identifier to index in behavData.flow
    std::unordered_map<Id, size_t> flowMap;

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
                        auto& [nodeId, nodeVaName, nodeIdx] = behavData.node[nit->second];
                        nodeText[i] = nodeVaName;
                    } else {
                        auto vIdx = behavData.node.size();
                        nodeText[i] = std::string("__v")+std::to_string(vIdx)+"_"+sanitizeVariable(rawName);
                        nodeMap.emplace(nodeKey, behavData.node.size());
                        behavData.node.emplace_back(nodeKey, nodeText[i], p);
                    }
                }
                std::string txt = potAccess+"("+nodeText[0];
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
                auto& [flowId, flowVaName, flowIdx] = behavData.flow[fit->second];
                flowText = flowVaName;
            } else {
                auto iIdx = behavData.flow.size();
                flowText = std::string("__i")+std::to_string(iIdx)+"_"+sanitizeVariable(origId);
                flowMap.emplace(origId, behavData.flow.size());
                behavData.flow.emplace_back(origId, flowText, idx);
            }
            size_t fnIdx = idx+1;
            sstack.push_back({potAccess+"("+flowText+")", false, fnIdx});
            idx = fnIdx;
            continue;
        }
        switch (e.type()) {
            case Rpn::TValue: {
                const Value& v = e.get<Value>();
                if (v.type()!=Value::Type::Real && v.type()!=Value::Type::Int) {
                    s.set(Status::Unsupported, "Only real and integer literals have a Verilog-A equivalent.");
                    s.extend(location(e));
                    return false;
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
                    auto& [paramId, paramVaName, paramType, paramIdx] = behavData.param[pit->second];
                    vaName = paramVaName;
                } else {
                    vaName = std::string("__p")+std::to_string(idx)+"_"+sanitizeVariable(name);
                    paramMap.emplace(name, behavData.param.size());
                    behavData.param.emplace_back(name, vaName, Value::Type::Real, idx);
                }
                sstack.push_back({std::move(vaName), true, idx});
                continue;
            }
            case Rpn::TOp: {
                code = e.get<Op>().code;
                auto it = opMap.find(code);
                const char* opStr = "";
                if (it!=opMap.end()) {
                    auto& [opText, opPrec] = it->second;
                    opStr = opText;
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
                        return false;
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
                        auto [ ex2, ex2IsId, ex2Idx ] = std::move(sstack.back());
                        sstack.pop_back();
                        auto [ ex1, ex1IsId, ex1Idx ] = std::move(sstack.back());
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
                        auto [ ex3, ex3IsId, ex3Idx ] = std::move(sstack.back());
                        sstack.pop_back();
                        auto [ ex2, ex2IsId, ex2Idx ] = std::move(sstack.back());
                        sstack.pop_back();
                        auto [ ex1, ex1IsId, ex1Idx ] = std::move(sstack.back());
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
                        return false;
                }
                break;
            }
            case TFunctionCall: {
                // f()
                // f(x1)
                // f(x1, x2, ..., xn)
                // $intparam() and $realparam() take only identifier as argument
                // and indicate the corresponding Verilog-A parameter type.
                // v(a)/v(a,b) and i(a) are handled earlier via lookahead, before this switch
                // Everything else is looked up in vaFuncMap by (name, arity)
                auto name = e.get<FunctionCall>().name;
                auto n = e.get<FunctionCall>().arity;
                if (name==intParamId || name==realParamId) {
                    auto& [argText, argIsId, argIdx] = sstack.back();
                    if (n!=1 || !argIsId) {
                        s.set(Status::Unsupported, std::string(name)+"() requires a single identifier argument.");
                        s.extend(location(e));
                        return false;
                    }
                    Id argName = expr.at(argIdx).get<Identifier>().name;
                    Value::Type newType = (name==intParamId) ? Value::Type::Int : Value::Type::Real;
                    auto& [paramId, paramVaName, paramType, paramIdx] = behavData.param[paramMap.at(argName)];
                    paramType = newType;
                    // Result of $intparam()/$realparam() is the identifier itself
                    continue;
                }
                auto fit = vaFuncMap.find(VAFuncKey(name, n));
                if (fit==vaFuncMap.end()) {
                    s.set(
                        Status::Unsupported,
                        std::string("Function ")+std::string(name)+"() with arity "+
                        std::to_string(n)+" cannot be translated to Verilog-A."
                    );
                    s.extend(location(e));
                    return false;
                }
                auto argPos = sstack.size()-n;
                std::string txt = fit->second(sstack, argPos, n);
                sstack.resize(argPos);
                sstack.push_back({std::move(txt), false, idx});
                break;
            }
            case TPackVec:
            case TPackList:
                // No Verilog-A equivalent for vector/list literals
                s.set(Status::Unsupported, "Vector/list literals have no Verilog-A equivalent.");
                s.extend(location(e));
                return false;
            case TMakeBoolean:
            case TJump:
            case TBranch:
                // Do not format if hidden
                break;
            default:
                s.set(Status::Unsupported, "Expression construct has no Verilog-A equivalent.");
                s.extend(location(e));
                return false;
        }
    }
    auto& [resultExpr, resultIsId, resultIdx] = sstack.back();
    // Build extra terminals string
    std::string extraTerminals;
    for (auto& [nodeId, nodeVaName, nodeIdx] : behavData.node) {
        extraTerminals += ", " + nodeVaName;
    }
    for (auto& [flowId, flowVaName, flowIdx] : behavData.flow) {
        extraTerminals += ", " + flowVaName;
    }
    // Build parameters string
    std::string parametersString;
    for (auto& [paramId, paramVaName, paramType, paramIdx] : behavData.param) {
        parametersString += std::string("  (*desc=\"") + std::string(paramId) + "\", units=\"\"*) parameter " +
            (paramType==Value::Type::Int ? "integer" : "real") + " " + paramVaName + " = 0;\n";
    }
    behavData.vaCode =
        std::string("module "+behavData.moduleName+"(__nt1, __nt2" + extraTerminals + ");\n") +
        "  inout __nt1, __nt2" + extraTerminals + ";\n" +
        "  " + discipline + " __nt1, __nt2" + extraTerminals + ";\n" +
        "  branch (__nt1, __nt2) br;\n" +
        parametersString +
        "  analog begin\n" +
        "    " + (behavData.currentSource ? flowAccess : potAccess) + "(br) <+ " + 
             resultExpr + ";\n" +
        "  end\n" +
        "endmodule\n";
    return true;
}

}
