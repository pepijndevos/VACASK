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

// vacask.options
void InitOptionsModule(py::module &m) {
    py::class_<sim::SimulatorOptions>(m, "SimulatorOptions")
        .def(py::init<>())
    
        .def_readwrite("temp", &sim::SimulatorOptions::temp)
        .def_readwrite("tnom", &sim::SimulatorOptions::tnom)
        .def_readwrite("gmin", &sim::SimulatorOptions::gmin)
        .def_readwrite("gshunt", &sim::SimulatorOptions::gshunt)
        .def_readwrite("minr", &sim::SimulatorOptions::minr)
        .def_readwrite("scale", &sim::SimulatorOptions::scale)
        .def_readwrite("tolmode", &sim::SimulatorOptions::tolmode)
        .def_readwrite("tolscale", &sim::SimulatorOptions::tolscale)
        .def_readwrite("reltol", &sim::SimulatorOptions::reltol)
        .def_readwrite("abstol", &sim::SimulatorOptions::abstol)
        .def_readwrite("vntol", &sim::SimulatorOptions::vntol)
        .def_readwrite("chgtol", &sim::SimulatorOptions::chgtol)
        .def_readwrite("fluxtol", &sim::SimulatorOptions::fluxtol)
        .def_readwrite("relrefsol", &sim::SimulatorOptions::relrefsol)
        .def_readwrite("relrefres", &sim::SimulatorOptions::relrefres)
        .def_readwrite("relreflte", &sim::SimulatorOptions::relreflte)
        .def_readwrite("relref", &sim::SimulatorOptions::relref)
    
        .def_readwrite("matrixcheck", &sim::SimulatorOptions::matrixcheck)
        .def_readwrite("rhscheck", &sim::SimulatorOptions::rhscheck)
        .def_readwrite("solutioncheck", &sim::SimulatorOptions::solutioncheck)
        .def_readwrite("rcondcheck", &sim::SimulatorOptions::rcondcheck)
    
        .def_readwrite("sweep_pointmarker", &sim::SimulatorOptions::sweep_pointmarker)
        .def_readwrite("sweep_debug", &sim::SimulatorOptions::sweep_debug)
    
        .def_readwrite("nr_debug", &sim::SimulatorOptions::nr_debug)
        .def_readwrite("nr_bypass", &sim::SimulatorOptions::nr_bypass)
        .def_readwrite("nr_convtol", &sim::SimulatorOptions::nr_convtol)
        .def_readwrite("nr_bypasstol", &sim::SimulatorOptions::nr_bypasstol)
        .def_readwrite("nr_conviter", &sim::SimulatorOptions::nr_conviter)
        .def_readwrite("nr_residualcheck", &sim::SimulatorOptions::nr_residualcheck)
        .def_readwrite("nr_damping", &sim::SimulatorOptions::nr_damping)
        .def_readwrite("nr_force", &sim::SimulatorOptions::nr_force)
        .def_readwrite("nr_nsforce", &sim::SimulatorOptions::nr_nsforce)
        .def_readwrite("nr_contbypass", &sim::SimulatorOptions::nr_contbypass)
    
        .def_readwrite("homotopy_debug", &sim::SimulatorOptions::homotopy_debug)
        .def_readwrite("homotopy_gminsteps", &sim::SimulatorOptions::homotopy_gminsteps)
        .def_readwrite("homotopy_srcsteps", &sim::SimulatorOptions::homotopy_srcsteps)
        .def_readwrite("homotopy_gminfactor", &sim::SimulatorOptions::homotopy_gminfactor)
        .def_readwrite("homotopy_startgmin", &sim::SimulatorOptions::homotopy_startgmin)
        .def_readwrite("homotopy_maxgmin", &sim::SimulatorOptions::homotopy_maxgmin)
        .def_readwrite("homotopy_mingmin", &sim::SimulatorOptions::homotopy_mingmin)
        .def_readwrite("homotopy_maxgminfactor", &sim::SimulatorOptions::homotopy_maxgminfactor)
        .def_readwrite("homotopy_mingminfactor", &sim::SimulatorOptions::homotopy_mingminfactor)
        .def_readwrite("homotopy_srcstep", &sim::SimulatorOptions::homotopy_srcstep)
        .def_readwrite("homotopy_srcscale", &sim::SimulatorOptions::homotopy_srcscale)
        .def_readwrite("homotopy_minsrcstep", &sim::SimulatorOptions::homotopy_minsrcstep)
        .def_readwrite("homotopy_sourcefactor", &sim::SimulatorOptions::homotopy_sourcefactor)
    
        .def_readwrite("op_debug", &sim::SimulatorOptions::op_debug)
        .def_readwrite("op_itl", &sim::SimulatorOptions::op_itl)
        .def_readwrite("op_itlcont", &sim::SimulatorOptions::op_itlcont)
        .def_readwrite("op_skipinitial", &sim::SimulatorOptions::op_skipinitial)
        .def_readwrite("op_homotopy", &sim::SimulatorOptions::op_homotopy)
        .def_readwrite("op_srchomotopy", &sim::SimulatorOptions::op_srchomotopy)
        .def_readwrite("op_nsiter", &sim::SimulatorOptions::op_nsiter)
    
        .def_readwrite("smsig_debug", &sim::SimulatorOptions::smsig_debug)
    
        .def_readwrite("tran_debug", &sim::SimulatorOptions::tran_debug)
        .def_readwrite("tran_method", &sim::SimulatorOptions::tran_method)
        .def_readwrite("tran_maxord", &sim::SimulatorOptions::tran_maxord)
        .def_readwrite("tran_fs", &sim::SimulatorOptions::tran_fs)
        .def_readwrite("tran_ffmax", &sim::SimulatorOptions::tran_ffmax)
        .def_readwrite("tran_fbr", &sim::SimulatorOptions::tran_fbr)
        .def_readwrite("tran_rmax", &sim::SimulatorOptions::tran_rmax)
        .def_readwrite("tran_minpts", &sim::SimulatorOptions::tran_minpts)
        .def_readwrite("tran_itl", &sim::SimulatorOptions::tran_itl)
        .def_readwrite("tran_ft", &sim::SimulatorOptions::tran_ft)
        .def_readwrite("tran_predictor", &sim::SimulatorOptions::tran_predictor)
        .def_readwrite("tran_redofactor", &sim::SimulatorOptions::tran_redofactor)
        .def_readwrite("tran_lteratio", &sim::SimulatorOptions::tran_lteratio)
        .def_readwrite("tran_spicelte", &sim::SimulatorOptions::tran_spicelte)
        .def_readwrite("tran_xmu", &sim::SimulatorOptions::tran_xmu)
        .def_readwrite("tran_trapltefilter", &sim::SimulatorOptions::tran_trapltefilter)
        .def_readwrite("tran_noisedebug", &sim::SimulatorOptions::tran_noisedebug)
        .def_readwrite("tran_laggednoise", &sim::SimulatorOptions::tran_laggednoise)
        .def_readwrite("tran_extraoct", &sim::SimulatorOptions::tran_extraoct)
        .def_readwrite("tran_noiselte", &sim::SimulatorOptions::tran_noiselte)
    
        .def_readwrite("hb_debug", &sim::SimulatorOptions::hb_debug)
        .def_readwrite("hb_itl", &sim::SimulatorOptions::hb_itl)
        .def_readwrite("hb_itlcont", &sim::SimulatorOptions::hb_itlcont)
        .def_readwrite("hb_skipinitial", &sim::SimulatorOptions::hb_skipinitial)
        .def_readwrite("hb_homotopy", &sim::SimulatorOptions::hb_homotopy)
        .def_readwrite("hb_nsiter", &sim::SimulatorOptions::hb_nsiter)
    
        .def_readwrite("pss_minpts", &sim::SimulatorOptions::pss_minpts)
        .def_readwrite("pss_tolscale", &sim::SimulatorOptions::pss_tolscale)
        .def_readwrite("pss_itl", &sim::SimulatorOptions::pss_itl)
        .def_readwrite("pss_debug", &sim::SimulatorOptions::pss_debug)
    
        .def_readwrite("rawfile", &sim::SimulatorOptions::rawfile)
        .def_readwrite("strictoutput", &sim::SimulatorOptions::strictoutput)
        .def_readwrite("strictsave", &sim::SimulatorOptions::strictsave)
        .def_readwrite("strictforce", &sim::SimulatorOptions::strictforce)
        .def_readwrite("accounting", &sim::SimulatorOptions::accounting);
}
        
      
