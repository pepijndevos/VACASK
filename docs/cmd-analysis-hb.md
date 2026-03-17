# Harmonic Balance Analysis

Harmonic balance (HB) analysis computes the quasi-periodic steady-state response of a circuit excited by one or more sinusoidal sources. It solves directly for the complex phasors at each frequency in the spectrum, avoiding the long transient simulation needed to reach steady state.

## Syntax

```text
analysis name hb [parameters]
```

## How it works

The circuit equations are

$$f(x(t)) + \frac{d}{dt} q(x(t)) = 0$$

In steady state, $x(t)$ is almost periodic with fundamental frequencies $f_1, \ldots, f_d$. Its spectrum consists of DC and intermodulation products at frequencies $f_k = \sum_j k_j f_j$. The solver represents $x$ as a vector of complex phasors $X$ (one entry per spectral frequency) and solves the harmonic balance equation

$$\Gamma\, f(\Gamma^{-1} X) + \Omega\, \Gamma\, q(\Gamma^{-1} X) = 0$$

where $\Gamma$ is the APFT (Almost Periodic Fourier Transform) mapping phasors to colocation timepoints, $\Gamma^{-1}$ is its inverse, and $\Omega$ is the frequency-domain time-derivative operator ($\Omega_{kk} = j\omega_k$). The Jacobian is assembled from the resistive and reactive circuit Jacobians evaluated at the colocation timepoints and is used by Newton-Raphson to converge to the solution.

### Spectrum truncation

Three truncation schemes control which intermodulation products are included in the spectrum: 

| Scheme | Description |
|--------|-------------|
| `"box"` | Includes all products with $0 \leq k_j \leq H_j$ (where $H_j$ is specified with `nharm`). |
| `"diamond"` | Like `box`, except that it includes only products with $a \leq \sum_j \|k_j\| \leq \mathrm{immax}$. Default. |
| `"raw"` | Uses the frequencies listed in `freq` directly as the spectrum. |

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `freq` | real vector | — | Fundamental frequencies (Hz). Required. A single value for single-tone; a list for multi-tone. |
| `nharm` | integer or integer vector | `4` | Number of harmonics per fundamental. Scalar applies to all; a vector sets per-tone limits. |
| `immax` | integer | `0` | Maximum intermodulation order for `"diamond"` truncation. If ≤ 0, defaults to the largest component of `nharm`. |
| `truncate` | string | `"diamond"` | Spectrum truncation scheme: `"diamond"`, `"box"`, or `"raw"`. |
| `samplefac` | real | `2` | Oversampling factor for colocation timepoints (≥ 1). Only the best $2n$ points are used where $n$ is the spectrum size. |
| `nper` | real | `3` | Number of periods over which colocation timepoints are distributed. |
| `sample` | string | `"random"` | Colocation sampling mode: `"uniform"` or `"random"`. |
| `nodeset` | string | `""` | Name of saved solution used as initial guess. |
| `store` | string | `""` | Save the computed solution under the given name. |
| `write` | boolean | `1` | Write the results to a file. |

## Save directives

| Directive | Description |
|-----------|-------------|
| `default` | Save all node phasors and branch flows (default behavior). |
| `full` | Saves all phasors (even those belonging to collapsed nodes). |
| `v(node)` | Save the phasor at the given node. |
| `i(instance)` | Save the branch flow phasor through the given instance. Only instances that introduce a current variable in the MNA system are valid (e.g. voltage sources, inductors). Equivalent to `v('instance:flow(br)')`. |

## Output

- A file `<analysis>.*` containing the complex phasors for each saved quantity at each spectral frequency.

| Variable | Description |
|----------|-------------|
| `frequency` | Spectral frequencies (Hz). Always present. DC is at index 0. |
| `node` | Complex phasor at the given node. Saved by `v(node)` or `default`. |
| `instance:flow(br)` | Complex phasor branch flow through the given instance. Saved by `i(instance)` or `default`. |

The `frequency` variable has a complex type; use `.real` when passing it to plotting functions.

## Examples

**Single-tone analysis, 12 harmonics:**

```text
analysis hb1 hb freq=[1k] nharm=12
```

**Multi-tone with diamond truncation, order 5:**

```text
analysis hb1 hb freq=[5k, 50k] nharm=5 immax=5
```

**Full circuit with embedded postprocessing:**

```text
HB of a cubic nonlinearity driven at 1 kHz

load "resistor.osdi"
load "nl3.va"

model vsource vsource
model resistor resistor
model nl3 nl3

v1 (1 0) vsource type="sine" sinedc=0 ampl=1 freq=1k
r1 (1 2) resistor r=1
nl1 (2 0) nl3

control
  analysis hb1 hb freq=[1k] nharm=8
  postprocess(PYTHON, "plot.py")
endc

embed "nl3.va" <<<FILE
`include "constants.vams"
`include "disciplines.vams"

module nl3(A, B);
    inout A, B;
    electrical A, B;
    analog begin
        I(A,B) <+ 0.5 + pow(V(A,B), 3);
    end
endmodule
>>>FILE

embed "plot.py" <<<FILE
import numpy as np
import matplotlib.pyplot as plt
from rawfile import rawread

hb1 = rawread('hb1.raw').get()
f = hb1['frequency'].real
v2 = hb1['2']

fig1, (ax_mag, ax_ph) = plt.subplots(2, 1, figsize=(6, 5), constrained_layout=True)
fig1.suptitle('HB spectrum at node 2')
ax_mag.set_ylabel('Magnitude')
ax_mag.stem(f / 1e3, np.abs(v2), markerfmt='.')
ax_ph.set_ylabel('Phase [deg]')
ax_ph.set_xlabel('f [kHz]')
ax_ph.stem(f / 1e3, np.angle(v2) * 180 / np.pi, markerfmt='.')
plt.show()
>>>FILE
```
