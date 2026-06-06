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
3. At each small-signal input frequency $f$ it forms the conversion matrix. Each
   subblock $(n,m)$ couples unknown $m$ to circuit equation $n$ across all spurs:

   $$H^{(nm)}_{kl}(f) = [G_{p(k,l)}]_{nm} + j\,(\omega_k + \omega)\,[C_{p(k,l)}]_{nm}, \qquad \omega_k = 2\pi f_k, \qquad \omega = 2 \pi f$$

   where $n$, $m$ are node indices, $k$, $l$ are spur (harmonic) indices,
   $f_k$ is the $k$-th spur frequency of the HB spectrum, and $G_{p(k,l)}$, $C_{p(k,l)}$
   are the Fourier coefficients of the resistive and reactive Jacobians. $p(k,l)$ is the index of the Jacobian Fourier coefficient that converts input frequency $f_l$ to output frequency $f_k$. If no such index exists $G_{p(k,l)}$ and $C_{p(k,l)}$ are considered to be 0. $f$ is the excitation's offset frequency. All small signal excitations share the same offset frequency. 
   
4. It solves $H(f)\,X = U$, where $U$ is assembled from the `spur`, `smag`, and `sphase`
   parameters of independent sources.
5. The selected output-spur rows of $X$ are written to the output file.
6. Steps 3-5 are repeated across the offset frequency sweep.

## Small-signal excitation

Each independent source injects excitation at the spurs listed in its `spur` parameter,
with magnitudes and phases given by `smag` and `sphase`. Sources with no `spur` entries
contribute no excitation. See [(Quasi)Periodic Small-Signal Excitation](dev-builtin-src.md#quasiperiodic-small-signal-excitation)
in the Independent Sources reference for the parameter details.

The `mag` and `phase` parameters of independent sources are used by AC and DC incremental
analyses and have no effect in `hbac`.

## Parameters

`hbac` exposes all HB parameters and adds output control parameters. Sweep parameters
(`from`, `to`, `step`, `mode`, `points`, `values`) follow the same convention as in
[AC Small-Signal Analysis](cmd-analysis-ac.md).

The following parameters are inherited from [Harmonic Balance Analysis](cmd-analysis-hb.md)
and have the same meaning: `freq`, `nharm`, `immax`, `truncate`,
`samplefac`, `tstart`, `nper`, `sample`, `shift`, `nodeset`, `store`.
When `hbsolve=1`, `freq` is required; the remaining inherited parameters have defaults.

The following parameters are specific to `hbac`:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `hbsolve` | boolean | `1` | Solve the HB operating point. Set to `0` to linearize at the nodeset without solving. |
| `outspur` | real, integer vector, or list | `{}` | Output spur(s) to observe. Each list entry is a real frequency (Hz) or an integer tone-weight vector. Default `{}` selects all spurs. |
| `maxharm` | integer or integer vector | `-1` | Truncate the conversion matrix to spurs whose tone weights satisfy $\lvert k_j \rvert \le \text{maxharm}_j$ for all $j$. Scalar applies to all tones. Negative: no truncation (use all spurs from the HB spectrum). |
| `maxfreq` | real | `-1` | Truncate the conversion matrix to spurs whose absolute frequency does not exceed `maxfreq` (Hz). Negative: no truncation. |
| `write` | boolean | `1` | Write the small-signal results to a file. |
| `writehb` | boolean | `0` | Also write the HB operating point results to `<analysis>.hb.*`. |

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

- A file `<analysis>.*` containing the small-signal results at each offset frequency point.
- If `writehb=1`, an additional `<analysis>.hb.*` file containing the HB operating point.

| Variable | Description |
|----------|-------------|
| `frequency` | Small-signal offset frequency sweep variable (Hz). Always present. |
| `node;k1,k2,...` | Complex small-signal phasor at `node` for output spur with tone weights $[k_1, k_2, \ldots]$. One variable per saved node per output spur. |
| `instance:flow(br);k1,k2,...` | Complex small-signal branch flow phasor for the output spur. |

The actual frequency at which the response is observed for offset frequency $f$ and output
spur with weights $[k_1, k_2, \ldots]$ is $f + k_1 f_1 + k_2 f_2 + \cdots$.

## Examples

**Single-tone, observe at fundamental output spur:**

```text
// Excitation at spur index 1
v1 (in 0) vsource dc=0 spur={[1]} smag=[1.0]
// Output spur with integer index 1
analysis hbac1 hbac freq=[1k] nharm=8 outspur=[1] from=1 to=10k mode="dec" points=10
```

**Multi-tone, observe down-converted output at DC spur:**

```text
// LO at index [1,0], RF at index [0,1]
vlo (lo 0) vsource dc=0 spur={[1,0]} smag=[1.0]
vrf (rf 0) vsource dc=0 spur={[0,1]} smag=[1.0]
// Output spur at 0GHz
analysis hbac1 hbac freq=[1G, 900M] nharm=3 immax=3 outspur=0G from=1 to=100M mode="dec" points=10
```

**Reuse a stored HB operating point:**

```text
v1 (in 0) vsource dc=0 spur={[1]} smag=[1.0]
analysis hb1 hb freq=[1k] nharm=8 store="op1"
// Output spurs with indices 1 and 2
analysis hbac1 hbac outspur={[1],[2]} from=1 to=10k mode="dec" points=10 hbsolve=0 nodeset="op1"
```

**Save specific nodes only:**

```text
save dv(out)
// All output spurs
analysis hbac1 hbac freq=[1k] nharm=8 outspur={} from=1 to=100k mode="dec" points=20
```

## Options

- [Harmonic Balance Options](cmd-options-hb.md)
- [Newton-Raphson Solver](cmd-options-nr.md)
- [Homotopy Algorithms](cmd-options-homotopy.md)
