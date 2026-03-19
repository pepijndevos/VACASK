# Independent Sources

Module names: `vsource`, `isource`

`vsource` and `isource` are independent voltage and current sources. They share the same parameter set. The `type` parameter selects the transient waveform; `mag` and `phase` set the small-signal excitation used by AC and DC incremental analyses independently of the transient type.

## Terminals

Both devices have two terminals that must be connected: `p n`.

| Terminal | Role |
|----------|------|
| `p` | Positive terminal |
| `n` | Negative terminal |

`vsource` enforces V(p) − V(n) = waveform value. It introduces one internal unknown (the branch current) and has a `flow(br)` internal node that current-controlled sources can reference.

`isource` drives a current equal to the waveform value from `p` to `n` through the device (i.e., the current exits into the circuit at terminal `n`).

## Common parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `type` | string | `"dc"` | Waveform type. One of `"dc"`, `"sine"`, `"pulse"`, `"exp"`, `"am"`, `"fm"`. |
| `delay` | real | 0 | Start time (s). Before this time the source holds the value it would have at t = `delay`. |
| `mag` | real | 0 | Small-signal excitation amplitude. For `vsource`: voltage in V. For `isource`: current per instance in A. |
| `phase` | real | 0 | Small-signal excitation phase in degrees. |
| `$mfactor` | real | 1 | Number of parallel instances. For `vsource` the voltage value is unaffected; for `isource` the total current scales with `$mfactor`. |

## Output variables

| Variable | Description |
|----------|-------------|
| `v` | Terminal voltage V(p) − V(n) |
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

```text
v(t) = sinedc + ampl × sin(2π × freq × (t − delay) + phase × π/180) × exp(−theta × (t − delay))
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `sinedc` | real | 0 | DC offset added to the sinusoid. |
| `ampl` | real | 1 | Amplitude. |
| `freq` | real | 1k | Frequency in Hz. Must be greater than 0. |
| `sinephase` | real | 0 | Initial phase in degrees. |
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
| `fall` | real | 0 | Fall time (s). Zero or negative: no fall — stays at `val1`. |
| `width` | real | 0 | Flat-top duration (s) between end of rise and start of fall. Zero: no flat top. |
| `period` | real | 0 | Repetition period (s). Zero or negative: single pulse. Must be greater than `rise + fall + width` when positive. |

---

### Exp

```text
type="exp"  val0=0 val1=1 delay=0 td2=... tau1=... tau2=...
```

Double-exponential: a rising exponential followed by a falling one.

```text
t < delay:              v(t) = val0
delay ≤ t < delay+td2: v(t) = val0 + (val1−val0) × (1 − exp(−(t−delay)/tau1))
t ≥ delay+td2:          v(t) = val0 + (val1−val0) × (1 − exp(−(t−delay)/tau1))
                               + (val0−val1) × (1 − exp(−(t−delay−td2)/tau2))
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `val0` | real | 0 | Initial and final value. |
| `val1` | real | 1 | Peak value at the turning point. |
| `td2` | real | 0 | Duration of the rising phase (s), measured from `delay`. Must be greater than 0. |
| `tau1` | real | 0 | Rise time constant (s). |
| `tau2` | real | 0 | Fall time constant (s). |

---

### AM

```text
type="am"  sinedc=0 ampl=1 freq=1k phase=0 delay=0 modfreq=1k modphase=0 modindex=0.5
```

Amplitude-modulated sinusoid.

```text
v(t) = sinedc + ampl × sin(2π × freq × (t−delay) + phase×π/180)
              × (1 + modindex × sin(2π × modfreq × (t−delay) + modphase×π/180))
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `sinedc` | real | 0 | DC offset. |
| `ampl` | real | 1 | Carrier amplitude. |
| `freq` | real | 1k | Carrier frequency in Hz. |
| `sinephase` | real | 0 | Carrier initial phase in degrees. |
| `modfreq` | real | 1k | Modulating frequency in Hz. |
| `modphase` | real | 0 | Modulating signal initial phase in degrees. |
| `modindex` | real | 0.5 | Modulation index. |

---

### FM

```text
type="fm"  sinedc=0 ampl=1 freq=1k phase=0 delay=0 modfreq=1k modphase=0 modindex=0.5
```

Frequency-modulated sinusoid.

```text
v(t) = sinedc + ampl × sin(2π × freq × (t−delay) + phase×π/180
              + modindex × sin(2π × modfreq × (t−delay) + modphase×π/180))
```

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
