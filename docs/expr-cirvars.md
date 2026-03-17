# Circuit Variables

Circuit variables are named values set and updated from the control block. They are available in every parameter expression in the netlist, letting you drive parameterized circuits from a single point of control.

## Setting variables

The `var` control block statement assigns one or more variables:

```text
var name=expr [name2=expr ...]
```

Example:
```text
control
  ...
  var vdd=1.8
  var gain=5
  ...
endc
```

For details see [Modifying Circuit Variables](cmd-var.md).

## Scope and availability

Variables live in a global context that is shared across the entire netlist. Any parameter expression — in instances, models, subcircuits, analyses, or sweeps — can reference a circuit variable by name. If a variable is set before elaboration, it participates in the initial parameter evaluation. If it is changed afterward, all parameterized values that depend on it are re-evaluated before the next analysis runs.

## Built-in variables

| Variable | Value | Description |
|----------|-------|-------------|
| `PYTHON`  | path string | Absolute path to the Python interpreter used by VACASK. Set automatically at startup. |

VACASK by default looks for the Python interpreter in the system path. When executing Python scripts through the `postprocess` control block statement the `PYTHONPATH` environmental variable is extended with the path to the Python helper scripts distributed with VACASK. 

## Sweeping variables

A circuit variable can be the target of a parameter sweep. Use `variable="name"` in a sweep statement to step through a range of values while re-running an analysis:

```text
sweep vsweep variable="vdd" start=1.0 stop=1.8 step=0.1 
  op1 op
```

Each step updates `vdd`, triggers re-evaluation of all dependent parameters, and runs the nested analysis block.

