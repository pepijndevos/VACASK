# Simulator Options

Simulator options control tolerances, numerical algorithms, output format, and other global settings. They are set with the `options` command in the control block and remain in effect for all subsequent analyses until changed or cleared.

## Option groups

Options are organized into the following groups. Each is documented in its own subsection.

| Group | Description |
|-------|-------------|
| [**Temperature, scale, and conductances**](cmd-options-temp.md) | `temp`, `tnom`, `scale`, `gmin`, `gshunt`, `minr` |
| [**Tolerances**](cmd-options-tol.md) | `tolmode`, `tolscale`, `reltol`, `abstol`, `vntol`, `chgtol`, `fluxtol` |
| [**Relative tolerance reference**](cmd-options-relref.md) | `relref`, `relrefsol`, `relrefres`, `relreflte` |
| [**Newton-Raphson solver**](cmd-options-nr.md) | `nr_*` options, `matrixcheck`, `rhscheck`, `solutioncheck`, `rcondcheck`, `strictforce` |
| [**Homotopy algorithms**](cmd-options-homotopy.md) | `homotopy_*` options |
| [**Operating point**](cmd-options-op.md) | `op_*` options |
| [**Small-signal analyses**](cmd-options-smsig.md) | `smsig_*` options |
| [**Transient analysis**](cmd-options-tran.md) | `tran_*` options |
| [**Harmonic balance**](cmd-options-hb.md) | `hb_*` options |
| [**Output and sweep**](cmd-options-output.md) | `rawfile`, `strictoutput`, `strictsave`, `sweep_*`, `accounting` |

## Example

```text
control
  options temp=85 reltol=1e-4 rawfile="ascii"
  analysis op1 op
endc
```
