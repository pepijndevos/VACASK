# Circuit Variables

Circuit variables are named values set and updated from the control block. They are available in every parameter expression in the netlist, letting you drive parameterized circuits from a single point of control.

## Setting variables

The `var` command assigns one or more variables:

```text
var name=value [name2=expr ...]
```

`value` or `expr` may be any scalar expression, including references to other variables and builtin constants. Variables are evaluated left to right. First all expression listed on a `var` statement are evaluated, upon which results are written to their destinations. If an expression references a variable that was assigned earlier in the same `var` statement, the value of the variable before the execution of the statement is used, 

```text
var vdd=1.8 vss=0
var gain=5
var gain=10 rfb=gain*1k // gain=5 is used
vat rin=gain*1k         // gain=10 is used
```

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

## Clearing variables

The `var` command only adds or updates names; it never removes them. To remove all  variables use the `clear variables` control block statement. 
