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
