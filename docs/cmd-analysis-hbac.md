# (Quasi)Periodic Small-Signal Analysis (hbac)

The `hbac` analysis computes the small-signal frequency-domain response of a circuit
operating in a (quasi)periodic steady state. It first solves the harmonic balance (HB)
operating point, then sweeps a small-signal input frequency and solves the linearized
conversion matrix at each frequency point. This is analogous to AC analysis but around
a (quasi)periodic operating point instead of a DC one.

## Syntax

```text
analysis name hbac [parameters]
```

## How it works

1. VACASK solves the (quasi)periodic steady-state operating point using harmonic balance.
   See [Harmonic Balance Analysis](cmd-analysis-hb.md) for details.
2. It assembles the frequency-domain (spectral) Jacobians $G_k$ and $C_k$ — the Fourier
   coefficients of the resistive and reactive Jacobians evaluated at the HB operating point.
3. At each small-signal input frequency $f$ it forms the conversion matrix

   $$H_{nm}(f) = G_k + j\,\omega_n\,C_k, \qquad \omega_n = 2\pi(f + f_n)$$

   where $f_n$ is the $n$-th spur frequency of the HB spectrum and $k$ is the Jacobian
   harmonic index coupling output row $n$ to input column $m$ (determined by the mixing
   stencil).
4. It solves $H(f)\,X = U$, where $U$ is assembled from the `spur`, `smag`, and `sphase`
   parameters of independent sources.
5. The selected output-spur rows of $X$ are written to the output file.
6. Steps 3-5 are repeated across the frequency sweep.

## Small-signal excitation

Each independent source can inject small-signal excitation at one or more spur frequencies
via the following instance parameters:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `spur` | value list | `[]` | List of excitation spurs. Each entry is either a real spur frequency (Hz) or an integer vector of tone weights $[k_1, k_2, \ldots]$. |
| `smag` | real vector | `[]` | Excitation magnitudes, one per entry in `spur`. Missing entries default to `0`. |
| `sphase` | real vector | `[]` | Excitation phases in degrees, one per entry in `spur`. Missing entries default to `0`. |

Sources with no `spur` entries contribute no small-signal excitation.

## Parameters

`hbac` exposes all HB parameters and adds sweep and output control parameters.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `freq` | real vector | - | Fundamental frequencies (Hz). Required. |
| `nharm` | integer or integer vector | `4` | Number of harmonics per fundamental. |
| `immax` | integer | `0` | Maximum intermodulation order. If <= 0, defaults to the largest component of `nharm`. |
| `truncate` | string | `"hybrid"` | Spectrum truncation: `"hybrid"`, `"diamond"`, `"box"`, or `"raw"`. |
| `samplefac` | real | `5` | Oversampling factor for colocation timepoints. |
| `nper` | real | `1` | Number of lowest-frequency periods spanned by colocation timepoints. |
| `sample` | string | `"uniform"` | Colocation sampling mode: `"uniform"`, `"random"`, or `"mixed"`. |
| `shift` | real | `0.2` | Fractional shift between consecutive colocation points. |
| `nodeset` | string | `""` | Name of a saved HB solution to use as the initial guess. |
| `store` | string | `""` | Save the computed HB solution under the given name. |
| `opsolve` | boolean | `1` | Solve the HB operating point. Set to `0` to linearize at the nodeset without solving. |
| `outspur` | real, integer vector, or list | `0.0` | Output spur(s) to observe. Each entry is a real frequency (Hz) or integer tone-weight vector. Default `0.0` selects the DC spur. |
| `from` | real | `0` | Start frequency (Hz). |
| `to` | real | `0` | Stop frequency (Hz). |
| `step` | real | `0` | Frequency step size (Hz) for a stepped linear sweep. |
| `mode` | string | - | Sweep mode: `"lin"`, `"dec"`, or `"oct"`. |
| `points` | integer | `0` | Number of points (total for `"lin"`, per decade for `"dec"`, per octave for `"oct"`). |
| `values` | real vector | - | Explicit list of frequencies (Hz). Overrides `from`/`to`/`step`/`mode`/`points`. |
| `write` | boolean | `1` | Write the small-signal results to a file. |
| `writehb` | boolean | `0` | Also write the HB operating point results to `<analysis>.hb.*`. |

See [AC Small-Signal Analysis](cmd-analysis-ac.md) for a description of sweep modes.

## Save directives

The following small-signal save directives are supported:

| Directive | Description |
|-----------|-------------|
| `default` | Save all small-signal node phasors and branch flows (default behavior). |
| `full` | Save all small-signal unknowns (even those belonging to collapsed nodes). |
| `dv(node)` | Save the small-signal phasor at the given node. |
| `di(instance)` | Save the small-signal current phasor through the given instance. Only instances that introduce a current variable in the MNA system are valid (e.g. voltage sources, inductors). |

The following HB operating point save directives are also supported. They apply to the HB
operating point results and are written to `<analysis>.hb.*` when `writehb=1`.

| Directive | Description |
|-----------|-------------|
| `hbdefault` | Save all HB node phasors and branch flows. |
| `hbfull` | Save all HB unknowns (even those belonging to collapsed nodes). |
| `v(node)` | Save the HB phasor at the given node. |
| `i(instance)` | Save the HB branch flow phasor through the given instance. Only instances that introduce a current variable in the MNA system are valid (e.g. voltage sources, inductors). |
| `p(instance,outvar)` | Save the HB output variable `outvar` of the given instance. |

## Output

- A file `<analysis>.*` containing the small-signal results at each frequency point.
- If `writehb=1`, an additional `<analysis>.hb.*` file containing the HB operating point.

| Variable | Description |
|----------|-------------|
| `frequency` | Small-signal input frequency sweep variable (Hz). Always present. |
| `node;k1,k2,...` | Complex small-signal phasor at `node` for output spur with tone weights $[k_1, k_2, \ldots]$. One variable per saved node per output spur. |
| `instance:flow(br);k1,k2,...` | Complex small-signal branch flow phasor for the output spur. |

The actual frequency at which the response is observed for sweep frequency $f$ and output
spur with weights $[k_1, k_2, \ldots]$ is $f + k_1 f_1 + k_2 f_2 + \cdots$.

## Examples

**Single-tone, observe at fundamental output spur:**

```text
v1 (in 0) vsource dc=0 spur={[1]} smag=[1.0]
analysis hbac1 hbac freq=[1k] nharm=8 outspur=[1] from=1 to=10k mode="dec" points=10
```

**Multi-tone, observe down-converted output at DC spur:**

```text
// LO at index [1,0], RF at index [0,1]
vlo (lo 0) vsource dc=0 spur={[1,0]} smag=[1.0]
vrf (rf 0) vsource dc=0 spur={[0,1]} smag=[1.0]
analysis hbac1 hbac freq=[1G, 900M] nharm=3 immax=3 outspur=[0,0] from=1 to=100M mode="dec" points=10
```

**Save specific nodes only:**

```text
save dv(out)
analysis hbac1 hbac freq=[1k] nharm=8 outspur=[1] from=1 to=100k mode="dec" points=20
```

## Options

- [Harmonic Balance Options](cmd-options-hb.md)
- [Newton-Raphson Solver](cmd-options-nr.md)
- [Homotopy Algorithms](cmd-options-homotopy.md)
