# (Quasi)Periodic Small-Signal Noise Analysis (hbnoise)

The `hbnoise` analysis computes the output-referred noise power spectral density of a
circuit operating in a (quasi)periodic steady state (cyclostationary noise), and the
contributions of individual noise sources to that output, as a function of offset
frequency. It also computes the power gain from a designated input source to the
output. This is the harmonic-balance counterpart of [Small-Signal Noise
Analysis](cmd-analysis-noise.md) - where plain `noise` linearizes around a single DC
operating point, `hbnoise` linearizes around a full periodic (or quasi-periodic,
multi-tone) large-signal harmonic-balance (HB) solution, so every noise source's
contribution is modulated by the time-varying operating point before reaching the
output.

## Syntax

```text
analysis name hbnoise [parameters]
```

## How it works

1. VACASK solves the (quasi)periodic steady-state operating point using harmonic
   balance (unless `hbsolve=0`, in which case it evaluates at the given `nodeset`
   instead). See [Harmonic Balance Analysis](cmd-analysis-hb.md).
2. It evaluates the circuit's time-domain resistive/reactive Jacobians and every
   noise source's *noise modulation function* - the time-varying quantity (e.g. a
   device's instantaneous transconductance or bias current) that amplitude-modulates
   that source's noise as the large-signal waveform swings - at the HB collocation
   points, then Fourier-transforms both into frequency-domain harmonics: $G_k$/$C_k$
   for the Jacobians, $M_k$ per noise source for its modulation function.
3. At each swept offset frequency $f$, it assembles the small-signal conversion
   matrix $H(\omega)$ exactly as [`hbac`](cmd-analysis-hbac.md) does, then solves
   **one adjoint linear system** - excited at the `outspur`/`out` node pair, using
   the transposed conversion matrix - instead of one forward solve per noise source.
   By reciprocity, dotting that single adjoint solution against any excitation's own
   nodes gives the same transfer function a forward solve from that excitation to the
   output would give, so this one solve simultaneously yields the power-gain transfer
   function (dotted against the `in` source's nodes at `inspur`) and every noise
   source's transfer function (dotted against that source's own excitation nodes) -
   see [LQTV Output PSD](noise/lqtv-output-psd.md) for the derivation.
4. For each noise source, its per-spur transfer function is folded through its own
   Toeplitz modulation matrix $M$ (built from that source's $M_k$ harmonics) without
   ever forming $M$ or $M M^H$ explicitly, then combined with the source's reference
   noise shape (white or flicker) at every spur to give its output-referred PSD
   contribution at the current offset frequency.
5. Contributions are accumulated per instance and summed into the total output noise.
6. Steps 3-5 are repeated across the offset frequency sweep.

## Parameters

`hbnoise` exposes all HB parameters (with the same defaults and meaning as in
[Harmonic Balance Analysis](cmd-analysis-hb.md)) plus its own output/spur/frequency
parameters. Sweep parameters (`from`/`to`/`step`/`mode`/`points`/`values`) follow the
same convention as [AC Small-Signal Analysis](cmd-analysis-ac.md).

The following parameters are inherited from [Harmonic Balance Analysis](cmd-analysis-hb.md)
and have the same meaning: `freq`, `nharm`, `immax`, `truncate`, `samplefac`,
`tstart`, `nper`, `sample`, `shift`, `nodeset`, `store`. When `hbsolve=1`, `freq` is
required; the remaining inherited parameters have defaults.

The following parameters are specific to `hbnoise`:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `out` | string or string vector | `""` | Output node or differential node pair. A single string specifies a node to ground; a two-element vector specifies a node pair. |
| `in` | string | `""` | Instance name of the independent source used as the input reference for power gain. |
| `outspur` | real or integer vector | `0.0` (DC) | Output spur at which noise/gain is observed: a scalar frequency (Hz) or an integer tone-weight vector. Unlike `hbac`, exactly one spur is selected (no list). |
| `inspur` | real or integer vector | `0.0` (DC) | Input spur at which the equivalent input excitation for the power-gain computation is placed. |
| `maxharm` | integer or integer vector | `-1` | Truncate the small-signal spectrum to spurs whose tone weights satisfy $\lvert k_j \rvert \le \text{maxharm}_j$ for all $j$. Scalar applies to all tones. Negative: no truncation. |
| `maxfreq` | real | `-1` | Truncate the small-signal spectrum to spurs whose absolute frequency does not exceed `maxfreq` (Hz). Negative: no truncation. |
| `hbsolve` | boolean | `1` | Solve the HB operating point. Set to `0` to linearize at the `nodeset` without solving. |
| `from` | real | `0` | Start offset frequency (Hz). |
| `to` | real | `0` | Stop offset frequency (Hz). |
| `step` | real | `0` | Offset frequency step size (Hz) for a stepped linear sweep. |
| `mode` | string | - | Sweep mode: `"lin"`, `"dec"`, or `"oct"`. |
| `points` | integer | `0` | Number of intervals for `"lin"` (yields `points+1` frequencies), per decade for `"dec"`, per octave for `"oct"`. |
| `values` | real vector | - | Explicit vector of offset frequencies (Hz). Overrides `from`/`to`/`step`/`mode`/`points`. |
| `write` | boolean | `1` | Write the small-signal noise results to a file. |
| `writehb` | boolean | `0` | Also write the HB operating point results to `<analysis>.hb.*`. |
| `solver` | string | `""` | Linear solver for the complex conversion-matrix solve, overriding the `qpsmsigsolver` option. See [Linear Solver Selection](cmd-options-solver.md). |
| `hbsolver` | string | `""` | Linear solver for the large-signal HB solve, overriding the `hbsolver` option. |

## Save directives

| Directive | Description |
|-----------|-------------|
| `default` | Save total noise contribution `n(instance)` for all noisy instances (default behavior). |
| `full` | Save total `n(instance)` and per-source `n(instance,contrib)` for all noisy instances and all their noise sources. |
| `n(instance)` | Save the total output-referred noise contribution of the given instance. |
| `n(instance,contrib)` | Save the output-referred contribution of a specific noise source `contrib` within `instance`. |
| `nc(instance)` | Save the total output-referred noise contribution of `instance`, plus one descriptor per individual noise source it has (each written as `n(instance,contrib)` in the output). |

The following HB operating point save directives are also supported. They apply to
the HB operating point results and are written to `<analysis>.hb.*` when `writehb=1`.

| Directive | Description |
|-----------|-------------|
| `hbdefault` | Save all HB node phasors and branch flows. |
| `hbfull` | Save all HB unknowns (even those belonging to collapsed nodes). |
| `v(node)` | Save the HB phasor at the given node. |
| `i(instance)` | Save the HB branch flow phasor through the given instance. Only instances that introduce a current variable in the MNA system are valid (e.g. voltage sources, inductors). |

## Output

- A file `<analysis>.*` containing the noise results at each offset frequency point.
- If `writehb=1`, an additional `<analysis>.hb.*` file containing the HB operating point.

| Variable | Description |
|----------|-------------|
| `frequency` | Offset frequency sweep variable (Hz). |
| `onoise` | Total output-referred noise power spectral density at `outspur`. |
| `gain` | Power gain from the `in` source at `inspur` to the output at `outspur` (dimensionless). |
| `n(instance)` | Total output-referred noise PSD contributed by `instance`. |
| `n(instance,contrib)` | Output-referred noise PSD of the specific noise source `contrib` within `instance`. |

The `onoise`/`gain`/`n(...)` are reported at the offset frequency. `onoise` and every
`n(...)` are one-sided PSDs (frequency $\ge 0$ only), the same convention
[Small-Signal Noise Analysis](cmd-analysis-noise.md) uses - a device biased at a
constant operating point gives the same `n(instance,contrib)` from either analysis.

## Examples

**Basic hbnoise analysis, output and input both at DC (outspur/inspur default):**

```text
v1 (in 0) vsource dc=0.8 type="sine" ampl=0.2 freq=1k spur={0} smag=[1]
r1 (in out) resistor r=1k noisy=1
d1 (out 0) d is=1e-12 n=2 kf=1e-15 af=1.2
c1 (out 0) capacitor c=1u

control
  save full
  analysis hbnoise1 hbnoise freq=[1k] nharm=8 in="v1" out="out" from=1 to=10k mode="dec" points=10
endc
```

**Observe upconverted noise around the first pump harmonic:**

```text
// Output spur one harmonic above the pump, input equivalent noise still referred to DC
analysis hbnoise1 hbnoise freq=[1k] nharm=8 in="v1" out="out" outspur=[1] inspur=[0] from=1 to=10k mode="dec" points=10
```

**Reuse a stored HB operating point:**

```text
analysis hb1 hb freq=[1k] nharm=8 store="op1"
analysis hbnoise1 hbnoise nodeset="op1" hbsolve=0 in="v1" out="out" from=1 to=10k mode="dec" points=10
```

## Options

- [Harmonic Balance Options](cmd-options-hb.md)
- [Newton-Raphson Solver](cmd-options-nr.md)
- [Linear Solver Selection](cmd-options-solver.md)
- [Homotopy Algorithms](cmd-options-homotopy.md)
