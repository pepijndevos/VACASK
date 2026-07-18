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

    // Load .osdi models for resistor and capacitor (vsource is a builtin device)
    tab.add(PTLoad("resistor.osdi"))
       .add(PTLoad("capacitor.osdi"));

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
