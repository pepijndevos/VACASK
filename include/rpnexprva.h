#ifndef __RPNEXPRVA_DEFINED
#define __RPNEXPRVA_DEFINED

#include <vector>
#include "identifier.h"
#include "value.h"
#include "common.h"


namespace NAMESPACE {

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
    // Current source flag
    bool currentSource;
    // Verilog-A module code
    std::string vaCode;
};

}

#endif
