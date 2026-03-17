# Small-Signal Noise Analysis

Small-signal noise analysis computes the output-referred noise power spectral density of a circuit and the contributions of individual noise sources to that output, as a function of frequency. It also computes the power gain from a designated input source to the output.

## Syntax

```text
analysis name noise [parameters]
```

## How it works

1. VACASK solves the operating point $x_0$.
2. It linearizes the circuit by computing the resistive Jacobian $J_r$ and reactive Jacobian $J_c$ at $x_0$.
3. For each noise source in each instance it solves

   $$(J_r + j \omega J_c)\, X = U$$

   with $U$ set to a unity excitation at that noise source's terminals. The resulting $U$ 
   is used for computing the transfer function $H$ from the noise source to the circuit's 
   output. 
4. The contribution of that noise source to the output power spectral density is

   $$S_\text{out} = |H|^2\, |S_\text{noise}|$$

   where $H = V_\text{out} / V_\text{excitation}$ is the transfer gain to the output node.
   Note that the absolute value of the noise source's PSD is used because Verilog-A can 
   also produce negative PSD values to indicate noise modulation sign change. 
5. Contributions are accumulated per instance and summed into the total output noise.
6. The power gain $G = |H_\text{in}|^2$ is computed by additionally solving the system with 
   unity excitation at the input source. It can be used for computing the equivalent total input noise. 
7. Steps 3–6 are repeated across the frequency sweep.

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `out` | string or string vector | `""` | Output node or differential node pair. A single string specifies a node to ground; a two-element vector specifies a node pair. |
| `in` | string | `""` | Instance name of the independent source used as the input reference for power gain. |
| `nodeset` | string or list | `""` | Initial guess for the operating point. See [Operating Point Analysis](cmd-analysis-op.md) for syntax. |
| `store` | string | `""` | Save the computed operating point under the given name. See [Operating Point Analysis](cmd-analysis-op.md). |
| `from` | real | `0` | Start frequency (Hz). |
| `to` | real | `0` | Stop frequency (Hz). |
| `step` | real | `0` | Frequency step size (Hz) for a stepped linear sweep. |
| `mode` | string | — | Sweep mode: `"lin"`, `"dec"`, or `"oct"`. |
| `points` | integer | `0` | Number of points (total for `"lin"`, per decade for `"dec"`, per octave for `"oct"`). |
| `values` | real vector | — | Explicit vector of frequencies (Hz). Overrides `from`/`to`/`step`/`mode`/`points`. |
| `write` | boolean | `1` | Write the analysis results to a file. |
| `writeop` | boolean | `0` | Also write the operating point results to `<analysis>.op.*`. |

See [AC Small-Signal Analysis](cmd-analysis-ac.md) for a description of sweep modes.

## Save directives

| Directive | Description |
|-----------|-------------|
| `default` | Save total noise contribution `n(instance)` for all noisy instances (default behavior). |
| `full` | Save total `n(instance)` and per-source `n(instance,contrib)` for all noisy instances and all their noise sources. |
| `n(instance)` | Save the total output-referred noise contribution of the given instance. |
| `nc(instance,contrib)` | Save the output-referred contribution of a specific noise source `contrib` within `instance`. |

In addition, noise analysis supports all operating point save directives (`v(node)`, `i(instance)`, `p(instance,outvar)`) because it runs an operating point core internally. These directives apply to the operating point results and specify which operating point results to write when `writeop=1`.

## Output

The output file always contains:

| Variable | Description |
|----------|-------------|
| `frequency` | Frequency sweep variable (Hz). |
| `onoise` | Total output-referred noise power spectral density. |
| `gain` | Power gain from the input source to the output (dimensionless). |
| `n(instance)` | Total output-referred noise PSD contributed by `instance`. |
| `n(instance,contrib)` | Output-referred noise PSD of the specific noise source `contrib` within `instance`. |

## Examples

**Basic noise analysis:**

```text
analysis noise1 noise out="3" in="v2" from=1 to=10k mode="dec" points=10
```

**With full per-source breakdown:**

```text
save full
analysis noise1 noise out="3" in="v2" from=1 to=10k mode="dec" points=10
```

**Full circuit with embedded postprocessing:**

```text
Noise of a nonlinear voltage divider

load "resistor.osdi"
load "capacitor.osdi"
load "spice/diode.osdi"

model resistor resistor has_noise=1
model capacitor capacitor
model vsource vsource
model d sp_diode is=1e-12 n=2 kf=1e-15 af=1.2

v1 (in 0) vsource dc=0.8
r1 (in out) resistor r=1k
d1 (out 0) d
c1 (out 0) capacitor c=1u

control
  save full
  analysis noise1 noise out="out" in="v1" from=1 to=10k mode="dec" points=10
  postprocess(PYTHON, "plot.py")
endc

embed "plot.py" <<<FILE
import numpy as np
import matplotlib.pyplot as plt
from rawfile import rawread

noise1 = rawread('noise1.raw').get()
f = noise1['frequency'].real

fig1, (ax_noise, ax_gain) = plt.subplots(2, 1)
fig1.suptitle('Noise analysis')
fig1.set_dpi(100)
ax_noise.set_ylabel('PSD [V²/Hz]')
ax_noise.loglog(f, noise1['n(r1)'],  label='R1')
ax_noise.loglog(f, noise1['n(d1)'],  label='D1')
ax_noise.loglog(f, noise1['onoise'], label='output')
ax_noise.legend(loc='lower left')
ax_noise.grid(True)
ax_gain.set_ylabel('Power gain [dB]')
ax_gain.set_xlabel('f [Hz]')
ax_gain.semilogx(f, 10*np.log10(noise1['gain']), label='gain')
ax_gain.grid(True)
plt.show()
>>>FILE
```
