#include "libplatform.h"
#include "simulator.h"
#include "parser.h"
#include "openvafcomp.h"
#include "circuit.h"
#include "processutils.h"

using namespace sim;

// Self-heating lightbulb built from behavioral sources (transient only).
// C++ API rewrite of demo/behavioral/lightbulb.sim's tran1 analysis.
//
// Filament resistance:   r(t)  = r0*(p0 + p1*Temp(t) + p2*Temp(t)^2)
// Electrical branch:     i(a,0) = V(a,0)/r(t)
// Thermal branch:        -p + (Temp(t)-Ta)/Rth + ddt(Cth*Temp(t)) = 0,  p = i(a,0)*V(a,0)
int main() {
    // Path to staged models (osdi files)
    std::string modulePath = "../../lib/vacask/mod";
    // Path to staged Python libraries
    std::string pythonLibraryPath = "../../lib/vacask/python";
    // Python binary name
    std::string pythonBinary = findPythonExecutable();

    // Status variable
    Status s;

    // Simulator setup, no paths set
    Simulator::setup();

    // Prepend directories to the list of osdi file paths
    Simulator::prependModulePath({modulePath});

    // Parser tables
    ParserTables tab("Self-heating lightbulb (behavioral sources)");

    // Parser, needs tables to store stats when parsing expressions and parameters
    Parser p(tab);

    // Build circuit description. No OSDI loads are needed: "vsrc" uses the
    // builtin vsource device, "bi"/"ct"/"rt"/"pwr" are behavioral sources
    // compiled from expressions.
    tab
        .defaultGround()
        .setDefaultSubDef(
            // Toplevel subcircuit definition, no name, no terminals
            PTSubcircuitDefinition()
            .add(p.parseParameters("rth=(3000-25)/60 cth=10e-3 ta=25 r0=8.612 p0=8.612 p1=0.0269 p2=1.914e-6"))
            .add(PTModel("vsrc", "vsource"))
            // Sine source directly (no need for the .sim demo's initial
            // dc-sweep type="dc"/alter-to-sine dance since we only run tran)
            .add(PTInstance("v1", "vsrc", {"a", "0"})
                .add(p.parseParameters("dc=0 type=\"sine\" sinedc=0.0 ampl=325 freq=50"))
            )
            // Electrical branch: current through the filament, resistance
            // depends on the temperature node "t"
            .add(PTBehavioral("bi", {"a", "0"})
                .setCurrent(p.parseExpression("v(a,0)/(r0*(p0+p1*v(t)+p2*v(t)**2))"))
            )
            // Thermal branch: dissipated power in, ambient cooling and
            // thermal mass out. Declared on the "thermal" discipline, so
            // v(t) inside these sources' own expressions is read back as a
            // temperature (Temp) rather than a voltage.
            .add(PTBehavioral("ct", {"t", "0"})
                .setCurrent(p.parseExpression("ddt(cth*v(t))"))
                .setDiscipline("thermal", "Temp", "Pwr")
            )
            .add(PTBehavioral("rt", {"t", "0"})
                .setCurrent(p.parseExpression("(v(t)-ta)/rth"))
                .setDiscipline("thermal", "Temp", "Pwr")
            )
            .add(PTBehavioral("pwr", {"t", "0"})
                .setCurrent(p.parseExpression("-(v(a,0)**2/(r0*(p0+p1*v(t)+p2*v(t)**2)))"))
                .setDiscipline("thermal", "Temp", "Pwr")
            )
        )
        // Embedded Python postprocessing script
        .add(PTEmbed("runme.py", R"script(
from rawfile import rawread
import matplotlib.pyplot as plt

tran1 = rawread('tran1.raw').get()
time = tran1['time']
Tt = tran1['t']

fig1, ax1 = plt.subplots(1, 1, figsize=(6,4), dpi=100, constrained_layout=True)
fig1.suptitle('Transient response of self-heating lightbulb (behavioral sources)')
fig1.axes[0].set_xlabel('time [s]')
fig1.axes[0].set_ylabel('T [deg C]')
fig1.axes[0].plot(time, Tt)

plt.show()
)script"));

    // Verify tables
    if (!tab.verify(s)) {
        Simulator::err() << s.message() << "\n";
        exit(1);
    }

    // Compile behavioral sources ("bi"/"ct"/"rt"/"pwr") into Verilog-A
    // modules. Must run once, after verify() and before the circuit is built.
    if (!tab.processBehaviorals(0, s)) {
        Simulator::err() << s.message() << "\n";
        exit(1);
    }

    // Dump tables for debugging
    tab.dump(0, Simulator::out());

    // Write embedded files
    if (!tab.writeEmbedded(1, s)) {
        Simulator::err() << s.message() << "\n";
        exit(1);
    }

    // Create circuit, create OpenVAF compiler with no options
    OpenvafCompiler comp;
    // Circuit object
    Circuit cir(tab, &comp, s);
    if (!cir.isValid()) {
        Simulator::err() << s.message() << "\n";
        exit(1);
    }

    // Use Verilog-A nature tolerance, ignore SPICE tolerances
    cir.setOption("tolmode", "va");

    // Elaborate default toplevel circuit
    if (!cir.elaborate({}, "__topdef__", "__topinst__", nullptr, s)) {
        Simulator::err() << "Elaboration failed.\n";
        Simulator::err() << s.message() << "\n";
        exit(1);
    }

    // Dump instance hierarchy
    cir.dumpHierarchy(0, Simulator::out());

    // Analysis description
    auto tranDesc = PTAnalysis("tran1", "tran");
    tranDesc
        .add(PV{"stop", 2})
        .add(PV{"step", 0.01e-3})
        .add(PV{"maxstep", 0.05e-3});

    // Analysis object
    auto tran = Analysis::create(tranDesc, cir, s);
    if (!tran) {
        Simulator::err() << "Failed to create analysis.\n";
        Simulator::err() << s.message() << "\n";
        exit(1);
    }

    // Save directives
    tran->add(PTSave("default"));

    // Run analysis
    auto [ok, canResume] = tran->run(s);
    if (!ok) {
        Simulator::err() << "Tran1 analysis failed.\n";
        Simulator::err() << s.message() << "\n";
        exit(1);
    }
    Simulator::out() << "Tran1 analysis OK. Can resume: " << (canResume ? "true" : "false") << "\n";

    // Cleanup
    delete tran;

    // Run postprocessing
    runProcess(pythonBinary, {"runme.py"}, &pythonLibraryPath, nullptr, false, false);

    return 0;
}
