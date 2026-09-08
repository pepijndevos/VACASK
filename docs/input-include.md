# Including a File

The `include` directive allows you to split large input files into smaller,
manageable pieces. It inserts the contents of another file into the current
input file at the point where the directive appears.

## Basic include

The simplest form includes a file by name:

```text
include "common.inc"
```

The filename must be enclosed in double quotes. Path resolution follows this
order:

1. **Directory of the including file** - If the current input file is in
   `/home/user/circuits/main.sim` and it includes `"models.inc"`, VACASK looks
   for `/home/user/circuits/models.inc`.

2. **Current working directory** - If not found in the including file's
   directory, VACASK checks the directory where VACASK was started.

3. **Include path** - Finally, VACASK searches the include path. This defaults
   to `<VACASK_LIB>/inc` but can be overridden with the `SIM_INCLUDE_PATH`
   environment variable. Use colons to separate multiple directories (semicolons
   on Windows).

Absolute paths are supported and bypass the search order:

```text
include "/usr/local/share/vacask/models.inc"
```

If the file is not found VACASK reports an error and stops parsing. 

## Library includes with sections

For larger projects, you can organize files into libraries with named sections.
This allows selective inclusion of specific parts of a file:

```text
include "library.inc" section=common
```

The file `library.inc` might contain multiple sections:

```text
section common
// Common definitions
model resistor resistor
model capacitor capacitor
endsection

section analog
// Analog-specific models
model opamp opamp
endsection

section digital
// Digital-specific models
model inverter inverter
endsection
```

When you include with `section=common`, only the content between `section common` 
and the next `endsection` directive is included. If the section is not found VACASK
reports an error and stops parsing. 

### Section syntax

Sections begin with:

```text
section section_name
```

The section name follows identifier rules: letters, digits, underscores, and
dollar signs, starting with a letter or underscore. Sections end at the next
`endsection` directive. 

### Library file search

When including with a section, VACASK searches for the file in the same
locations as basic includes, but treats it as a library file. Library files
can contain multiple sections for different purposes.

## Foreign-format includes (SPICE / Spectre)

When VACASK is built with SPICE/Spectre support (the `VACASK_WITH_SPICE` CMake
option, on by default), the `include` directive also accepts SPICE and Spectre
netlist files. They are dispatched to the bundled Rust parser and their
**models, subcircuit definitions, and device instances** are merged into the
top-level circuit, exactly as if you had written them in native VACASK syntax:

```text
include "sky130_models.spice" lang=ngspice
include "corner.lib" lang=ngspice section=tt
include "models.scs" lang=spectre
```

The format is selected solely by the explicit `lang=` value; filename
extensions are not used for dialect inference. Supported values are `ngspice`,
`hspice`, `pspice`, `xyce`, and `spectre`. The `section=` selector works for
SPICE-style `.lib`/`.endl` sections; it is not supported with `lang=spectre`.
Native `.sim` files remain ordinary VACASK includes and do not use `lang=`.

Foreign formats are supported as includes in a native VACASK deck, not as root
input files to the `vacask` command. The native deck supplies the ground,
analysis, control flow, and postprocessing configuration.

Five behaviours are specific to foreign includes:

- **Commands are ignored.** Analysis/control directives inside an included SPICE
  or Spectre file (for example a SPICE `.tran` card or a Spectre analysis
  statement) are **not** translated into VACASK commands; a warning is printed
  and they are skipped. Write your analyses, options, saves, and loads in the
  native VACASK top-level deck. The guiding principle is *translate the models,
  not the testbench*.

- **OSDI models are auto-loaded.** You do not need to `load` the `.osdi` module
  for a device a foreign file uses. VACASK emits the required `load` directives
  automatically for the masters it references (built-in devices such as voltage
  and current sources need no load).

- **SPICE `.model` names get an `m_` prefix.** SPICE keeps `.model` and
  `.subckt` in separate name scopes, so the same name may denote both; a PDK
  such as Sky130 does exactly that. VACASK registers models and subcircuits in
  one scope (an instance may name either), so every name coming from a SPICE
  `.model` card is registered as `m_<name>`. References from within the same
  foreign file are rewritten to match, so nothing has to change in the included
  netlist. It matters only when the **native** deck names a model card itself:

  ```text
  include "models.spice" lang=ngspice   // contains: .model resr r

  r1 (n1 0) m_resr r=1k                 // native reference needs the prefix
  ```

  The prefix is applied unconditionally, not only on collision, so the name a
  card gets never depends on what some other file happens to define. It also
  matches the convention of the Cadnip converter. Subcircuit names and
  references to them are left unchanged. Names of SPICE model cards also appear
  prefixed in `print device(...)` output.

- **SPICE `m=` becomes `$mfactor`.** SPICE spells the parallel-device
  multiplier `m`, both on a device line and on a subcircuit call, where it
  multiplies every device the subcircuit contains. VACASK spells it
  [`$mfactor`](cir-mfactor.md), and a subcircuit must declare it and forward it
  to its contents. The adapter writes that forwarding: every included SPICE
  `.subckt` gains a `$mfactor` parameter defaulting to 1, each device inside it
  receives `$mfactor` multiplied by its own `m=` if it has one, and an `m=` on
  a subcircuit call is passed down the same way, so nested calls compose.

  ```text
  .subckt rblk a b                      parameters $mfactor=1
  rr a b 1k                 becomes     rr (a b) sp_resistor r=1k $mfactor=$mfactor
  .ends
  x1 out 0 rblk m=4                     x1 (out 0) rblk $mfactor=4
  ```

  Devices that impose a potential (`V`, `E`, `H`, and a `B` source written
  `v=`) take no multiplier, matching ngspice: replicating them in parallel
  changes neither the imposed voltage nor any node current. A behavioral source
  has no `$mfactor` parameter, so a current-defining source folds the multiplier
  into its expression. A probe-dependent behavioral resistor uses a generated
  `mfactor` Verilog-A parameter to scale its current and noise contributions.

- **Solution-dependent resistor values become generic behavioral sources.** A
  resistor whose `r=` expression contains `v(...)` or `i(...)` must be evaluated
  with the circuit solution, rather than as an elaboration-time parameter. The
  adapter lowers it through the behavioral `expr=` API and emits the resistor's
  current, thermal noise, and flicker noise from a synthesized Verilog-A body.
  Supported geometry and temperature parameters from the instance, plus noise
  and noise-area parameters from its `.model` card, are forwarded to that body.

- **SPICE `temper` becomes `$temp`.** ngspice names the simulation temperature
  (in degrees Celsius) `temper`; VACASK spells the same quantity in the same units
  [`$temp`](expr-special.md). The identifier is rewritten wherever it appears in
  an included SPICE expression: `.param` cards, model and instance parameters,
  and behavioral source expressions alike. It then tracks option
  [`temp`](cmd-options-temp.md), re-evaluating when it changes:

  ```text
  .param rt = '1000*(1+3.9e-3*(temper-27))'   //  rt=1000*(1+3.9e-3*($temp-27))
  ```

  Whole identifiers are matched, so a parameter named `temperature` or
  `mytemper` is left alone, as is a node named `temper` inside a `v()`/`i()`
  probe. The rewrite is unconditional: a SPICE file that declares its own
  parameter named `temper` gets a warning, and expressions referencing it still
  see the simulator temperature.

- **SPICE `pwr()` is expanded, and it means two different things.** ngspice has
  two expression parsers and they do not agree on `pwr`. In `.param` and
  `.model` values it is `|x|`<sup>`y`</sup>, discarding the sign of the base; in
  a behavioral expression (a `B` source, or a resistance written `r=`) it keeps
  the sign, as `sgn(x)*|x|`<sup>`y`</sup>. Each is expanded to match its own
  context:

  ```text
  .param k = 'pwr(-2,3)'          //  k=pow(abs(-2),3)               -> +8
  b1 out 0 v='pwr(v(in),3)'       //  sgn(v(in))*pow(abs(v(in)),3)   -> -8 at v(in)=-2
  ```

  ngspice's `pwrs` is *not* accepted: it is not an ngspice function at all, only
  a PSPICE-compatibility definition, and VACASK does not implement that mode.

Foreign includes are supported at the **top level** of the deck. An `include` of
a foreign-format file inside a `subckt` body is merged into the top-level
definition rather than that subcircuit.

If VACASK was built with `-DVACASK_WITH_SPICE=OFF`, including a foreign-format
file reports an error asking you to rebuild with the option enabled; native
`.sim` includes are unaffected.

## Nesting includes

Includes can be nested arbitrarily deep. A file included with sections can
itself include other files. VACASK maintains a stack of open files and
prevents infinite recursion by tracking the file stack.

## Use cases

**Modular circuit design** - Split large circuits into functional blocks:

```text
// main.sim
include "power.inc"
include "analog.inc"
include "digital.inc"

// Circuit definition here
```

**Reusable model libraries** - Create libraries of device models:

```text
// models.lib
section basic
model resistor resistor
model capacitor capacitor
endsection

section advanced
model bsim4 bsim4
endsection
```

Then include selectively:

```text
include "models.lib" section=basic
```

## Debugging includes

Use the `-df` command-line option to see which files VACASK loads and their
paths:

```bash
vacask -df circuit.sim
```
