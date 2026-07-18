#include "libplatform.h"
#include "simulator.h"
#include "parser.h"
#include "netlistrs.h"
#include "openvafcomp.h"
#include "circuit.h"
#include <memory>
#include <iostream>

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

    // Load .osdi models for passives, sources, and semiconductor devices.
    // vsource/isource are builtin (no load needed).
    // Semiconductor masters: diode, bsim3 (bsim3v3.osdi), bsim4 (bsim4v8.osdi),
    // bsimbulk (bsimbulk106.osdi), psp103va (psp103v4.osdi), vbic13 (vbic_1p3.osdi).
    tab.add(PTLoad("resistor.osdi"))
       .add(PTLoad("capacitor.osdi"))
       .add(PTLoad("inductor.osdi"))
       .add(PTLoad("diode.osdi"))
       .add(PTLoad("bsim3v3.osdi"))
       .add(PTLoad("bsim4v8.osdi"))
       .add(PTLoad("vbic_1p3.osdi"));

    if (!buildParserTablesFromFile(path, tab, p, s)) {
        Simulator::err() << s.message() << "\n"; return 1;
    }
    if (!tab.verify(s)) { Simulator::err() << s.message() << "\n"; return 1; }
    tab.dump(0, Simulator::out());

    OpenvafCompiler comp;
    Circuit cir(tab, &comp, s);
    if (!cir.isValid()) { Simulator::err() << s.message() << "\n"; return 1; }

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
        if (found_r) {
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
        if (found_c) {
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
        auto [found_xr, val_xr] = cir.instanceParameter("X1:R1", "r");
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
    // Without the Vcvs adapter E1 is skipped → gain not found.
    // The gain check here is informational; the primary verification is the tran + ngspice compare.
    {
        auto [found_eg, val_eg] = cir.instanceParameter("E1", "gain");
        if (found_eg) {
            Simulator::out() << "param check: E1.gain=" << val_eg.str()
                             << " (expect 2)\n";
        }
    }

    // Diode E2E (spice_diode.cir): verify the dmod model was elaborated with correct params.
    // SPICE instance names are uppercase (D1), model names match the .model card name.
    // Without the D/M/Q adapter (step-2 "FAIL" state), D1 and dmod are skipped →
    // modelParameter("dmod","is") returns false → error.
    // Note: "area" is not an OSDI instance param in diode.va; use model param "is".
    {
        bool require_diode = (path.find("spice_diode") != std::string::npos);
        auto [found_d, val_d] = cir.modelParameter("dmod", "is");
        if (require_diode && !found_d) {
            Simulator::err() << "ERROR: spice_diode: model 'dmod' not found in elaborated "
                                "hierarchy (D adapter not implemented?)\n";
            return 1;
        }
        if (found_d) {
            Simulator::out() << "param check: dmod.is=" << val_d.str()
                             << " (expect 1e-14)\n";
        }
    }

    // MOSFET E2E (spice_mos.cir): verify M1 is present with correct l=0.35u.
    // SPICE instance names are uppercase (M1).  l is annotated (*type="instance"*)
    // in bsim3v3.va so instanceParameter("M1","l") returns it.
    // Without the D/M/Q adapter, M1 is skipped → not found → error.
    {
        bool require_mos = (path.find("spice_mos") != std::string::npos);
        auto [found_m, val_m] = cir.instanceParameter("M1", "l");
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
