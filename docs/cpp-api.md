# C++ API

VACASK's simulation engine is a static library, `simlib`, built from `lib/` and exposed through the headers in `include/`. The `vacask` executable itself (`simulator/main.cpp`) is a thin driver: it parses a netlist file, elaborates the circuit, and interprets the control block through `simulator/cmd.h`'s `CommandInterpreter` - which is *not* part of `simlib`. An API program that wants the same result (parse a netlist, run its analyses) has to reproduce that driving logic itself, using the classes documented here. This is also what makes the API useful beyond replicating `vacask`: a program can build a circuit programmatically, without ever writing netlist syntax, and can change parameters, options, or topology between analyses under full C++ control.

Six complete, runnable programs demonstrating the API live in `demo/api/` (`demo1.cpp` through `demo6.cpp`); this document explains the pattern they all follow and points back to the relevant demo for each topic. Every symbol below is in `namespace sim`.

## Linking against `simlib`

```cmake
add_executable(myprog myprog.cpp)
add_dependencies(myprog simlib)
target_link_libraries(myprog simlib ${SIM_EXE_LIBRARIES})
```

See `demo/api/CMakeLists.txt` for the exact pattern, including the static-link options needed under Windows. `simlib`'s public headers are the ones in `include/`; nothing in `simulator/` (the `vacask` executable's own sources, including the control-block interpreter) is part of the library.

At startup a program must still be able to find OSDI device modules and, if it uses embedded Python postprocessing, the bundled Python helper modules (see [Python Helpers](python-overview.md)) and a Python interpreter:

```cpp
#include "libplatform.h"
#include "simulator.h"

std::string modulePath = "../../lib/vacask/mod";        // staged .osdi files
std::string pythonLibraryPath = "../../lib/vacask/python"; // staged rawfile.py etc.
std::string pythonBinary = findPythonExecutable();

Simulator::setup();
Simulator::prependModulePath({modulePath});
```

`findPythonExecutable()` (`libplatform.h`) locates a Python interpreter on the system path. `Simulator::setup()` initializes the simulator's global state (registers builtin devices and analysis types) and must be called once before anything else. `Simulator::prependModulePath()`/`appendModulePath()` extend the search path `load` directives use to resolve device files (see [Loading Devices](cir-loading.md)); `prependIncludePath()`/`appendIncludePath()` do the same for `include` directives.

## Workflow overview

Every API program follows the same sequence, regardless of whether the circuit is built programmatically or parsed from netlist text:

1. **Describe** the circuit into a `ParserTables` object - either by building it with the `PT*` fluent builder classes (`parseroutput.h`), or by parsing netlist syntax with `Parser` (`parser.h`).
2. **Verify** the description (`ParserTables::verify()`), process any behavioral sources (`processBehaviorals()`), and write out any embedded files (`writeEmbedded()`).
3. **Construct** a `Circuit` (`circuit.h`) from the tables, then **elaborate** it (`Circuit::elaborate()`).
4. **Describe and run** one or more analyses: build a `PTAnalysis`, create it with `Analysis::create()` (`an.h`), attach save directives, and call `run()`.
5. Optionally **change** circuit variables, options, or instance/model parameters and **re-elaborate incrementally** (`Circuit::elaborateChanges()`) before running further analyses.
6. Optionally **postprocess** results, e.g. by running an embedded Python script with `runProcess()` (`processutils.h`).

| Header | Provides |
|---|---|
| `libplatform.h` | Platform helpers: `findPythonExecutable()`, path utilities. |
| `simulator.h` | `Simulator` - global setup, module/include search paths, output streams. |
| `status.h` | `Status` - the error-reporting type used throughout the API. |
| `value.h` | `Value` - the tagged-union type holding parameter/option/variable values. |
| `parser.h` | `Parser` - parses expressions, parameter lists, or whole netlist files/strings. |
| `parseroutput.h` | `ParserTables` and the `PT*` classes - the in-memory, buildable circuit description. |
| `openvafcomp.h` | `OpenvafCompiler` - compiles Verilog-A source files to OSDI on demand. |
| `circuit.h` | `Circuit` - elaboration, variables, options, instance/model parameters. |
| `an.h` | `Analysis` - creating, configuring, and running analyses. |
| `processutils.h` | `runProcess()` - launching an external postprocessing script. |

## Error handling: `Status`

Almost every API call that can fail takes a `Status& s` output parameter (`status.h`), defaulting to the singleton `Status::ignore` when the caller doesn't care about the failure reason. A freshly constructed `Status s;` starts clear; `s.message()` returns the accumulated error text after a call returns `false`. `Status` is move-only (never copied), so pass it by reference:

```cpp
Status s;
if (!cir.elaborate({}, "__topdef__", "__topinst__", nullptr, s)) {
    Simulator::err() << "Elaboration failed.\n" << s.message() << "\n";
    exit(1);
}
```

`Simulator::out()`/`err()`/`dbg()` (`simulator.h`) are the streams the library itself writes diagnostic and dump output to; using them for a program's own messages keeps output consistent with `-df`/`-dt`-style debug dumps.

## Building the circuit description

### Parser tables and the parser

`ParserTables` (`parseroutput.h`) is the in-memory description a `Circuit` is built from - the same structure the netlist parser itself fills in. A `Parser` (`parser.h`) is needed alongside it even when the circuit is built entirely with `PT*` objects, because expressions and ad hoc parameter lists still need to be parsed into `Rpn`/`PTParameters` values:

```cpp
ParserTables tab("RC transient");   // title, appears in raw file headers
Parser p(tab);                      // shares tab's storage for parsed expressions
```

`Parser` also accepts whole netlist files or strings, as an alternative to building the description with `PT*` objects - useful for loading an existing `.sim` file into a program that then drives analyses itself:

```cpp
bool parseNetlistFile(FileStackFileIndex fileIndex, Status& s=Status::ignore);
bool parseNetlistString(std::string input, Status& s=Status::ignore);
Rpn parseExpression(std::string input, Status& s=Status::ignore);          // throws on error
PTParameters parseParameters(std::string input, Status& s=Status::ignore); // e.g. "r=100 tc1=1e-3"
```

A file must first be registered with `tab.fileStack()` (see `filestack.h`) to obtain a `FileStackFileIndex`; `vacask`'s own `main.cpp` shows the exact sequence. Loading netlist text this way still leaves the program responsible for driving analyses - `simulator/cmd.h`'s control-block interpreter is not part of `simlib` (see above).

### Loading devices

```cpp
tab.add(PTLoad("resistor.osdi"));
tab.add(PTLoad("my_model.va"));   // compiled to OSDI on demand, see below
```

`PTLoad` corresponds to the netlist `load` directive (see [Loading Devices](cir-loading.md)); its optional `map` parameter renames or filters the devices a file exports, same as in netlist syntax.

### Ground and global nodes

```cpp
tab.defaultGround();   // node "0" is ground, unless a ground node was already added
tab.addGlobal(PTParsedIdentifier("vdd"));
```

### Models and instances

The default toplevel subcircuit is a `PTSubcircuitDefinition` set with `setDefaultSubDef()`; `PTModel` and `PTInstance` are added to it (or to a nested `PTSubcircuitDefinition`, see below) with a fluent `add()` chain:

```cpp
tab.setDefaultSubDef(
    PTSubcircuitDefinition()
    .add(PTModel("res", "resistor"))
    .add(PTModel("vsrc", "vsource"))
    .add(PTInstance("r1", "res", {"1", "2"})
        .add(PV{"r", 1000})                          // constant parameter
    )
    .add(PTInstance("v1", "vsrc", {"1", "0"})
        .add(p.parseParameters("type=\"pulse\" val0=0 val1=5 width=4m"))
    )
);
```

`PTModel(name, device)` names a model after an already-loaded device (or, for a subcircuit definition, the reserved device name `"__hierarchical__"`, which is exactly what the `PTSubcircuitDefinition` constructor uses). `PTInstance(name, master, terminals)` instantiates a model or subcircuit definition by name; `terminals` is a `PTIdentifierList` (a `vector<PTParsedIdentifier>`), most conveniently written as a brace-init list of terminal-name strings as shown above.

### Parameters: constants and expressions

A parameter is either a constant `PTParameterValue` (alias `PV`) or an `PTParameterExpression` (alias `PE`) holding a parsed `Rpn`:

```cpp
.add(PV{"r", 1000})                                    // constant
.add(PE{"c", p.parseExpression("2*c0")})                // expression, evaluated at elaboration
.add(p.parseParameters("is=1e-12 n=2 rs=1"))             // several parameters from one string
```

`PVv(...)`/`PEv(...)` build a `std::vector<PV>`/`std::vector<PE>` from a parameter pack, for passing several constant/expression parameters through `PTParameters`'s vector constructor at once (see `demo1.cpp`). `PTModel`, `PTInstance`, `PTSubcircuitDefinition`, `PTLoad`, `PTSweep`, `PTAnalysis`, and `PTBehavioral` all accept parameters through the same `add()` overload set.

### Subcircuits and hierarchy

A `PTSubcircuitDefinition` can be nested inside another (its `add(PTSubcircuitDefinition&&)` overload), corresponding to a subcircuit `.subckt` definition nested in the netlist (see [Subcircuits and Hierarchy](cir-hierarchy.md)):

```cpp
.add(PTSubcircuitDefinition("mysub", {"p", "out", "n"})
    .add(PV("r", 1000))
    .add(PTInstance("r1", "res", {"p", "out"}).add(PE("r", p.parseExpression("r*0.5"))))
    .add(PTInstance("r2", "res", {"out", "n"}).add(PE("r", p.parseExpression("r*0.5"))))
)
.add(PTInstance("x1", "mysub", {"1", "2", "0"}).add(PV{"r", 1000}))
```

`Circuit::elaborate()`'s `toplevelDefinitions` argument (see below) selects, by name, which additional subcircuit definitions besides the default one are instantiated at the toplevel - the mechanism `demo4.cpp` uses to switch between two alternative circuit topologies without reparsing anything.

### Conditional blocks

`PTBlockSequence`/`PTBlock` mirror the netlist's `if`/`else` conditional blocks (see [Conditional Netlist Blocks](cir-conditional.md)): each entry pairs a condition expression (an empty `Rpn()` for the unconditional "else" branch) with a `PTBlock` of models/instances/behaviorals to include when it is true:

```cpp
.add(PTBlockSequence()
    .add(p.parseExpression("mode==0"), PTBlock()
        .add(PTInstance("r1", "res", {"p", "out"}).add(PE("r", p.parseExpression("r*(1-fact)"))))
    )
    .add(Rpn(), PTBlock()   // else
        .add(PTInstance("r1", "res", {"p", "out"}).add(PE("r", p.parseExpression("r*fact"))))
    )
)
```

### Behavioral sources

`PTBehavioral` builds a two-terminal voltage or current source directly from an expression (see [Nonlinear Behavioral Sources](dev-builtin-behavioral.md)), without a separate `PTModel`/device load:

```cpp
tab.add(PTBehavioral("bi", {"a", "0"})
    .setCurrent(p.parseExpression("v(a,0)/(r0*(p0+p1*v(t)+p2*v(t)**2))"))
);
```

`setCurrent()`/`setVoltage()` select the source kind; `setDiscipline()` overrides the default `electrical`/`V`/`I` discipline and accessor names (needed for a thermal or other non-electrical branch, as in `demo6.cpp`).

`setExpression()` selects a third kind - a custom Verilog-A body - instead of having VACASK generate the branch contribution itself: `expr`'s translated value is only made available, through the literal placeholder `#expr#`, to a hand-written analog-block body given via `setUserEvaluation()`, with `setUserDeclarations()` supplying matching module-level Verilog-A text (parameters, local variables) that body can refer to (see [Custom Verilog-A Body](dev-builtin-behavioral.md#custom-verilog-a-body)):

```cpp
tab.add(PTBehavioral("b1", {"a", "0"})
    .setExpression(p.parseExpression("v(a)*2"))
    .setUserDeclarations("real val;")
    .setUserEvaluation("val = #expr#; I(br) <+ val;")
);
```

`setUserEvaluation()`'s text is not optional in practice - an empty body leaves the synthesized analog block empty, so the instance contributes nothing. Both strings are spliced verbatim into the generated module, so they must use that module's own generated names, not the netlist names on the source line: the source's own two terminals are reachable through the branch alias `br` (`I(br) <+ ...`/`V(br) <+ ...`), but any extra node or instance pulled in through `v(...)`/`i(...)` inside the `expr` passed to `setExpression()` is only reachable under a generated `__v<N>_...`/`__i<N>_...` name - there is no API-level way to predict it, so check the synthesized `.va` file (or `print model(...)` on the running circuit) before referencing it from `setUserEvaluation()`.

`PTBehavioral::add()` (the same `PV`/`PE`/`PTParameters` overload set every other `PT*` object uses) is how a `declarations=`-declared Verilog-A parameter gets its value - the netlist equivalent is the second, comma-separated parameter list after `expr=.../declarations=.../evaluation=...`. These are ordinary parameters of the synthesized module, distinct from the ones VACASK auto-generates for each free identifier referenced inside `expr` (which are never set this way, only referenced by `#expr#`):

```cpp
tab.add(PTBehavioral("b1", {"a", "0"})
    .setExpression(p.parseExpression("v(a)*$userparam(p)"))
    .setUserDeclarations("parameter real p=1; real val;")
    .setUserEvaluation("val = #expr#; I(br) <+ val;")
    .add(PV("p", 10))   // overrides the p=1 default from setUserDeclarations()
);
```

It is an error for a name added this way to collide with one VACASK already generated for a referenced identifier.

Behavioral sources are translated into synthesized Verilog-A modules and compiled through the same OSDI pipeline as ordinary devices - this translation step must be triggered exactly once, after `verify()` and before the `Circuit` is constructed:

```cpp
if (!tab.verify(s)) { /* ... */ }
if (!tab.processBehaviorals(0, s)) { /* ... */ }   // debug level, status
```

### Embedded files

`PTEmbed(filename, contents)` embeds file contents directly in the description - most commonly a Python postprocessing script (see [Embedded Files](input-embed.md)):

```cpp
tab.add(PTEmbed("runme.py", R"script(
from rawfile import rawread
plot = rawread('tran1.raw').get()
print(plot.names)
)script"));
```

`ParserTables::writeEmbedded(debugLevel, s)` writes every embedded file to disk in the current working directory; call it once, after `verify()`. 

### Verifying the description

Once the description is complete, the calls from the previous sections run in a fixed order - `verify()` first, `processBehaviorals()` only if the circuit uses `PTBehavioral` (skip it otherwise), `writeEmbedded()` last:

```cpp
if (!tab.verify(s)) { Simulator::err() << s.message() << "\n"; exit(1); }
if (!tab.processBehaviorals(0, s)) { /* ... */ }   // only if PTBehavioral was used
tab.dump(0, Simulator::out());                      // optional: print the built tables
if (!tab.writeEmbedded(1, s)) { /* ... */ }
```

`verify()` runs the more thorough checks appropriate for a manually built circuit (`verifyAfterParse()` is the lighter variant the netlist parser itself uses, since a description that came from valid netlist syntax already satisfies more invariants by construction).

## Creating and elaborating the circuit

### Constructing the `Circuit`

```cpp
OpenvafCompiler comp;                 // no extra compiler args
Circuit cir(tab, &comp, s);
if (!cir.isValid()) { Simulator::err() << s.message() << "\n"; exit(1); }
```

`OpenvafCompiler` (`openvafcomp.h`) is the `SourceCompiler` implementation that compiles `.va` files loaded via `PTLoad`/`load` to OSDI with the OpenVAF-Reloaded compiler; pass `nullptr` instead if the circuit only loads already-compiled `.osdi` files. The `Circuit` constructor processes every `PTLoad` in `tab` immediately, so `cir.isValid()` must be checked right after construction.

```cpp
OpenvafCompiler(
    std::optional<const std::string> compiler = std::nullopt,
    std::optional<const std::vector<std::string>> compilerArgs = std::nullopt
);
```

Both constructor arguments are optional and are simply stored - the constructor does no filesystem or process work itself; compilation happens lazily, the first time a loaded `.va` file actually needs it (no matching `.osdi` file, or one older than the source).

- `compiler` - path or bare name of the OpenVAF-Reloaded binary. Omitted (`std::nullopt`, as in `OpenvafCompiler comp;` above), it falls back to `defaultOpenVafBinaryName()` (`"openvaf-r"`/`"openvaf-r.exe"`), resolved the same way `vacask` itself resolves it: the directory the simulator is installed in, then the system `PATH` (see [Loading Devices](cir-loading.md)).
- `compilerArgs` - extra command-line arguments prepended before the `-o <output> <input.va>` arguments compilation always appends. Omitted, no extra arguments are passed.

`vacask`'s own driver (`simulator/main.cpp`) instead passes both explicitly, sourced from the TOML config file's `[Binaries]` section:

```cpp
OpenvafCompiler comp(Platform::openVaf(), Platform::openVafArgs());
```

### Full elaboration

```cpp
bool elaborate(
    const std::vector<Id>& toplevelDefinitions={},
    const std::string& topDefName="__topdef__", const std::string& topInstName="__topinst__",
    DeviceRequests* devReq=nullptr,
    Status& s=Status::ignore
);
```

```cpp
if (!cir.elaborate({}, "__topdef__", "__topinst__", nullptr, s)) { /* ... */ }
```

`toplevelDefinitions` names additional subcircuit definitions (besides the default one) to instantiate at the toplevel, each getting its own top instance (see [Subcircuits and Hierarchy](cir-hierarchy.md) and `demo4.cpp`); `topDefName`/`topInstName` are the prefixes used to build internal model/instance names for these top-level instances and rarely need to be anything other than the defaults. `devReq`, if non-null, is filled in with whether a Verilog-A `$abort`/`$finish`/`$stop` was requested while evaluating parameter expressions during elaboration (`DeviceRequests`, `elsetup.h`); pass `nullptr` unless the circuit depends on catching that.

Elaboration does **not** reset circuit variables or simulator options - set them (see below) either before the first `elaborate()` call or afterward, followed by `elaborateChanges()`. `cir.clearVariables()`/`cir.clearOptions()` reset them explicitly if needed.

### Variables, options, and instance/model parameters

```cpp
cir.setVariable("myvar", 60);                                    // circuit variable, referenced as bare "myvar" in expressions
cir.setOption("reltol", 1e-4);                                   // simulator option
cir.setInstanceParameter("v1", "dc", 20);                        // instance parameter
cir.setModelParameter("dio", "is", 1e-12);                       // model parameter

auto var = cir.getVariable("myvar");                              // const Value*, nullptr if not found
auto [ok, val] = cir.instanceParameter("r1", "r", s);              // ok, value
Simulator::out() << "temp=" << cir.simulatorOptions().core().temp << "\n";
```

Every setter takes a `Value` (`value.h`, implicitly constructible from `Int`/`Real`/`String`/vectors thereof) and returns whether the change actually took effect (and, for `setVariable()`/`setOption()`, whether it differs from the previous value). Setting an instance/model parameter or a hierarchy-affecting option marks the circuit as needing elaboration - `Circuit::needsElaboration()` reflects this - but does not elaborate it immediately; see below.

`setOptions(const PTParameters&, Status&)` and `setInstanceParameters()`/`setModelParameters()` apply several parameters at once, e.g. from `p.parseParameters(...)`, letting an option or parameter be given as an expression rather than a constant (`demo2.cpp`/`demo3.cpp`).

### Partial (incremental) elaboration

After the first full `elaborate()`, further changes are normally applied with `elaborateChanges()` rather than a second `elaborate()` call - it re-propagates only what actually changed (variables, instance/model parameters, options) down the hierarchy, rebuilding the topology only where necessary:

```cpp
std::tuple<bool, bool, bool> elaborateChanges(DeviceRequests* devReq, Status& s=Status::ignore);
// returns: ok, topologyChanged, analysisRebindingNeeded
```

```cpp
cir.setVariable("tnominal", 40);
cir.setInstanceParameter("v1", "dc", 20);
if (auto [ok, topologyChanged, bindingNeeded] = cir.elaborateChanges(nullptr, s); !ok) {
    Simulator::err() << s.message() << "\n"; exit(1);
} else if (topologyChanged) {
    Simulator::out() << "Topology changed.\n";
}
```

`bindingNeeded` reports whether an analysis needs to rebind to the circuit's Jacobian/sparsity structures - this happens automatically inside an analysis's own sweep loop, so an API user driving analyses with `Analysis::run()` (below) never needs to act on it directly; it exists mainly to inform diagnostics. See `demo5.cpp` for a full walk-through of setting variables/options/parameters, re-elaborating, and inspecting the resulting state between steps.

## Running analyses

### Describing and creating an analysis

```cpp
auto tranDesc = PTAnalysis("tran1", "tran");   // name, analysis type ("op", "tran", "ac", ...)
tranDesc.add(PV{"step", 1e-6}).add(PV{"stop", 10e-3});

auto tran = Analysis::create(tranDesc, cir, s);
if (!tran) { Simulator::err() << s.message() << "\n"; exit(1); }
```

`PTAnalysis(name, typeName)` corresponds to an `analysis` statement (see [Analysis Statements](cmd-analysis.md)); `typeName` is one of the type names registered with `Analysis::registerFactory()` by `Simulator::setup()`: `op`, `dcinc`, `dcxf`, `ac`, `acxf`, `acstb`, `acsp`, `noise`, `tran`, `hb`, `hbac`, `pss` (see [Circuit Analyses](cmd-analysis-overview.md); [Transient Noise Analysis](cmd-analysis-trannoise.md) is `tran` with `noisefmax` set, not a separate type). Its parameters are added the same way as any other `PT*` object's - as constants (`PV`), expressions (`PE`), or parsed strings - and are documented, per analysis type, in the corresponding page under [Circuit Analyses](cmd-analysis-overview.md).

[Monte Carlo Analysis](cmd-analysis-mc.md) (`mc ... endmc`) is a different kind of construct: a looping control-block command implemented in `simulator/cmd.cpp`'s `CommandInterpreter`, not an `analysis` statement or an `Analysis` subclass - like the interpreter itself, it is not part of `simlib` and has no `PTAnalysis`/`Analysis::create()` equivalent. Reproducing it means writing the sampling loop (randomizing parameters marked with a distribution function, re-elaborating, and running the analyses inside the loop) in the API program itself.

`Analysis::create()` is a factory: it looks up `typeName` in a registry populated by `Simulator::setup()` and returns a heap-allocated `Analysis*` (owned by the caller - `delete` it when done) or `nullptr` on failure.

### Sweeps

```cpp
tranDesc.add(PTSweep("mode")
    .add(PV("instance", "x1"))
    .add(PV("parameter", "mode"))
    .add(PV("values", IntVector{0, 1}))
);
```

`PTSweep(name)` corresponds to a `sweep` statement (see [Sweeping](cmd-sweep.md)) and is added to a `PTAnalysis` before creating the `Analysis`; its parameters (`instance`/`model`/`parameter`, `option`, `variable`, and the range parameters `from`/`to`/`step`/`mode`/`points`/`values`) are exactly the ones documented there. Multiple `PTSweep`s stack, outermost first, into a nested sweep.

### Save directives

```cpp
tran->add(PTSave("default"));
tran->add(PTSave("p", "r1", "i"));   // p(r1, i) - one instance output variable
```

`Analysis::add(const PTSave&)`/`add(const PTSaves&)` attach save directives after the `Analysis` object exists (unlike sweeps, which are part of the `PTAnalysis` description). `PTSave(typeName, arg1, arg2)` mirrors the netlist `save` directive's forms (see [Saving Results](cmd-save.md) for the common ones, and each analysis's own doc under [Circuit Analyses](cmd-analysis-overview.md) for analysis-specific directives like AC's `dv`/`di` or noise's `n`/`nc`).

### Running and cleanup

```cpp
auto [ok, canResume] = tran->run(s);
if (!ok) { Simulator::err() << "Analysis failed.\n" << s.message() << "\n"; exit(1); }
delete tran;
```

`run(s)` drives the analysis (and any sweep attached to it) to completion in one call, returning whether it finished without error and whether it could, in principle, be resumed (relevant only to a sweep left mid-run by a request like `$stop`, not to an ordinary completed run). After `run()` is finished (cannot be resumed anymore), the circuit's variables, options, instance/model parameters, and topology are restored to what they were before the call - an API program can freely run further, independent analyses against the same `Circuit` object afterward, as `demo3.cpp`/`demo4.cpp` do.

### Step-by-step control (advanced)

`Analysis` also exposes the coroutine `run()` drives internally, for a program that needs to interleave its own logic with individual sweep points rather than running an analysis to completion in one call:

```cpp
bool start(Status& s=Status::ignore);   // create and prime the coroutine
bool isRunning();
AnalysisState resume();                 // Aborted, Stopped, Finished, or SweepPoint
bool finish(Status& s=Status::ignore);  // tear down cleanly
```

`resume()` returns `AnalysisState::SweepPoint` after each point of an attached sweep, letting the caller inspect intermediate state before continuing; `run()` is simply a loop over `start()`/`resume()` that stops at `Finished`/`Aborted`/`Stopped`. Most programs only need `run()`.

## Postprocessing

```cpp
#include "processutils.h"

runProcess(pythonBinary, {"runme.py"}, &pythonLibraryPath, nullptr, false, false);
```

`runProcess()` (`processutils.h`) launches an external program (typically the Python interpreter found by `findPythonExecutable()`) with the given arguments, prepending `pythonLibraryPath` to `PYTHONPATH` so an embedded script can `import rawfile` (see [Python Helpers](python-overview.md)) without further setup. Run it after the analyses that produce the files a script reads have finished.

## Demo programs

| File | Demonstrates |
|---|---|
| `demo1.cpp` | The full basic workflow: load devices, build a circuit, run a transient analysis, postprocess. |
| `demo2.cpp` | A parameterized subcircuit with a conditional block, swept over an instance parameter. |
| `demo3.cpp` | Circuit variables and parameterized options, swept two different ways (`variable=` vs `option=`). |
| `demo4.cpp` | Elaborating alternative toplevel topologies (`toplevelDefinitions`) without reparsing. |
| `demo5.cpp` | Partial elaboration: inspecting circuit state as variables/options/parameters change and `elaborateChanges()` runs. |
| `demo6.cpp` | Behavioral sources (`PTBehavioral`, `processBehaviorals()`) building a self-heating device with no OSDI loads at all. |

Each is a complete `main()`; build them via the top-level CMake build (they are part of the `demo` target tree) and run from the `build.*/demo/api` directory so the relative `modulePath`/`pythonLibraryPath` used in the examples above resolve correctly.
