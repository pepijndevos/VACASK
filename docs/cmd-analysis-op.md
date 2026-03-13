# Operating Point Analysis

The operating point (OP) analysis finds the DC steady-state solution of a circuit. This is the fundamental analysis that determines the voltages at all nodes and currents through selected branches (mostly voltage sources) when the circuit is in equilibrium.

## Syntax

```text
analysis name op [parameters]
```

## Description

Operating point analysis solves the nonlinear system of equations that describe the circuit's DC behavior. It uses Newton's method with homotopy algorithms to handle convergence issues in difficult circuits.

The analysis computes:
- Node voltages relative to the ground node
- Branch currents of certain branches
- Output variables from device models (e.g., transistor differential conductances and capacitances)

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `nodeset` | string or list | `""` | Initial guess for the solution. Can be a stored solution name (string) or explicit node voltage list. |
| `store` | string | `""` | Name under which to store the computed solution for later use as nodesets/initial conditions in other analyses. |
| `write` | boolean | `1` | Whether to write the results to the output file. Set to 0 to suppress output. |

Nodesets are hnints to the solver specifying what the expected solution should be 
(approximately). You can think of them as starting points for the Newton-Raphson algorithm. 
The results can be used as nodesets or initial conditions for subsequent analyses like 
AC, transient, or noise. For that purpose you can store them in a solution slot. The name of 
the solution slot is specified by the `write` parameter.

## Save Directives

Operating point analysis supports the following save directives to control what data is written to the output file:

| Directive     | Description |
|---------------|-------------|
| `default`     | Saves all node voltages and branch currents (default behavior). |
| `full`        | Saves all unknowns (even those belonging to collapsed ones). |
| `v(node)`     | Saves the voltage at the specified node as `node`. |
| `i(instance)` | Saves the current through the specified instance. Equivalent to `v('instance:flow(br)')`. |
| `p(instance,outvar)` | Saves the specified output variable (opvar) from the given instance as `instance.opvar`|

## Output

- A file `<analysis>.*` containing the operating point.

## Examples

**Basic operating point:**

```text
analysis op1 op
```

**With nodeset from stored solution:**

Uses the solution stored in slot `previous_op` as the nodeset for the analysis. 
```text
analysis op1 op nodeset="previous_op"
```

**With explicit nodeset and storage:**

Applies nodeset of 1.2V to `node1` and a nodeset of 0.8V to voltage difference between nodes `node2` and `node3`. The analysis result is stored in slot `my_op` so that it can be applied as a nodeset or initial condition in subsequent analyses. 

```text
analysis op1 op nodeset=["node1"; 1.2; "node2"; "node3"; 0.8] store="my_op"
```

**Suppress output:**

```text
analysis op1 op write=0
```

**1-dimensional DC sweep**
```text
sweep vds instance="vdd" parameter="dc" from=-0.8 to=1.8 mode="lin" points=100
  analysis dc1 op
```

**2-dimensional DC sweep**
```text
sweep vgs instance="vgg" parameter="dc" values=[0, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8]
  sweep vds instance="vdd" parameter="dc" from=-0.8 to=1.8 mode="lin" points=100
    analysis dc1 op
```

## Convergence

VACASK uses advanced convergence techniques:
- Damped Newton-Raphson method. 
- Homotopy algorithms (gmin stepping, source stepping)
