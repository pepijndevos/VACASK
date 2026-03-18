# Analysis Statements

An analysis statement runs a simulation and writes results to output file(s). It must appear inside the `control`/`endc` block.

## Syntax

```text
analysis name type [parameters]
```

- `name` — unique identifier for this analysis run. Used as the base name for the output file (`name.raw`).
- `type` — the kind of analysis to run. See the analysis chapters for available types and their parameters.
- `parameters` — analysis-specific parameters given as `key=value` pairs.

```text
control
  analysis op1 op
  analysis ac1 ac start=1k stop=1g points=100 mode="dec"
endc
```

If the circuit has not been elaborated yet, elaboration is triggered automatically before the analysis runs.

## Preceding sweeps

One or more `sweep` statements may precede an analysis statement on adjacent lines. The sweep statements and the analysis together form a parametric sweep. See [Sweeping](cmd-sweep.md) for details.

```text
control
  sweep vsweep instance="vdd" parameter="dc" from=1.0 to=1.8 step=0.1
    analysis op1 op
endc
```

## Output

Each analysis writes a SPICE raw file named `name.raw` in the current working directory. What is written depends on the active save directives; see [Saving Results](cmd-save.md) and the individual analysis documentation. Some analyses produce multiple files on request, e.g. the AC small-signal analysis stores the operating point results in file `name.op.raw` if this is requested by setting the `writeop` parameter to `1`. 
