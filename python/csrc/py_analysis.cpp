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

// vacask.analysis
void InitAnalysisModule(py::module &m) {
    py::class_<sim::Analysis>(m, "Analysis")
        .def_property_readonly("name", &sim::Analysis::name)
        .def("sweep_count", &sim::Analysis::sweepCount)
        .def("update_sweeper", &sim::Analysis::updateSweeper)
        .def(
            "add",
            py::overload_cast<const sim::PTSave &>(&sim::Analysis::add),
            py::return_value_policy::reference_internal
        )
        .def(
            "add",
            py::overload_cast<const sim::PTSaves &>(&sim::Analysis::add),
            py::return_value_policy::reference_internal
        )
        .def(
            "add",
            py::overload_cast<const sim::PTParameters &>(&sim::Analysis::add),
            py::return_value_policy::reference_internal
        )
        .def(
            "add",
            [](sim::Analysis &self, sim::PTParameters &p) -> sim::Analysis& {
                return self.add(std::move(p));
            },
            py::return_value_policy::reference_internal
        )
        .def(
            "add",
            py::overload_cast<const sim::PTParameterMap &>(&sim::Analysis::add),
            py::return_value_policy::reference_internal
        )
        .def_static(
            "create",
            &sim::Analysis::create,
            py::arg("pt_analysis"),
            py::arg("circuit"),
            py::arg("status"),
            py::return_value_policy::take_ownership,
            // The analysis stores references to its description and to the
            // circuit: keep both alive while the analysis lives.
            py::keep_alive<0, 1>(),
            py::keep_alive<0, 2>()
        )
        .def("start", &sim::Analysis::start)
        .def("is_running", &sim::Analysis::isRunning)
        .def("resume", &sim::Analysis::resume)
        .def("finish", &sim::Analysis::finish)
        .def("run", &sim::Analysis::run)
        .def("update_parameter_expressions", &sim::Analysis::updateParameterExpressions)
        //.def("requests_rebuild", &sim::Analysis::requestsRebuild)
        .def("pre_mapping", &sim::Analysis::preMapping)
        .def("populate_structures", &sim::Analysis::populateStructures);
}
        
      