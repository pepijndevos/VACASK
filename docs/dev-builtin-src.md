# Independent Sources

Module names: `vsource`, `isource`

`vsource` and `isource` are independent voltage and current sources. They share the same parameter set. The `type` parameter selects the transient waveform. Small-signal excitation is set independently of the transient type.

## Terminals

Both devices have two terminals that must be connected: `p n`.

| Terminal | Role |
|----------|------|
| `p` | Positive terminal |
| `n` | Negative terminal |

`vsource` enforces $V(p) - V(n) =$ waveform value. It introduces one internal unknown (the branch current) and has a `flow(br)` internal node that current-controlled sources can reference.

`isource` drives a current equal to the waveform value from `p` to `n` through the device (i.e., the current exits into the circuit at terminal `n`).

## Common parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `type` | string | `"dc"` | Waveform type. One of `"dc"`, `"sine"`, `"pulse"`, `"exp"`, `"pwl"`, `"am"`, `"fm"`. |
| `delay` | real | 0 | Start time (s). Before this time the source holds the value it would have at t = `delay`. |
| `$mfactor` | real | 1 | Number of parallel instances. For `vsource` the voltage value is unaffected; for `isource` the total current scales with `$mfactor`. |

## Small-Signal Excitation

These parameters set the small-signal excitation for [DC incremental](cmd-analysis-dcinc.md) and
[AC](cmd-analysis-ac.md) analyses, independently of the transient waveform type.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `mag` | real | 0 | Excitation amplitude. For `vsource`: voltage in V. For `isource`: current per instance in A. |
| `phase` | real | 0 | Excitation phase in degrees. Used only in AC analysis; ignored by DC incremental. |

## (Quasi)Periodic Small-Signal Excitation

These parameters define the small-signal excitation injected by the source in
[(quasi)periodic small-signal (hbac) analysis](cmd-analysis-hbac.md). They have no
effect in AC or transient analysis.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `spur` | value list | `{}` | List of spurs at which the source injects excitation. Each entry is a real frequency (Hz) or an integer tone-weight vector $[k_1, k_2, \ldots]$. An empty list means no excitation. |
| `smag` | real vector | `[]` | Excitation magnitudes, one per `spur` entry. If shorter than `spur`, missing entries default to 0. Extra entries are ignored. |
| `sphase` | real vector | `[]` | Excitation phases in degrees, one per `spur` entry. If shorter than `spur`, missing entries default to 0. Extra entries are ignored. |

For a single-tone circuit with one fundamental $f_1$, the entry `[1]` selects the first harmonic
(i.e., $f_1$). For a two-tone circuit with fundamentals $f_1$ and $f_2$, the entry `[0,1]` selects
the spur at $f_2$, and `[1,0]` selects $f_1$.

## Output variables

| Variable | Description |
|----------|-------------|
| `v` | Terminal voltage $V(p) - V(n)$ |
| `i` | Current through one parallel instance. For `vsource`: positive when flowing into terminal `p` (passive sign convention). For `isource`: equals the instantaneous waveform value, positive when flowing from `p` to `n` through the device. |

## Waveform types

### DC

```text
type="dc"  dc=value
```

Constant value. Used for bias sources.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `dc` | real | 0 | Constant output value (V or A). |

---

### Sine

```text
type="sine"  sinedc=0 ampl=1 freq=1k phase=0 delay=0 theta=0
```

Damped sinusoid. Before `delay` the source holds the value it would produce at t = `delay`.

$$v(t) = \text{sinedc} + \text{ampl} \cdot \sin\!\left(2\pi \cdot \text{freq} \cdot (t - \text{delay}) + \frac{\text{phase} \cdot \pi}{180}\right) \cdot \exp\!\left(-\text{theta} \cdot (t - \text{delay})\right)$$

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `sinedc` | real | 0 | DC offset added to the sinusoid. |
| `ampl` | real | 1 | Amplitude. |
| `freq` | real | 1k | Frequency in Hz. Must be greater than 0. |
| `tdphase` | real | 0 | Initial phase in degrees. |
| `theta` | real | 0 | Damping coefficient (1/s). Zero for an undamped sinusoid. |

---

### Pulse

```text
type="pulse"  val0=0 val1=1 delay=0 rise=1n fall=0 width=0 period=0
```

Piecewise-linear pulse waveform. The waveform starts at `val0`, rises linearly to `val1`, optionally holds there, then falls back to `val0`.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `val0` | real | 0 | Base value. |
| `val1` | real | 1 | Pulse value. |
| `rise` | real | 1n | Rise time (s). Must be greater than 0. |
| `fall` | real | 0 | Fall time (s). Zero or negative: no fall - stays at `val1`. |
| `width` | real | 0 | Flat-top duration (s) between end of rise and start of fall. Zero: no flat top. |
| `period` | real | 0 | Repetition period (s). Zero or negative: single pulse. Must be greater than `rise + fall + width` when positive. |

---

### Exp

```text
type="exp"  val0=0 val1=1 delay=0 td2=... tau1=... tau2=...
```

Double-exponential: a rising exponential followed by a falling one.

For $t < \text{delay}$:

$$v(t) = \text{val0}$$

For $\text{delay} \le t < \text{delay} + \text{td2}$:

$$v(t) = \text{val0} + (\text{val1} - \text{val0})\left(1 - e^{-(t-\text{delay})/\text{tau1}}\right)$$

For $t \ge \text{delay} + \text{td2}$:

$$v(t) = \text{val0} + (\text{val1} - \text{val0})\left(1 - e^{-(t-\text{delay})/\text{tau1}}\right) + (\text{val0} - \text{val1})\left(1 - e^{-(t-\text{delay}-\text{td2})/\text{tau2}}\right)$$

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `val0` | real | 0 | Initial and final value. |
| `val1` | real | 1 | Peak value at the turning point. |
| `td2` | real | 0 | Duration of the rising phase (s), measured from `delay`. Must be greater than 0. |
| `tau1` | real | 0 | Rise time constant (s). |
| `tau2` | real | 0 | Fall time constant (s). |

---

### PWL

```text
type="pwl"  wave=[x1,y1, x2,y2, ...] offset=0 scale=1 stretch=1 delay=0 period=0 tdphase=0 break="auto"
```

Piecewise-linear waveform defined as a list of $(x_i, y_i)$ point pairs. The waveform value is the linear interpolation between adjacent points, with optional periodic repetition.

Let $\tau = (t - \text{delay}) / \text{stretch}$ be the position along the (unstretched) waveform timeline. For an aperiodic waveform ($\text{period} \le 0$):

$$v(t) = \text{scale} \cdot \text{interp}(\tau) + \text{offset}$$

where $\text{interp}(\tau)$ linearly interpolates between consecutive $(x_i, y_i)$ pairs. For $\tau < x_1$ the value is held at $y_1$; for $\tau \ge x_N$ the value is held at $y_N$.

For a periodic waveform ($\text{period} > 0$), $\tau$ is wrapped into $[0, \text{period})$ before interpolation, after shifting by the initial phase $\text{tdphase}/360 \cdot \text{period}$.

Constraints on `wave`:

- Must contain an even number of values (point pairs).
- At least two points are required.
- $x_i$ must be strictly increasing.
- $x_1 \ge 0$.
- For periodic waveforms, $x_N \le \text{period}$. If $x_1 = 0$ and $x_N = \text{period}$, then $y_1$ must equal $y_N$ (the last point repeats the first); the last point is then dropped.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `wave` | real vector | (empty) | Flat list of $(x_i, y_i)$ pairs: `[x1, y1, x2, y2, ...]`. |
| `scale` | real | 1 | Vertical scaling factor applied to the interpolated value. |
| `offset` | real | 0 | Vertical offset added to the interpolated value after scaling. |
| `stretch` | real | 1 | Horizontal stretch factor. Must be greater than 0. The full $x$-axis is multiplied by `stretch`. |
| `period` | real | 0 | Repetition period in unstretched time. Zero or negative: aperiodic (the waveform plays once and then holds the final value). |
| `tdphase` | real | 0 | Initial phase in degrees, used only when `period > 0`. A phase of $\phi$ shifts the waveform start by $\phi/360 \cdot \text{period}$ in unstretched time. |
| `break` | string | `"auto"` | Breakpoint mode. One of `"all"` (break at every waveform point), `"none"` (no breakpoints), `"auto"` (break only where the slope changes appreciably). |
| `slopetol` | real | 0 | Absolute slope-change tolerance for `break="auto"`. Must be $\ge 0$. |
| `sloperel` | real | 0 | Local relative slope-change tolerance for `break="auto"`. Compared against $\max(|\text{slope before}|, |\text{slope after}|) \cdot \text{sloperel}$. Must be $\ge 0$. |
| `slopeglob` | real | 0.01 | Global relative slope-change tolerance for `break="auto"`. Compared against $\text{max slope across the waveform} \cdot \text{slopeglob}$. Must be $\ge 0$. |

For `break="auto"` a breakpoint is inserted at point $i$ when $|\text{slope}_{i+1} - \text{slope}_i|$ exceeds the largest of the three tolerances. Aperiodic waveforms always have breakpoints at the first and last points regardless of slope.

---

### AM

```text
type="am"  sinedc=0 ampl=1 freq=1k phase=0 delay=0 modfreq=1k modphase=0 modindex=0.5
```

Amplitude-modulated sinusoid.

$$v(t) = \text{sinedc} + \text{ampl} \cdot \sin\!\left(2\pi \cdot \text{freq} \cdot (t - \text{delay}) + \frac{\text{phase} \cdot \pi}{180}\right) \cdot \left(1 + \text{modindex} \cdot \sin\!\left(2\pi \cdot \text{modfreq} \cdot (t - \text{delay}) + \frac{\text{modphase} \cdot \pi}{180}\right)\right)$$

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `sinedc` | real | 0 | DC offset. |
| `ampl` | real | 1 | Carrier amplitude. |
| `freq` | real | 1k | Carrier frequency in Hz. |
| `tdphase` | real | 0 | Carrier initial phase in degrees. |
| `modfreq` | real | 1k | Modulating frequency in Hz. |
| `modphase` | real | 0 | Modulating signal initial phase in degrees. |
| `modindex` | real | 0.5 | Modulation index. |

---

### FM

```text
type="fm"  sinedc=0 ampl=1 freq=1k phase=0 delay=0 modfreq=1k modphase=0 modindex=0.5
```

Frequency-modulated sinusoid.

$$v(t) = \text{sinedc} + \text{ampl} \cdot \sin\!\left(2\pi \cdot \text{freq} \cdot (t - \text{delay}) + \frac{\text{phase} \cdot \pi}{180} + \text{modindex} \cdot \sin\!\left(2\pi \cdot \text{modfreq} \cdot (t - \text{delay}) + \frac{\text{modphase} \cdot \pi}{180}\right)\right)$$

Parameters are the same as for `am`.

---

## Example

```text
Independent source types

ground 0
load "resistor.osdi"

model resistor resistor
model vsource vsource
model isource isource

// DC bias
vdd (vdd 0) vsource dc=1.8

// Sinusoidal input, 1 kHz, used for transient, DC uses the value at t=0.
// mag sets the sine magnitude in small-signal analyses.
vin (in 0) vsource type="sine" sinedc=0 ampl=0.5 freq=1k mag=1

// Pulse clock
vclk (clk 0) vsource type="pulse" val0=0 val1=1.8 rise=1n fall=1n width=500n period=1u

// Aperiodic PWL ramp: 0V until t=0, ramp to 1V at t=1us, hold at 1V
vramp (ramp 0) vsource type="pwl" wave=[0, 0,  1u, 1.0]

// Periodic PWL trapezoid, 1us period, scaled and shifted in time
// Underlying waveform: rise 0->1 in 0..200n, hold 1 in 200n..800n, fall 1->0 in 800n..1u.
// stretch=2 doubles all times -> effective period 2us; delay=500n shifts start.
vtrap (trap 0) vsource type="pwl"
    wave=[0, 0,  200n, 1,  800n, 1,  1u, 0]
    period=1u stretch=2 delay=500n scale=1.8 offset=0

// PWL current pulse, only break at corners that matter
ipwl (vdd inj) isource type="pwl"
    wave=[0, 0,  1u, 0,  1.05u, 1m,  2u, 1m,  2.05u, 0,  3u, 0]
    break="auto" slopeglob=0.01

// Current bias
ibias (vdd out) isource dc=1m

r1 (in 0) resistor r=1k
r2 (out 0) resistor r=1k

control
  abort always
  analysis op1 op
  analysis ac1 ac from=1 to=10M mode="dec" points=10
  analysis tran1 tran stop=3u step=1n
endc
```
