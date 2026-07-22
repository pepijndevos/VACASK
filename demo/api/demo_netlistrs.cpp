#include "libplatform.h"
#include "simulator.h"
#include "parser.h"
#include "netlistrs.h"
#include "openvafcomp.h"
#include "circuit.h"
#include <memory>
#include <iostream>
#include <cmath>
#include <stdexcept>

using namespace sim;

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: demo_netlistrs <file.scs|.cir|.sim>\n"; return 2; }
    std::string path = argv[1];

    // argv[2] overrides the compile-time default so CTest and manual runs can
    // point to any model directory without recompiling.
    std::string modDir = (argc >= 3) ? argv[2] : VACASK_MOD_DIR;

    Simulator::setup();
    Simulator::prependModulePath({modDir});

    ParserTables tab("rc from rust parser");
    Parser p(tab);
    Status s;

    // OSDI models for passives, sources, and semiconductor devices are now
    // auto-emitted by the adapter (buildParserTablesFromFile -> emitOsdiLoads)
    // for exactly the masters the parsed netlist references. vsource/isource are
    // builtin (no load needed).

    if (!buildParserTablesFromFile(path, tab, p, s)) {
        Simulator::err() << s.message() << "\n"; return 1;
    }
    if (!tab.verify(s)) { Simulator::err() << s.message() << "\n"; return 1; }
    tab.dump(0, Simulator::out());

    OpenvafCompiler comp;
    Circuit cir(tab, &comp, s);
    if (!cir.isValid()) { Simulator::err() << s.message() << "\n"; return 1; }

    // Sky130 PDK geometries are in microns; the models expect scale=1e-6
    // (matches the PDK's own VACASK smoke-test recipe). Must be set before
    // elaborate (scale affects device sizing/mapping).
    bool is_sky130 = (path.find("sky130") != std::string::npos);
    if (is_sky130) cir.setOption("scale", 1e-6);

    if (!cir.elaborate({}, "__topdef__", "__topinst__", nullptr, s)) {
        Simulator::err() << "elaboration failed: " << s.message() << "\n"; return 1;
    }
    cir.dumpHierarchy(0, Simulator::out());

    // Verify call-site parameter overrides when the nested subckt is present.
    // instanceParameter returns (false, _) when the instance does not exist, so
    // this is harmless for rc.scs / rc_inc.scs (no x1:r1 instance there).
    // For the nested netlist the instances MUST be found — if path contains
    // "rc_nested" we require both and fail hard if either is missing.
    {
        bool require_nested = (path.find("rc_nested") != std::string::npos);
        auto [found_r, val_r] = cir.instanceParameter("x1:r1", "r");
        if (require_nested && !found_r) {
            Simulator::err() << "ERROR: rc_nested: x1:r1 not found in hierarchy\n";
            return 1;
        }
        if (require_nested && found_r) {
            Simulator::out() << "param check: x1:r1 r=" << val_r.str() << "\n";
            if (val_r.str() != "2000") {
                Simulator::err() << "ERROR: x1:r1 r expected 2000 (rext override), got " << val_r.str() << "\n";
                return 1;
            }
        }
        auto [found_c, val_c] = cir.instanceParameter("x1:c1", "c");
        if (require_nested && !found_c) {
            Simulator::err() << "ERROR: rc_nested: x1:c1 not found in hierarchy\n";
            return 1;
        }
        if (require_nested && found_c) {
            Simulator::out() << "param check: x1:c1 c=" << val_c.str() << "\n";
            if (val_c.str() != "3e-06") {
                Simulator::err() << "ERROR: x1:c1 c expected 3e-06 (call-site override), got " << val_c.str() << "\n";
                return 1;
            }
        }
    }

    // SPICE subckt E2E (spice_subckt.cir): verify X1:R1 r=2000 (rval override from 1k to 2k).
    // X1 is a SubcktCall with master rcblock; R1 is inside rcblock with r=rval param.
    // Without the SubcktCall adapter X1 is skipped → X1:R1 not found → error.
    {
        bool require_spice_subckt = (path.find("spice_subckt") != std::string::npos);
        // SPICE names are canonicalized to lowercase by the netlistrs adapter.
        auto [found_xr, val_xr] = cir.instanceParameter("x1:r1", "r");
        if (require_spice_subckt && !found_xr) {
            Simulator::err() << "ERROR: spice_subckt: X1:R1 not found in hierarchy "
                                "(SubcktCall adapter not implemented?)\n";
            return 1;
        }
        if (found_xr) {
            Simulator::out() << "param check: X1:R1 r=" << val_xr.str()
                             << " (expect 2000 from rval=2k override)\n";
            if (val_xr.str() != "2000") {
                Simulator::err() << "ERROR: spice_subckt: X1:R1 r expected 2000 (rval=2k "
                                    "override), got " << val_xr.str() << "\n";
                return 1;
            }
        }
    }

    // VCVS E2E (spice_vcvs.cir): verify E1 gain=2.
    // Without the Vcvs adapter E1 is skipped → gain not found → error.
    {
        bool require_vcvs = (path.find("vcvs") != std::string::npos);
        auto [found_eg, val_eg] = cir.instanceParameter("e1", "gain");
        if (require_vcvs && !found_eg) {
            Simulator::err() << "ERROR: spice_vcvs: E1 not found in elaborated hierarchy "
                                "(Vcvs adapter not implemented?)\n";
            return 1;
        }
        if (found_eg) {
            Simulator::out() << "param check: E1.gain=" << val_eg.str()
                             << " (expect 2)\n";
            double gval_e = 0.0;
            try { gval_e = std::stod(val_eg.str()); } catch (...) {}
            if (std::abs(gval_e - 2.0) > 1e-9) {
                Simulator::err() << "ERROR: spice_vcvs: E1 gain expected 2, got "
                                 << val_eg.str() << "\n";
                return 1;
            }
        }
    }

    // Diode E2E (spice_diode.cir): verify the dmod model was elaborated with correct params.
    // SPICE instance names are uppercase (D1), model names match the .model card name.
    // Without the D/M/Q adapter (step-2 "FAIL" state), D1 and dmod are skipped →
    // modelParameter("dmod","is") returns false → error.
    // Note: "area" is not an OSDI instance param in diode.va; use model param "is".
    {
        // require_diode: spice_diode.cir direct test + spice_inc.cir include-resolution test
        bool require_diode = (path.find("spice_diode") != std::string::npos)
                           || (path.find("spice_inc") != std::string::npos);
        auto [found_d, val_d] = cir.modelParameter("dmod", "is");
        if (require_diode && !found_d) {
            Simulator::err() << "ERROR: model 'dmod' not found in elaborated hierarchy"
                             << (path.find("spice_inc") != std::string::npos
                                 ? " (SPICE .include not resolved?)"
                                 : " (D adapter not implemented?)")
                             << "\n";
            return 1;
        }
        if (found_d) {
            Simulator::out() << "param check: dmod.is=" << val_d.str()
                             << " (expect 1e-14)\n";
            if (require_diode && val_d.str() != "1e-14") {
                Simulator::err() << "ERROR: dmod.is expected 1e-14, got " << val_d.str() << "\n";
                return 1;
            }
        }
    }

    // MOSFET E2E (spice_mos.cir): verify M1 is present with correct l=0.35u.
    // SPICE instance names are uppercase (M1).  l is annotated (*type="instance"*)
    // in bsim3v3.va so instanceParameter("M1","l") returns it.
    // Without the D/M/Q adapter, M1 is skipped → not found → error.
    {
        bool require_mos = (path.find("spice_mos") != std::string::npos);
        auto [found_m, val_m] = cir.instanceParameter("m1", "l");
        if (require_mos && !found_m) {
            Simulator::err() << "ERROR: spice_mos: M1 not found in elaborated hierarchy "
                                "(M adapter not implemented?)\n";
            return 1;
        }
        if (found_m) {
            Simulator::out() << "param check: M1.l=" << val_m.str()
                             << " (expect 3.5e-07)\n";
            // l=0.35u = 3.5e-7
            if (val_m.str() != "3.5e-07") {
                Simulator::err() << "ERROR: M1.l expected 3.5e-07, got " << val_m.str()
                                 << " (check W/L param parsing)\n";
                return 1;
            }
        }
    }

    // CCCS E2E (spice_cccs.cir): verify F1 gain=5 (Fix 3).
    // Convention: ctlinst=Vsense (controlling vsource name), ctlnode="flow(br)" (default).
    // With Vin=1V, Rsense=1k, Vsense(0V sense): I_sense=1mA → F1 injects 5mA at node 2.
    // V(2) = -5V (F1 drains 5mA from node 2; Rload=1k to 0).
    {
        bool require_cccs = (path.find("spice_cccs") != std::string::npos);
        auto [found_fg, val_fg] = cir.instanceParameter("f1", "gain");
        if (require_cccs && !found_fg) {
            Simulator::err() << "ERROR: spice_cccs: F1 not found in elaborated hierarchy "
                                "(Cccs adapter not working?)\n";
            return 1;
        }
        if (found_fg) {
            Simulator::out() << "param check: F1.gain=" << val_fg.str()
                             << " (expect 5)\n";
            double gval_f = 0.0;
            try { gval_f = std::stod(val_fg.str()); } catch (...) {}
            if (std::abs(gval_f - 5.0) > 1e-9) {
                Simulator::err() << "ERROR: F1.gain expected 5, got " << val_fg.str() << "\n";
                return 1;
            }
        }
    }

    // Resistor model-reference E2E (spice_res_model.cir): the R instance names
    // a `.model reshead R` card AND carries r={rhead}. Verify x1:rend1 resolved
    // with r=100 (from the subckt .param) — proving the positional token was
    // read as the model name, not emitted as a colliding r= value.
    {
        bool require_res_model = (path.find("spice_res_model") != std::string::npos);
        auto [found_rr, val_rr] = cir.instanceParameter("x1:rend1", "r");
        if (require_res_model && !found_rr) {
            Simulator::err() << "ERROR: spice_res_model: x1:rend1 not found "
                                "(resistor model-reference not handled?)\n";
            return 1;
        }
        if (found_rr) {
            Simulator::out() << "param check: x1:rend1 r=" << val_rr.str()
                             << " (expect 100)\n";
            if (val_rr.str() != "100") {
                Simulator::err() << "ERROR: x1:rend1 r expected 100, got "
                                 << val_rr.str() << "\n";
                return 1;
            }
        }
    }

    // Sky130 devices: run a DC operating point (the PDK recipe uses op; a bare
    // transient without an initial op tends to be stiff for these models).
    if (is_sky130) {
        auto opDesc = PTAnalysis("op1", "op");
        std::unique_ptr<Analysis> op(Analysis::create(opDesc, cir, s));
        if (!op) { Simulator::err() << "analysis create failed: " << s.message() << "\n"; return 1; }
        op->add(PTSave("default"));
        auto [ok, canResume] = op->run(s);
        if (!ok) { Simulator::err() << "analysis failed: " << s.message() << "\n"; return 1; }
        Simulator::out() << "Analysis OK (sky130 op).\n";
        return 0;
    }

    auto tranDesc = PTAnalysis("tran1", "tran");
    tranDesc.add(PV{"step", 1e-5}).add(PV{"stop", 10e-3});
    std::unique_ptr<Analysis> tran(Analysis::create(tranDesc, cir, s));
    if (!tran) { Simulator::err() << "analysis create failed: " << s.message() << "\n"; return 1; }
    tran->add(PTSave("default"));

    auto [ok, canResume] = tran->run(s);
    if (!ok) { Simulator::err() << "analysis failed: " << s.message() << "\n"; return 1; }
    Simulator::out() << "Analysis OK.\n";
    return 0;
}
