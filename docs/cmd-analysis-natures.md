# Verilog-A Natures and Tolerances

Natures and disciplines from Verilog-A device models determine how absolute tolerances are applied to circuit unknowns and residuals during simulation. VACASK supports three tolerance modes — SPICE-compatible (`spice`), Verilog-A (`va`), and mixed (`mixed`) — selectable via the `tolmode` simulator option.

## Tolerance modes

Set the tolerance mode in the control block with:

```text
options tolmode="spice"   // default
options tolmode="va"
options tolmode="mixed"
```

### SPICE mode (default)

In SPICE mode VACASK applies fixed tolerances based on node type, independent of the natures defined in the loaded `.osdi` files.

| Node type | Quantity      | Nature name | Absolute tolerance option |
|-----------|---------------|-------------|--------------------------|
| Potential | unknown       | `.voltage`  | `vntol`  |
|           | unknown idt   | `.flux`     | `fluxtol` |
|           | residual      | `.current`  | `abstol` |
|           | residual idt  | `.charge`   | `chgtol` |
| Flow      | unknown       | `.current`  | `abstol` |
|           | unknown idt   | `.charge`   | `chgtol` |
|           | residual      | `.voltage`  | `vntol`  |
|           | residual idt  | `.flux`     | `fluxtol` |

Natures whose name starts with a dot are builtin SPICE natures. Implicit equations are treated as potential nodes. Idt natures apply to reactive residual contributions and are currently used in the element evaluation bypass algorithm.

### Verilog-A mode

Each `.osdi` file carries its own set of natures and disciplines which apply only to the models defined in that file. Each device node applies an absolute tolerance to the circuit node it is connected to. When two devices apply different tolerances to the same node, the lower value wins.

To list the tolerances applied by a specific device add the following command to the control block:

```text
print device("<device name>")
```

The output includes an "Absolute tolerances in Verilog-A mode" section listing tolerances for each node's unknown, its integral, the residual, and the residual's integral.

### Mixed mode

`tolmode="mixed"` uses Verilog-A tolerances where available and falls back to SPICE tolerances for unknowns and residuals that have no Verilog-A tolerance assigned.

## Nature names

### Names from Verilog-A files

Each `.osdi` file has its own set of natures and disciplines that apply only to the models defined in that file. Nature names are used to identify the same nature across different `.osdi` files — for example, when computing global maxima for relative tolerance reference in the nonlinear solver. Two natures from different files are considered the same nature if their names match.

In `print tolerances` output, tolerance names are derived from discipline names rather than being resolved to the underlying nature name, because a discipline declaration can override nature attributes including `abstol`. The naming convention is:

- Tolerance from the flow nature of a discipline: `<discipline>.flow`
- Tolerance from the potential nature of a discipline: `<discipline>.potential`

Tolerances of integral quantities (idt natures) are resolved to the actual nature name, since Verilog-A does not allow the idt nature of a potential or flow to be overridden in a discipline declaration.

### Builtin SPICE nature names

VACASK defines the following builtin natures for use in SPICE tolerance mode. Their names all start with a dot:

| Nature name | Quantity | Units |
|-------------|----------|-------|
| `.voltage`  | Potential | V |
| `.current`  | Current | A |
| `.flux`     | Magnetic flux | Wb=Vs |
| `.charge`   | Charge | C=As |

Builtin devices (independent and controlled linear voltage/current sources) always use these SPICE natures regardless of the active `tolmode`.

## Tolerance scaling

All absolute tolerances can be scaled simultaneously with the `tolscale` option (default `1`):

```text
options tolscale=0.1
```

## Residual tolerance and convergence

Tolerances on unknowns are always enforced. Tolerances on residuals are enforced only for KCL equations of non-internal device nodes. For internal nodes, only a single contribution exists, making a relative tolerance reference ill-defined as the solver converges.

If convergence problems occur due to small residual reference values, try:

- `options relrefres="pointglobal"` — use the maximum contribution across all residuals of the same nature at the current timepoint as the reference.
- `options relrefres="global"` — use the maximum across all timepoints seen so far.
- `options relref="allglobal"` — equivalent to setting `relrefsol`, `relrefres`, and `relreflte` all to `"pointglobal"`.
- `options nr_residualcheck=0` — disable residual tolerance checks entirely.

## Inspecting natures and tolerances

List the natures and disciplines of all loaded `.osdi` files whose canonical name contains a given substring:

```text
print device_file("<file name substring>")
```

List the canonical names of all loaded `.osdi` files:

```text
print device_files
```

Print the tolerances and natures assigned to all circuit unknowns and residuals after elaboration:

```text
print tolerances
```
