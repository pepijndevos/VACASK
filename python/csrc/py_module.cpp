#define PYBIND11_DETAILED_ERROR_MESSAGES
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>
#include <simulator.h>
#include <circuit.h>
#include <openvafcomp.h>
#include <parser.h>
#include <sourceloc.h>

namespace py = pybind11;

void InitSimulatorModule(py::module &m);
void InitLocModule(py::module &m);
void InitIdModule(py::module &m);
void InitParserOutputModule(py::module &m);
void InitCircuitModule(py::module &m);
void InitCompilerModule(py::module &m);
void InitAnalysisModule(py::module &m);
void InitOptionsModule(py::module &m);

PYBIND11_MODULE(_vacask, m, py::mod_gil_not_used()) {
    m.doc() = "VACASK Simulator.";
    m.def("startupPath", &sim::Simulator::startupPath, "Simulator start-up path.");
    
    // vacask.status
    {
        auto mod_status = m.def_submodule("status");
        py::class_<sim::Status>(mod_status, "Status")
            .def(py::init())
            .def("message", &sim::Status::message);
    }
    
    // vacask.elsetup
    {
        auto mod_elsetup = m.def_submodule("elsetup");
        py::class_<sim::DeviceRequests>(mod_elsetup, "DeviceRequests");
    }

    // vacask.simulator
    {
        auto mod_sim = m.def_submodule("simulator");
        InitSimulatorModule(mod_sim);
    }
        
    { // vacask.loc
        auto mod_loc = m.def_submodule("loc");
        InitLocModule(mod_loc);
    }

    { // vacask.id
        auto mod_id = m.def_submodule("id");
        InitIdModule(mod_id);
    }

    { // pyvacsk.value
        auto mod_value = m.def_submodule("value");
        py::class_<sim::Value>(mod_value, "Value")
            .def(py::init<>())
            .def(py::init<sim::Int>())
            .def(py::init<sim::Real>())
            .def(py::init<const sim::String &>())
            .def(py::init<const char *>())
            .def(py::init<const sim::IntVector &>())
            .def(py::init<const sim::RealVector &>())
            .def(py::init<const sim::StringVector &>())
            .def(py::init<const sim::ValueVector &>());
    }

    { // vacask.parser_output
        auto mod_parser_output = m.def_submodule("parser_output");
        InitParserOutputModule(mod_parser_output);
    }

    { // vacask.parser
        auto mod_parser = m.def_submodule("parser");
        py::class_<sim::Parser>(mod_parser, "Parser")
            .def(py::init<sim::ParserTables&>(),
                    py::arg("tables"),
                    py::keep_alive<1, 2>())
            .def("parseNetlistFile",
                    [](sim::Parser &self, sim::FileStackFileIndex fileIndex) {
                        return self.parseNetlistFile(fileIndex);
                    },
                    py::arg("fileIndex"))
            .def("parseNetlistString",
                    [](sim::Parser &self, const std::string &input) {
                        return self.parseNetlistString(input);
                    },
                    py::arg("input"))
            .def("parseExpression",
                    [](sim::Parser &self, const std::string &input) {
                        return self.parseExpression(input);
                    },
                    py::arg("input"))
            .def("parseExpression",
                    [](sim::Parser &self, const std::string &input, sim::Status& s) {
                        return self.parseExpression(input, s);
                    },
                    py::arg("input"), py::arg("s"))
            .def("parseParameters",
                    [](sim::Parser &self, const std::string &input) {
                        return self.parseParameters(input);
                    },
                    py::arg("input"));
    }
    
    { // vacask.circuit
        auto mod_circuit = m.def_submodule("circuit");
        InitCircuitModule(mod_circuit);
    }

    {  // vacask.compiler
        auto mod_compiler = m.def_submodule("compiler");
        InitCompilerModule(mod_compiler);
    }

    { // vacask.analysis
        auto mod_analysis = m.def_submodule("analysis");
        InitAnalysisModule(mod_analysis);
    }

    { // rpnexpr
        auto mod_rpnexpr = m.def_submodule("rpnexpr");
        py::class_<sim::Rpn>(mod_rpnexpr, "Rpn")
            .def(py::init<>());
    }

    { // options
        auto mod_options = m.def_submodule("options");
        InitOptionsModule(mod_options);
    }
}
