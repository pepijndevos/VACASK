# Linear Solver Selection

These options choose the sparse linear solver used to factor and solve the
circuit Jacobian inside each analysis. VACASK always provides `klu` (the KLU
sparse solver). Builds compiled with the SuperLU_MT backend also provide
`superlu`, a multithreaded solver whose thread count is set with the
[`--ncpu` command line option](startup-options.md#parallelism).

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `tdsolver` | string | `""` | `klu`, `superlu`, `""` | Solver for the real Jacobian: operating point, `dcinc`, `dcxf`, transient, and the shooting loop of periodic steady-state analysis. The frequency-domain small-signal analyses also use it for the operating-point solve they run first. |
| `smsigsolver` | string | `""` | `klu`, `superlu`, `""` | Solver for the complex Jacobian of the frequency-domain small-signal analyses: `ac`, `acxf`, `acstb`, `acsp`, and `noise`. |
| `hbsolver` | string | `""` | `klu`, `superlu`, `""` | Solver for the real harmonic balance Jacobian, in both `hb` and the large-signal solve of `hbac`. |
| `qpsmsigsolver` | string | `""` | `klu`, `superlu`, `""` | Solver for the complex conversion-matrix Jacobian of the harmonic-balance-based (quasi)periodic small-signal analysis (`hbac`). |

An empty string selects the built-in default: `klu` for `tdsolver` and
`smsigsolver`, and `superlu` for `hbsolver` and `qpsmsigsolver` when the
SuperLU_MT backend is present (`klu` otherwise). Harmonic balance defaults to
`superlu` because its Jacobian has dense spectral coupling that KLU handles
poorly.

Each analysis also accepts a `solver` parameter that overrides the matching
option for that one analysis. The frequency-domain small-signal analyses
additionally accept `opsolver`, which overrides `tdsolver` for their
operating-point solve.

## Example

```text
control
  options hbsolver="klu"
  analysis hb1 hb freq=[1G] nharm=7
  analysis hb2 hb freq=[1G] nharm=7 solver="superlu"
endc
```
