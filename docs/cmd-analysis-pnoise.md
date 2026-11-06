# Periodic Small-Signal Noise Analysis (pnoise)

The `pnoise` analysis computes the output-referred noise power spectral density of a
circuit operating in a periodic steady state (cyclostationary noise), and the
contributions of individual noise sources to that output, as a function of offset
frequency. It also computes the power gain from a designated input source to the
output. It is the shooting-based counterpart of [hbnoise](cmd-analysis-hbnoise.md),
using the same periodic steady state as [pac](cmd-analysis-pac.md): every noise
source's contribution is modulated by the time-varying operating point before
reaching the output. Like `pac`, it is suited for circuits with a single fundamental
frequency, including strongly nonlinear and switching circuits.

## Syntax

```text
analysis name pnoise [parameters]
```

## How it works

1. VACASK finds the periodic steady state with the shooting Newton method (unless
   `psssolve=0`, in which case it evaluates at the stored solution named by `ic`
   instead). See [Periodic Steady-State Analysis](cmd-analysis-pss.md).
2. One more period is integrated. At equally spaced time points VACASK records the
   resistive and reactive Jacobians $G(t)$/$C(t)$, exactly as `pac` does, and every
   noise source's *noise modulation function* - the time-varying quantity (e.g. a
   device's instantaneous transconductance or bias current) that amplitude-modulates
   that source's noise as the large-signal waveform swings. The number of points is
   the period divided by the maximal timestep of the shooting transient, rounded up
   to a power of 2.
3. An FFT turns the Jacobians and every noise source's modulation function into
   Fourier coefficients $G_k$/$C_k$ and $M_k$, $k=0,\ldots,\text{truncharm}$. Because
   all of these are real time-domain signals, the coefficients for negative $k$ are
   the complex conjugates of those for $\lvert k \rvert$.
4. At each swept offset frequency $f$, it assembles the small-signal conversion matrix
   $H(\omega)$ exactly as `pac` does - Toeplitz in the harmonic indices, from $G_k$/$C_k$
   - then solves **one adjoint linear system** - excited at the `outharm`/`out` node
   pair, using the transposed conversion matrix - instead of one forward solve per
   noise source. By reciprocity, dotting that single adjoint solution against any
   excitation's own nodes gives the same transfer function a forward solve from that
   excitation to the output would give, so this one solve simultaneously yields the
   power-gain transfer function (dotted against the `in` source's nodes at `inharm`)
   and every noise source's transfer function (dotted against that source's own
   excitation nodes) - see [LQTV Output PSD](noise/lqtv-output-psd.md) for the
   derivation.
5. For each noise source, its per-harmonic transfer function is folded through its
   own Toeplitz modulation matrix $M$ (built from that source's $M_k$ harmonics, the
   same Toeplitz construction as $H(\omega)$ in step 4) without ever forming $M$ or
   $M M^H$ explicitly, then combined with the source's reference noise shape (white,
   or $1/f^{\alpha}$ with the exponent the flicker source declares) at every harmonic
   to give its output-referred PSD contribution at the current offset frequency.
6. Contributions are accumulated per instance and summed into the total output noise.
7. Steps 4-6 are repeated across the offset frequency sweep.

The response at harmonic $h$ is observed at the frequency $f + h/T$, where $T$ is the
PSS period.

If the sum of the offset frequency and a harmonic's frequency is zero the 1/f shape of
a flicker source is undefined. That harmonic's flicker contribution is left out at
such a point and a warning names the offset frequency. Choose a sweep grid that
does not land on pump harmonics if the flicker skirt around a harmonic matters.

The skirt of upconverted flicker noise around a pump harmonic is only as wide as
the flicker corner frequency of the sources (typically kHz to tens of kHz), so a
coarse logarithmic sweep usually steps over it. To resolve the skirts, use the
`values` parameter with a vector that has dense points close to each harmonic,
on both sides. Such a vector can be built with the expression functions
(see [Vector construction](expr-functions.md#vector-construction)) and merged
with `sort()`.

## Parameters

Sweep parameters (`from`, `to`, `step`, `mode`, `points`, `values`) follow the same
convention as in [AC Small-Signal Analysis](cmd-analysis-ac.md). The sweep variable
is the offset frequency $f$.

The following parameters are specific to `pnoise`:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `out` | string or string vector | `""` | Output node or differential node pair. A single string specifies a node to ground; a two-element vector specifies a node pair. |
| `in` | string | `""` | Instance name of the independent source used as the input reference for power gain. |
| `outharm` | integer | `0` | Output harmonic at which noise/gain is observed, a signed integer harmonic number in the range $-\text{truncharm},\ldots,\text{truncharm}$. A frequency cannot be used because the period is not known before the PSS is solved. |
| `inharm` | integer | `0` | Input harmonic at which the equivalent input excitation for the power-gain computation is placed. |
| `truncharm` | integer | `10` | Number of harmonics kept beyond DC. The Jacobian's and every modulation function's Fourier series are truncated beyond this order. Must be >=0. It must be smaller than half the number of time points of the recorded period, see [Choosing truncharm](cmd-analysis-pac.md#choosing-truncharm). |
| `write` | boolean | `1` | Write the small-signal noise results to a file. |
| `solver` | string | `""` | Linear solver for the complex conversion-matrix solve, overriding the `qpsmsigsolver` option. See [Linear Solver Selection](cmd-options-solver.md). |

`pnoise` exposes the following parameters of the
[periodic steady-state analysis](cmd-analysis-pss.md) with the same meaning: `tper`,
`oscillator`, `tstab`, `stabstep`, `icmode`, `ic`, `nodeset`, `maxharm`, `maxacfreq`,
`store`, and `writestab`. The remaining PSS parameters are renamed or have different
defaults:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `psssolve` | boolean | `1` | Solve the PSS. Set to `0` to linearize at the stored PSS solution named by `ic` without solving. `ic` must then be a string naming a stored PSS solution, and the period is taken from it. |
| `writepss` | boolean | `0` | Write the PSS waveform (one period) to `<analysis>.pss.*`. `writestab` additionally writes the stabilization transient to `<analysis>.pss.tran.*`. |
| `pssopsolver` | string | `""` | Linear solver for the operating point, stabilization transient, and shooting iterations, overriding the `tdsolver` option. It is the PSS `solver` parameter. In `pnoise`, `solver` refers to the conversion-matrix solver. |

`truncharm` bounds both what `pnoise` observes (`outharm`/`inharm`) and how finely the
Jacobian and modulation functions are resolved; the PSS's own `maxharm` (shooting
timestep resolution, see above) must be at least `truncharm`, exactly as in `pac`.

### Oscillators

`pnoise` does not compute oscillator phase noise. Any nonzero `oscillator` value makes
the PSS solve for the period, but the noise computation still uses the algorithm for
driven circuits. This plain linearization rises as $1/\Delta f^2$ and diverges very close
to the carrier, where the true spectrum does not, so `oscillator=1` produces a warning.

If you still want this analysis, set `oscillator` to a nonzero value other than 1, for
example `oscillator=2`. The analysis then runs the same way, but without the warning.

## Save directives

The following noise save directives are supported:

| Directive | Description |
|-----------|-------------|
| `default` | Save total noise contribution `n(instance)` for all noisy instances (default behavior). |
| `full` | Save total `n(instance)` and per-source `n(instance,contrib)` for all noisy instances and all their noise sources. |
| `n(instance)` | Save the total output-referred noise contribution of the given instance. |
| `n(instance,contrib)` | Save the output-referred contribution of a specific noise source `contrib` within `instance`. |
| `nc(instance)` | Save the total output-referred noise contribution of `instance`, plus one descriptor per individual noise source it has (each written as `n(instance,contrib)` in the output). |

The following PSS save directives are also supported. They apply to the PSS waveform
and are written to `<analysis>.pss.*` when `writepss=1` (and to
`<analysis>.pss.tran.*` for the stabilization transient when `writestab=1`).

| Directive | Description |
|-----------|-------------|
| `pssdefault` | Save all PSS node values and branch flows. |
| `pssfull` | Save all PSS unknowns (even those belonging to collapsed nodes). |
| `v(node)` | Save the PSS waveform at the given node. |
| `i(instance)` | Save the PSS branch flow through the given instance. Only instances that introduce a current variable in the MNA system are valid (e.g. voltage sources, inductors). |
| `p(instance,outvar)` | Save the output variable `outvar` of the given instance along the PSS waveform. |

## Output

- A file `<analysis>.*` containing the noise results at each offset frequency point.
- If `writepss=1`, an additional `<analysis>.pss.*` file containing one period of the PSS waveform.
- If `writestab=1`, an additional `<analysis>.pss.tran.*` file containing the stabilization transient.

| Variable | Description |
|----------|-------------|
| `frequency` | Offset frequency sweep variable (Hz). |
| `onoise` | Total output-referred noise power spectral density at `outharm`. |
| `gain` | Power gain from the `in` source at `inharm` to the output at `outharm` (dimensionless). |
| `n(instance)` | Total output-referred noise PSD contributed by `instance`. |
| `n(instance,contrib)` | Output-referred noise PSD of the specific noise source `contrib` within `instance`. |

The `onoise`/`gain`/`n(...)` are reported at the offset frequency. `onoise` and every
`n(...)` are one-sided PSDs (frequency $\ge 0$ only), the same convention
[Small-Signal Noise Analysis](cmd-analysis-noise.md) uses - a device biased at a
constant operating point gives the same `n(instance,contrib)` from either analysis.

## Examples

**Basic pnoise analysis, output and input both at DC (outharm/inharm default):**

```text
v1 (in 0) vsource dc=0.8 type="sine" ampl=0.2 freq=1k
r1 (in out) resistor r=1k noisy=1
d1 (out 0) d is=1e-12 n=2 kf=1e-15 af=1.2
c1 (out 0) capacitor c=1u

control
  save full
  analysis pnoise1 pnoise tper=1m tstab=20m truncharm=8 maxharm=8 in="v1" out="out" from=1 to=10k mode="dec" points=10
endc
```

**Observe upconverted noise around the first pump harmonic:**

```text
// Output at the harmonic one above the pump, input equivalent noise still referred to DC
analysis pnoise1 pnoise tper=1m tstab=20m truncharm=8 maxharm=8 in="v1" out="out" outharm=1 inharm=0 from=1 to=10k mode="dec" points=10
```

**Reuse a stored PSS solution:**

```text
analysis pss1 pss tper=1m tstab=20m store="psssol"
// Linearize at the stored solution, the period is taken from it
analysis pnoise1 pnoise ic="psssol" psssolve=0 truncharm=8 in="v1" out="out" from=1 to=10k mode="dec" points=10
```

## Options

- [Periodic Steady-State Options](cmd-options-pss.md)
- [Small-Signal Analysis Options](cmd-options-smsig.md) (`smsig_debug`)
- [Transient Analysis Options](cmd-options-tran.md)
- [Newton-Raphson Solver](cmd-options-nr.md)
- [Linear Solver Selection](cmd-options-solver.md)
