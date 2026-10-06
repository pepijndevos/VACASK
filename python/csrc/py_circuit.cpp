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

// vacask.circuit
void InitCircuitModule(py::module &m) {
    py::class_<sim::Circuit, std::unique_ptr<sim::Circuit>>(m, "Circuit")
        .def(py::init<sim::ParserTables&, sim::SourceCompiler*, sim::Status&>(),
            py::arg("tables"),
            py::arg("compiler") = nullptr,
            py::arg("status"),
            py::return_value_policy::reference_internal)
        .def("isValid", &sim::Circuit::isValid)
        .def("needsElaboration", &sim::Circuit::needsElaboration)
        .def("clear", &sim::Circuit::clear)
        .def("title",
            &sim::Circuit::title,
            py::return_value_policy::reference_internal)
        .def("setTitle", &sim::Circuit::setTitle)
        .def("deviceCount", &sim::Circuit::deviceCount)
        .def("nodeCount", &sim::Circuit::nodeCount)
        .def("unknownCount", &sim::Circuit::unknownCount)
        .def("instanceCount", &sim::Circuit::instanceCount)
        .def("subcircuitInstanceCount", &sim::Circuit::subcircuitInstanceCount)
        .def("getVariable",
            [](const sim::Circuit& c, sim::Id name) -> py::object {
                auto* v = c.getVariable(name);
                if (v)
                    return py::cast(*v);
                return py::none();
            })
        .def("getVariable",
            [](const sim::Circuit& c, std::string name) -> py::object {
                auto* v = c.getVariable(sim::Id(name));
                if (v)
                    return py::cast(*v);
                return py::none();
            })
        .def("setVariable",
            [](sim::Circuit& c, sim::Id name, const sim::Value& v) {
                return c.setVariable(name, v);
            })
        .def("setVariable",
            [](sim::Circuit& c, std::string name, const sim::Value& v) {
                return c.setVariable(sim::Id(name), v);
            })
        .def("setOption",
            [](sim::Circuit& c, sim::Id name, const sim::Value& v) {
                sim::Status s; // TODO
                return c.setOption(name, v, s);
            })
        .def("setOption",
            [](sim::Circuit& c, std::string name, const sim::Value& v) {
                sim::Status s; // TODO
                return c.setOption(sim::Id(name), v, s);
            })
        .def("setOptions",
            [](sim::Circuit& c, sim::IStruct<sim::SimulatorOptions>& opt) {
                return c.setOptions(opt);
            })
        .def("setOptions",
            [](sim::Circuit& c, const sim::PTParameters& params) {
                sim::Status s; // TODO
                return c.setOptions(params);
            }
        )
        .def("simulatorOptions",
            [](sim::Circuit& c) {
                const auto& options = c.simulatorOptions();
                return options.core();
            }
        )
        .def(
            "elaborate",
            [](sim::Circuit &self,
                const std::vector<sim::Id> &defs,
                const std::string &defName,
                const std::string &instName,
                sim::DeviceRequests *devReq,
                sim::Status &status)
            {
                return self.elaborate(defs, defName, instName, devReq, status);
            },
            py::arg("toplevel_definitions") = std::vector<sim::Id>{},
            py::arg("top_def_name") = "__topdef__",
            py::arg("top_inst_name") = "__topinst__",
            py::arg("dev_req") = nullptr,
            py::arg("status") 
        )
        .def("elaborateChanges",
            [](sim::Circuit &self, sim::Status& s) {
                return self.elaborateChanges(nullptr, s);
            },
            py::arg("status")
        )
        .def("dumpDevices",                                   
            [](const sim::Circuit& self, int indent) {
                self.dumpDevices(indent, std::cout);
            },
            py::arg("indent")) 
        .def("dumpModels",                                   
            [](const sim::Circuit& self, int indent) {
                self.dumpModels(indent, std::cout);
            },
            py::arg("indent")) 
        .def("dumpVariables",                                   
            [](const sim::Circuit& self, int indent) {
                self.dumpVariables(indent, std::cout);
            },
            py::arg("indent"))
        .def("dumpOptions",                               
            [](const sim::Circuit& self, int indent) {
                self.dumpOptions(indent, std::cout);
            },
            py::arg("indent"))
        .def("dumpHierarchy",
            [](const sim::Circuit& self, int indent) {
                self.dumpHierarchy(indent, std::cout);
            },
            py::arg("indent"))
        .def("dumpNodes",
            [](const sim::Circuit& self, int indent) {
                self.dumpNodes(indent, std::cout);
            },
            py::arg("indent"))
        .def("dumpUnknowns",
            [](const sim::Circuit& self, int indent) {
                self.dumpUnknowns(indent, std::cout);
            },
            py::arg("indent"))
        .def("dumpSparsity",
            [](const sim::Circuit& self, int indent) {
                self.dumpSparsity(indent, std::cout);
            },
            py::arg("indent"))
        .def("dumpDeviceCounts",
            [](const sim::Circuit& self, int indent) {
                self.dumpDeviceCounts(indent, std::cout);
            },
            py::arg("indent"))
        .def("instanceParameter",
            [](sim::Circuit& c, std::string name, std::string param) {
                sim::Status s;
                return c.instanceParameter(sim::Id(name), sim::Id(param), s);
            },
            py::arg("name"), py::arg("param")
        )
        .def("setInstanceParameter",
            [](sim::Circuit& c, std::string name, std::string param, const sim::Value& v) {
                sim::Status s;
                return c.setInstanceParameter(sim::Id(name), sim::Id(param), v, s);
            }
        );

    py::class_<sim::SourceCompiler>(m, "SourceCompiler");
}
        
      
