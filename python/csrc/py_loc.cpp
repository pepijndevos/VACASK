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

// vacask.loc
void InitLocModule(py::module &m) {
    py::class_<sim::Loc>(m, "Loc")
        .def(py::init<>())
        .def(py::init<
                    sim::FileStackIndex,
                    sim::FileStackFileIndex,
                    sim::SourceLineNumber,
                    sim::SourceColumnNumber>(),
                py::arg("file_stack"),
                py::arg("file"),
                py::arg("line"),
                py::arg("column"))
        .def("data", &sim::Loc::data)
        .def("toString", &sim::Loc::toString)
        .def("__str__", &sim::Loc::toString)
        .def("__repr__",
                [](const sim::Loc &self) {
                    return "Loc('" + self.toString() + "')";
                })
        .def("__bool__",
                [](const sim::Loc &self) {
                    return static_cast<bool>(self);
                })
        .def("__eq__",
                [](const sim::Loc &a, const sim::Loc &b) {
                    return a == b;
                })
        .def("__ne__",
                [](const sim::Loc &a, const sim::Loc &b) {
                    return a != b;
                })
        .def_readonly_static("bad", &sim::Loc::bad);
}
        
      