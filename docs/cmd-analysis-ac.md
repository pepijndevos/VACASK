# AC Small-Signal Analysis

AC small-signal analysis computes the linear frequency-domain response of a circuit around its DC operating point. It sweeps a range of frequencies and solves for the complex phasor response at each frequency.

## Syntax

```text
analysis name ac [parameters]
```

## How it works

1. VACASK first performs an operating point (OP) analysis to find the DC solution $x_0$.
2. It linearizes the circuit by computing the resistive Jacobian $J_r$ (derivative of the resistive residual $f$) and the reactive Jacobian $J_c$ (derivative of the reactive residual $q$) at $x_0$.
3. For each frequency $f$ in the sweep it solves

   $$( J_r + j \omega J_c )\, X = U$$

   where $\omega = 2\pi f$ and $U$ comprises the AC excitations supplied by the `mag` and `phase` parameters of independent sources.

The result $X$ holds phasors for all circuit unknowns. A sinusoidal signal $A\cos(\omega t + \varphi)$ corresponds to phasor $A e^{j\varphi}$.

## AC excitation

Independent sources contribute to `U` through their `mag` and `phase` parameters:

- `mag` — excitation magnitude. A negative value is equivalent to adding 180° to the phase.
- `phase` — excitation phase in degrees (default 0).

Sources with neither `mag` nor `phase` set contribute no AC excitation.

## Parameters

AC analysis exposes the operating point parameters and adds sweep and output control parameters.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `nodeset` | string or list | `""` | Initial guess for the operating point. Can be a stored solution name or explicit node voltages. See [Operating Point Analysis](cmd-analysis-op.md) for syntax. |
| `store` | string | `""` | Save the computed operating point under the given name. See [Operating Point Analysis](cmd-analysis-op.md). |
| `from` | real | `0` | Start frequency (Hz) for stepped or mode-based sweeps. |
| `to` | real | `0` | Stop frequency (Hz) for stepped or mode-based sweeps. |
| `step` | real | `0` | Frequency step size (Hz) for a linear stepped sweep. |
| `mode` | string | — | Sweep mode: `"lin"` (linear), `"dec"` (logarithmic per decade), or `"oct"` (logarithmic per octave). |
| `points` | integer | `0` | Number of points (total for `"lin"`, per decade for `"dec"`, per octave for `"oct"`). |
| `values` | list | — | Explicit list of frequencies (Hz). Overrides `from`/`to`/`step`/`mode`/`points`. |
| `write` | boolean | `1` | Write the analysis results to a file. |
| `writeop` | boolean | `0` | Also write the operating point results to `<analysis>.op.*`. |

### Sweep modes

| Mode | Required parameters | Description |
|------|---------------------|-------------|
| Stepped linear | `from`, `to`, `step` | Frequencies `from`, `from+step`, …, `to`. |
| Linear | `from`, `to`, `mode="lin"`, `points` | `points` linearly spaced frequencies from `from` to `to`. |
| Decade | `from`, `to`, `mode="dec"`, `points` | `points` frequencies per decade, logarithmically spaced. |
| Octave | `from`, `to`, `mode="oct"`, `points` | `points` frequencies per octave, logarithmically spaced. |
| Explicit | `values=[f1, f2, …]` | Frequencies taken directly from the list. |

## Save directives

| Directive | Description |
|-----------|-------------|
| `default` | Save all AC node voltages and branch currents (default behavior). |
| `full` | Save all AC node voltages only. |
| `dv(node)` | Save the AC phasor voltage at the given node. |
| `di(instance)` | Save the AC phasor current through the given instance. Only instances that introduce a current variable in the MNA system are valid (e.g. voltage sources, inductors). Equivalent to `dv('instance:flow(br)')`. |

AC analysis also supports all operating point save directives (`v(node)`, `i(instance)`, `p(instance,outvar)`) because it runs an operating point core internally. These directives apply to the operating point results and specify which operating point results to write when `writeop=1`.

## Output

- A file `<analysis>.*` containing the complex phasor results for each saved quantity at each frequency point.
- If `writeop=1`, an additional `<analysis>.op.*` file containing the operating point solution.

| Variable | Description |
|----------|-------------|
| `frequency` | Frequency sweep variable (Hz). Always present. |
| `node` | Complex phasor at the given node. Saved by `dv(node)` or `default`. |
| `instance:flow(br)` | Complex phasor branch flow through the given instance. Saved by `di(instance)` or `default`. |

## Examples

**Decade sweep, 10 points per decade from 1 Hz to 10 kHz:**

```text
analysis ac1 ac from=1 to=10k mode="dec" points=10
```

**Linear sweep with explicit point count:**

```text
analysis ac1 ac from=100 to=1meg mode="lin" points=200
```

**Stepped linear sweep:**

```text
analysis ac1 ac from=1k to=100k step=1k
```

**Explicit frequency list:**

```text
analysis ac1 ac values=[1k, 10k, 100k, 1meg]
```

**Save specific nodes only:**

```text
save dv(out)
save di(Vdd)
analysis ac1 ac from=1 to=1meg mode="dec" points=20
```

**Also write the operating point:**

```text
analysis ac1 ac from=1 to=1meg mode="dec" points=20 writeop=1
```

**Full circuit with embedded postprocessing:**

```text
AC analysis of a nonlinear voltage divider with capacitive load

load "resistor.osdi"
load "capacitor.osdi"
load "spice/diode.osdi"

model resistor resistor
model capacitor capacitor
model vsource vsource
model d sp_diode is=1e-12 n=2

v1 (in 0) vsource dc=0.8 mag=1.0
r1 (in out) resistor r=1k
d1 (out 0) d
c1 (out 0) capacitor c=1u

control
  analysis ac1 ac from=1 to=100k mode="dec" points=10
  postprocess(PYTHON, "plot.py")
endc

embed "plot.py" <<<FILE
import numpy as np
import matplotlib.pyplot as plt
from rawfile import rawread

ac1 = rawread('ac1.raw').get()
f = ac1['frequency']
vout = ac1['out']

fig1, (ax_mag, ax_ph) = plt.subplots(2, 1)
fig1.suptitle('AC analysis')
fig1.set_dpi(100)
ax_mag.set_ylabel('Magnitude [dB]')
ax_mag.semilogx(f.real, 20*np.log10(np.abs(vout)), label="mag(vout)")
ax_ph.set_ylabel('Phase [deg]')
ax_ph.set_xlabel('f [Hz]')
ax_ph.semilogx(f.real, np.unwrap(np.angle(vout))*180/np.pi, label="ph(vout)")
plt.show()
>>>FILE
```

