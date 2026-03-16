# DC Small-Signal Transfer Function Analysis (dcxf)

The DC small-signal transfer function analysis (`dcxf`) computes the small-signal
transfer functions from all independent sources to a selected output. It also computes 
input impedances of the circuit at the terminals of each independent source. 
It linearises the circuit at its operating point and evaluates how small perturbations 
in each independent source affect a specified output.

## Syntax

```text
analysis name dcxf [parameters]
```

## How it works

1. VACASK first performs an operating point (OP) analysis to obtain the DC
   operating point (node voltages, device biases, etc.).
2. It computes the resistive Jacobian matrix at the operating point.
3. For each independent source in the circuit, it injects a unit incremental
   excitation and solves the linear system.
4. The results are used to compute:
   - **Transfer function** from the source to a designated output node (or node pair)
   - **Input impedance** (`Zin`) and **input admittance** (`Yin`) seen by each source

## Parameters

DC transfer function analysis exposes the operating point parameters and adds
its own parameters:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `nodeset` | string or list | `""` | Initial guess for the operating point solution. |
| `store` | string | `""` | Store the computed operating point solution for reuse. |
| `out` | string/vector | `""` | Output node or node pair used for transfer function evaluation. Specify as a single node name or two node names in a list. |
| `write` | boolean | `1` | Write analysis results to a file. |
| `writeop` | boolean | `0` | Also write the underlying operating point results to `<analysis>.op.*`. |

## Save directives

The `dcxf` analysis supports the following save directives:

| Directive | Description |
|-----------|-------------|
| `default` | Save all transfer functions and input impedances. |
| `tf(source)` | Save transfer function of the given independent source to the output as `tf(source)`. |
| `zin(source)` | Save input impedance seen by the given source as `zin(source)`. |
| `yin(source)` | Save input admittance seen by the given source as `yin(source)`. |

In addition, `dcxf` supports all operating point save directives (e.g.
`v(node)`, `i(instance)`, `p(instance,outvar)`) because it reuses the operating
point core. These directives apply to the operating point results and specify
which operating point results to write when `writeop=1`.

## Output

- A file `<analysis>.*` containing the requested transfer functions, impedances,
  and admittances.
- If `writeop=1`, an additional `<analysis>.op.*raw*` file containing the operating
  point solution.

| Variable | Description |
|----------|-------------|
| `tf(source)` | Transfer function from `source` to the output. |
| `zin(source)` | Input impedance seen by `source`. |
| `yin(source)` | Input admittance seen by `source`. |

## Example

```text
// Both is OK
analysis xf1 dcxf out=["out_node"]
analysis xf1 dcxf out="out_node"

// Output is a voltage between nodes
analysis xf1 dcxf out=["outp", "outn"]
```
