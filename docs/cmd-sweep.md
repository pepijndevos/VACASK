# Sweeping

A sweep repeats an analysis over a range of values of a parameter, option, or circuit variable. One or more `sweep` statements placed immediately before an `analysis` statement form a parametric sweep.

## Syntax

```text
sweep name [parameters]
analysis name type [parameters]
```

`name` identifies the sweep. It appears as a vector in the output file holding the swept values at each point.

## What can be swept

Specify exactly one of the following to select the sweep target:

| Keyword | Sweeps |
|---------|--------|
| `instance="name" parameter="p"` | Instance parameter `p` of instance `name` |
| `model="name" parameter="p"` | Model parameter `p` of model `name` |
| `option="name"` | Simulator option `name` |
| `variable="name"` | Circuit variable `name` |

If the instance parameter name is omitted the principal parameter is swept. For Verilog-A devices the principal parameter is the first instance parameter listed in the Verilog-A module. 

## Sweep range

Specify the range with one of these forms:

**Stepped (uniform step):**
```text
sweep s instance="vgs" parameter="dc" from=0 to=3 step=0.5
```

**Linear (fixed number of points):**
```text
sweep s instance="vgs" parameter="dc" from=0 to=3 mode="lin" points=100
```

**Logarithmic (points per decade or octave):**
```text
sweep s instance="vgs" parameter="dc" from=1k to=1g mode="dec" points=10
sweep s instance="vgs" parameter="dc" from=1k to=1g mode="oct" points=3
```

**Explicit values:**
```text
sweep s variable="corner" values=["tt", "ff", "ss"]
```

## Nested sweeps

Multiple `sweep` lines before a single `analysis` produce a multidimensional sweep. The last `sweep` is the inner (fastest) loop:

```text
sweep vgs instance="vgs" parameter="dc" from=1 to=3 step=0.5
sweep vds instance="vds" parameter="dc" from=0 to=5 step=0.1
  analysis dc1 op
```

## Parametric expressions in sweep properties

All sweep range properties (`from`, `to`, `step`, `points`, `values`) accept expressions, not just literals. Expressions are evaluated during analysis and may reference circuit variables and builtin constants:

```text
var vdd=1.8 vstep=0.1

sweep vds instance="vds" parameter="dc" from=0 to=vdd step=vstep
  analysis dc1 op
```

This makes it easy to adjust a sweep range by changing a single parameter without editing the sweep statement itself. You can also sweep a variable: 

```text
var vdd=1.8

sweep v_sweep variable="vdd" from=0 to=1.8 step=0.1
  analysis op1 op
```

A variable swept by an outer sweep can be used in the inner sweep's settings:

```text
var vsup=4

sweep vsup variable="vsup" values=[1, 2, 3, 4]
sweep v1 instance="v1" parameter="dc" from=0 to=vsup mode="lin" points=5
  analysis op1 op
```

You can use swept variables for the analysis settings: 

```text
var fmax=1k

sweep fmax_sweep variable="fmax" values=[1k, 2k, 5k, 10k]
  analysis ac1 ac from=1 to=fmax mode="lin" points=100
```

When sweeping a string parameter via `values` the swept value stored in the .raw file is the index of the value specified in the `values` vector/list. 

```text
var corners=["tt", "ff", "ss", "fs", "sf"]

sweep corner_sweep instance="xmod" parameter="corner" values=corners
  analysis op1 op
```

## Continuation

By default (`continuation=1`) the simulator uses the solution from the previous sweep point as the starting point for the next one. This reduces the number of iterations needed by the nonlinear solver and speeds up the simulation. Set `continuation=0` to start each point from scratch:

```text
sweep s variable="corner" values=["tt", "ff", "ss"] continuation=0
  analysis op1 op
```

## Output

The swept variable appears as a vector named after the sweep in the output file alongside the analysis results. For nested sweeps each sweep has its own vector. For a nested n-dimensional sweep the first n columns (vectors) in a .raw file correspond to the swept properties. The results are concatenated in 1-dimensional vectors in the .raw file. 
