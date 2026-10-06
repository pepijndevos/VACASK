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
void InitIdModule(py::module &m) {
    py::class_<sim::Id>(m, "Id")
    // Constructors
    .def(py::init<>())
    .def(py::init<sim::IdentifierIndex>(),
            py::arg("id"))
    .def(py::init<const std::string &>(),
            py::arg("name"))
    .def_property_readonly("id", &sim::Id::id)
    .def("c_str", &sim::Id::c_str)
    .def("__str__",
            [](const sim::Id &self) {
                return std::string(self);
            })
    .def("__repr__",
            [](const sim::Id &self) {
                return "<Id '" + std::string(self) + "'>";
            })
    .def("__bool__",
            [](const sim::Id &self) {
                return static_cast<bool>(self);
            })
    .def(py::self == py::self)
    .def(py::self != py::self)
    .def_static("createStatic",
                &sim::Id::createStatic,
                py::arg("name"))
    .def_readonly_static("none", &sim::Id::none);
}
        
      