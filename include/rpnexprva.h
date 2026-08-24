#ifndef __RPNEXPRVA_DEFINED
#define __RPNEXPRVA_DEFINED

#include <vector>
#include "identifier.h"
#include "value.h"
#include "common.h"


namespace NAMESPACE {

// Behavioral source type: what expr represents and how its Verilog-A
// contribution statement is built. Defined here, rather than nested in
// PTBehavioral, because parseroutput.h transitively includes this header
// (PTBehavioral::Type is an alias for this enum).
enum class BehavioralType : char {
    CurrentSource,
    VoltageSource,
    Expression
};

struct RPNBehavioralVA {
    // Vectors with parameters, input potential nodes and input flow nodes
    // (parameter name, Verilog-A name, type, source RPN index)
    std::vector<std::tuple<Id, std::string, Value::Type, size_t>> param;
    // (node name, Verilog-A name, source RPN index)
    std::vector<std::tuple<Id, std::string, size_t>> node;
    // (instance name, Verilog-A name, source RPN index)
    std::vector<std::tuple<Id, std::string, size_t>> flow;
    // Verilog-A module name
    std::string moduleName;
    // Behavioral source type
    BehavioralType type;
    // User-supplied Verilog-A declarations and evaluation code, for Type::Expression
    std::string userDeclarations;
    std::string userEvaluation;
    // Verilog-A module code
    std::string vaCode;
};

}

#endif
