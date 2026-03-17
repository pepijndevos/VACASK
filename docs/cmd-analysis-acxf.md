# AC Small-Signal Transfer Function Analysis (acxf)

The AC small-signal transfer function analysis (`acxf`) computes the complex
transfer functions from all independent sources to a selected output over a
frequency sweep. It also computes the input impedance and admittance at the
terminals of each independent source.

## Syntax

```text
analysis name acxf [parameters]
```

## How it works

1. VACASK solves the operating point $x_0$.
2. It linearizes the circuit by computing $J_r$ and $J_c$ at $x_0$.
3. For each frequency $f$ and each independent source it solves

   $$(J_r + j\omega J_c)\, X = U$$

   with $U$ set to a unity excitation at that source's terminals and
   $\omega = 2\pi f$.
4. From $X$ it computes:
   - Transfer function $H = V_\text{out}/V_\text{excitation}$ to the designated output
   - Input impedance $Z_\text{in}$ and input admittance $Y_\text{in}$ at the source terminals
5. Steps 3–4 are repeated across the frequency sweep.

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `out` | string or string vector | `""` | Output node or node pair. A single string specifies a node to ground; a two-element vector specifies a node pair. |
| `nodeset` | string or list | `""` | Initial guess for the operating point. See [Operating Point Analysis](cmd-analysis-op.md) for syntax. |
| `store` | string | `""` | Save the computed operating point under the given name. See [Operating Point Analysis](cmd-analysis-op.md). |
| `from` | real | `0` | Start frequency (Hz). |
| `to` | real | `0` | Stop frequency (Hz). |
| `step` | real | `0` | Frequency step size (Hz) for a stepped linear sweep. |
| `mode` | string | — | Sweep mode: `"lin"`, `"dec"`, or `"oct"`. |
| `points` | integer | `0` | Number of points (total for `"lin"`, per decade for `"dec"`, per octave for `"oct"`). |
| `values` | real vector | — | Explicit list of frequencies (Hz). Overrides `from`/`to`/`step`/`mode`/`points`. |
| `write` | boolean | `1` | Write the analysis results to a file. |
| `writeop` | boolean | `0` | Also write the operating point results to `<analysis>.op.*`. |

See [AC Small-Signal Analysis](cmd-analysis-ac.md) for a description of sweep modes.

## Save directives

| Directive | Description |
|-----------|-------------|
| `default` | Save all transfer functions and input impedances (default behavior). |
| `tf(source)` | Save the transfer function from the given independent source to the output. |
| `zin(source)` | Save the input impedance seen by the given source. |
| `yin(source)` | Save the input admittance seen by the given source. |

In addition, `acxf` supports all operating point save directives (`v(node)`,
`i(instance)`, `p(instance,outvar)`) because it runs an operating point core
internally. These directives apply to the operating point results and specify
which operating point results to write when `writeop=1`.

## Output

- A file `<analysis>.*` containing the requested transfer functions, impedances,
  and admittances at each frequency point.
- If `writeop=1`, an additional `<analysis>.op.*` file containing the operating
  point solution.

| Variable | Description |
|----------|-------------|
| `frequency` | Frequency sweep variable (Hz). Always present. |
| `tf(source)` | Complex transfer function from `source` to the output. |
| `zin(source)` | Complex input impedance seen by `source`. |
| `yin(source)` | Complex input admittance seen by `source`. |

## Examples

**Transfer function to a single output node:**

```text
analysis xf1 acxf out="out" from=1 to=100k mode="dec" points=10
```

**Differential output:**

```text
analysis xf1 acxf out=["outp", "outn"] from=1 to=100k mode="dec" points=10
```

**Save specific source only:**

```text
save tf(v1)
save zin(v1)
analysis xf1 acxf out="out" from=1 to=100k mode="dec" points=10
```
