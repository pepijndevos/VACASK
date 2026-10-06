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

// vacask.compiler
void InitCompilerModule(py::module &m) {
    py::class_<sim::OpenvafCompiler, sim::SourceCompiler>(m, "OpenvafCompiler")
        .def(py::init([](
                std::optional<std::string> compiler,
                std::optional<std::vector<std::string>> compilerArgs) {

                return sim::OpenvafCompiler(
                    compiler,
                    compilerArgs);
            }),
            py::arg("compiler") = py::none(),
            py::arg("compiler_args") = py::none())
        .def("compile",
            [](sim::OpenvafCompiler& self,
                const std::string& loadDirectiveCanonicalPath,
                const std::string& fileName,
                const std::string& canonicalPath) {

                std::string outputCanonicalPath;
                sim::Status status;

                auto [success, cached] =
                    self.compile(loadDirectiveCanonicalPath,
                                    fileName,
                                    canonicalPath,
                                    outputCanonicalPath,
                                    status);

                return py::make_tuple(
                    success,
                    cached,
                    outputCanonicalPath);
            },
            py::arg("load_directive_canonical_path"),
            py::arg("file_name"),
            py::arg("canonical_path"));
}
        
      