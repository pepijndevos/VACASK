# AC Small-Signal Stability Analysis (acstb)

AC small-signal stability analysis (`acstb`) computes the open-loop gain of a feedback circuit as a function of frequency. The user must insert a probe voltage source serially into the feedback loop; the analysis breaks the loop at that point, applies two independent excitations, and from the responses computes forward and reverse open-loop gains together with the admittance parameters of the device under test (DUT).

## Syntax

```text
analysis name acstb [parameters]
```

## How it works

1. VACASK solves the operating point $x_0$.
2. It linearizes the circuit by computing the resistive Jacobian $J_r$ and the reactive Jacobian $J_c$ at $x_0$.
3. At each frequency $f$ ($\omega = 2\pi f$) it forms the complex system matrix $J_r + j\omega J_c$ and solves it twice:
   - **Current excitation** — unity current injected at the probe's positive node (from the local ground), probe voltage clamped to zero. Responses: probe current $I_{2I}$ and DUT input voltage $U_{1I}$.
   - **Voltage excitation** — unity voltage applied across the probe. Responses: probe current $I_{2U}$ and DUT input voltage $U_{1U}$.
4. The four responses form a parameter matrix:

   $$A = I_{2I}, \quad B = I_{2U}, \quad C = U_{1I}, \quad D = U_{1U}$$

5. Admittance parameters of the DUT (port 1 is the input side) are:

   $$y_{11} = \frac{1+AD-BC-A-D}{C}, \quad y_{12} = \frac{BC-AD+D}{C}$$
   $$y_{21} = \frac{BC-AD+A}{C}, \quad y_{22} = \frac{AD-BC}{C}$$

6. Forward, reverse, and total open-loop gain are:

   $$W_f = \frac{y_{21}}{y_{11}+y_{22}}, \quad W_r = \frac{y_{12}}{y_{11}+y_{22}}, \quad W = W_f + W_r$$

   Apply the Nyquist criterion to $W$ to determine closed-loop stability.

## Probe placement

The probe must be a voltage source (`vsource`) instance inserted serially into the feedback loop. Its positive terminal faces the **DUT input side** and its negative terminal faces the **DUT output side**. When placed this way, $W_f$ carries the forward open-loop gain and $W_r$ the reverse.

Set the probe's `dc` parameter to `0` so it does not disturb the operating point.

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `probe` | string | `""` | Name of the voltage source instance that breaks the feedback loop. Required. |
| `localgnd` | string | `""` | Local ground node used as the voltage reference for $U_{1I}$ and $U_{1U}$. Defaults to global ground (node `0`). |
| `nodeset` | string or list | `""` | Initial guess for the operating point. See [Operating Point Analysis](cmd-analysis-op.md) for syntax. |
| `store` | string | `""` | Save the computed operating point under the given name. See [Operating Point Analysis](cmd-analysis-op.md). |
| `from` | real | `0` | Start frequency (Hz). |
| `to` | real | `0` | Stop frequency (Hz). |
| `step` | real | `0` | Frequency step size (Hz) for a stepped linear sweep. |
| `mode` | string | — | Sweep mode: `"lin"`, `"dec"`, or `"oct"`. |
| `points` | integer | `0` | Number of points (total for `"lin"`, per decade for `"dec"`, per octave for `"oct"`). |
| `values` | real vector | — | Explicit list of frequencies (Hz). Overrides `from`/`to`/`step`/`mode`/`points`. |
| `write` | boolean | `1` | Write the analysis results to a file. |
| `writeop` | boolean | `0` | Also write the operating point results to `<analysis>.op.*`. |

See [AC Small-Signal Analysis](cmd-analysis-ac.md) for a description of sweep modes.

## Save directives

The stability quantities (`wf`, `wr`, `w`, and the $y$-parameters) are always saved; they cannot be individually suppressed.

`acstb` supports all operating point save directives (`v(node)`, `i(instance)`, `p(instance,outvar)`) because it runs an operating point core internally. These directives apply to the operating point results and specify which operating point results to write when `writeop=1`.

## Output

- A file `<analysis>.*` containing the stability results at each frequency point.
- If `writeop=1`, an additional `<analysis>.op.*` file containing the operating point solution.

| Variable | Description |
|----------|-------------|
| `frequency` | Frequency sweep variable (Hz). Always present. |
| `wf` | Forward open-loop gain $W_f$. Complex. |
| `wr` | Reverse open-loop gain $W_r$. Complex. |
| `w` | Total open-loop gain $W = W_f + W_r$. Complex. |
| `y(1,1)` | DUT admittance parameter $y_{11}$. Complex. |
| `y(1,2)` | DUT admittance parameter $y_{12}$. Complex. |
| `y(2,1)` | DUT admittance parameter $y_{21}$. Complex. |
| `y(2,2)` | DUT admittance parameter $y_{22}$. Complex. |

## Examples

**Stability analysis with a decade sweep:**

```text
analysis stb1 acstb probe="vprobe" from=1 to=100M mode="dec" points=10
```

**Full circuit with probe subcircuit and postprocessing:**

```text
Stability analysis of an inverting amplifier

load "resistor.osdi"
load "capacitor.osdi"
load "opamp.osdi"

model r resistor
model c capacitor
model oa opamp rin=100k rout=100 gain=1e4 fp1=1k fp2=100k fp3=1M
model vsrc vsource

r1 (1 n) r r=1k
r2 (n n1) r r=10k
x1 (0 n out) oa
vin (1 0) vsrc dc=0

subckt probe()
  vprobe (n1 out) vsrc dc=0
ends

control
  elaborate circuit("probe")
  analysis stb1 acstb probe="vprobe" from=1 to=100M mode="dec" points=10
  postprocess(PYTHON, "plot.py")
endc

embed "plot.py" <<<FILE
import numpy as np
import matplotlib.pyplot as plt
from rawfile import rawread

stb1 = rawread('stb1.raw').get()
f = np.real(stb1['frequency'])
w = stb1['w']

fig, (ax_mag, ax_ph) = plt.subplots(2, 1)
fig.suptitle('Open-loop gain')
ax_mag.set_ylabel('|W| [dB]')
ax_mag.semilogx(f, 20*np.log10(np.abs(w)))
ax_ph.set_ylabel('phase(W) [deg]')
ax_ph.set_xlabel('f [Hz]')
ax_ph.semilogx(f, np.unwrap(np.angle(w))*180/np.pi)
plt.show()
>>>FILE
```

## Options

- [Small-Signal Analysis Options](cmd-options-smsig.md)
- [Newton-Raphson Solver](cmd-options-nr.md)
- [Homotopy Algorithms](cmd-options-homotopy.md)
