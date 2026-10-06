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

// vacask.simulator
void InitSimulatorModule(py::module &m) {
    m.def(
        "setup",
        [](py::object obj) {
            if (obj.is_none()) {
                return sim::Simulator::setup();
            }
            return sim::Simulator::setup(1, 1, obj.cast<sim::Status&>());
        },
        py::arg("s") = py::none());
    m.def(
        "prependModulePath",
        [](std::vector<std::string> paths) {
            sim::Simulator::prependModulePath(std::move(paths));
        },
        py::arg("paths")
    );
    m.def(
        "appendModulePath",
        [](std::vector<std::string> paths) {
            sim::Simulator::appendModulePath(std::move(paths));
        },
        py::arg("paths")
    );
    m.def(
        "prependIncludePath",
        [](std::vector<std::string> paths) {
            sim::Simulator::prependIncludePath(std::move(paths));
        },
        py::arg("paths")
    );
    m.def(
        "appendIncludePath",
        [](std::vector<std::string> paths) {
            sim::Simulator::appendIncludePath(std::move(paths));
        },
        py::arg("paths")
    );
    m.def("modulePath", &sim::Simulator::modulePath);
    m.def("startupPath", &sim::Simulator::startupPath);
    m.def("includePath", &sim::Simulator::includePath);
}
        
      
