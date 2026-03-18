# The Control Block

The control block contains the simulation commands. It is separated from the circuit description and is the only place where analyses, sweeps, options, save directives, and other commands may appear.

## Syntax

```text
control
  [commands and analyses]
endc
```

The `control` and `endc` keywords must each appear at the start of a line. Only one control block is allowed per netlist file. It must be part of the top-level circuit definition — control blocks inside subcircuit definitions are not allowed.

## Execution model

Commands in the control block are executed in order from top to bottom. State accumulates as execution proceeds:

- **Save directives** (`save`) accumulate and are applied to all subsequent analyses until cleared.
- **Options** (`options`) accumulate and are applied just before the next elaboration or analysis.
- **Circuit variables** (`var`) are applied just before the next elaboration or analysis.

If the circuit has not been elaborated when an `analysis`, `print`, `alter`, or `elaborate changes` command is reached, default elaboration is triggered automatically.

## Summary of control block commands

| Command | Purpose |
|---------|---------|
| `analysis name type [params]` | Run an analysis. See [Analysis Statements](cmd-analysis.md). |
| `sweep name [params]` | Sweep a parameter over a range. See [Sweeping](cmd-sweep.md). |
| `save directives` | Select output quantities. See [Saving Results](cmd-save.md). |
| `options name=value ...` | Set simulator options. See [Simulator Options](cmd-options.md). |
| `var name=expr ...` | Set circuit variables. See [Modifying Circuit Variables](cmd-var.md). |
| `elaborate circuit ...` | Elaborate the circuit explicitly. See [Circuit Elaboration](cmd-elaboration.md). |
| `elaborate changes` | Re-elaborate only affected parts of an already-elaborated circuit. |
| `alter instance(...) p=v ...` | Modify instance or model parameters after elaboration. |
| `print ...` | Print circuit information to the console. |
| `postprocess(prog, args...)` | Run an external program after an analysis. |
| `clear [var\|saves\|options]` | Clear accumulated state. |
| `abort ...` | Configure error handling behaviour. |

## Example

```text
Sample control block

load "spice/mos1.osdi"
model nmos sp_mos1 type=1 tox=50n

vin (in 0) vsource dc=0
r1 (in out) resistor r=1k

control
  options rawfile="binary"
  save v(out) i(vin)
  sweep vin_sweep instance="vin" parameter="dc" from=0 to=3 step=0.1
    analysis dc1 op
  postprocess(PYTHON, "plot.py")
endc
```
