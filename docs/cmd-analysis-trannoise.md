# Transient Noise Analysis

Transient noise analysis is an ordinary transient analysis where intrinsic noise sources of devices inject noise into the circuit in the time domain. Both white (thermal, shot) and flicker (1/f) noise types are supported.

## Syntax

```text
analysis name tran [parameters]
```

## How it works

Noise sources declared in device models contribute random excitations to the circuit equations at each timestep. VACASK generates band-limited noise spanning the frequency range from `noisefmin` to `noisefmax`. Noise is disabled by default and is enabled by setting `noisefmax` to a nonzero value.

## Parameters

All [Transient Analysis](cmd-analysis-tran.md) parameters apply. The following additional parameters control the noise generation.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `noisefmax` | real | `0` | Upper frequency limit of the generated noise (Hz). Set to a nonzero value to enable transient noise. |
| `noisefmin` | real | `0` | Lower frequency limit of the generated noise (Hz). If `0`, defaults to `noisefmax/1e3`. Must be less than `noisefmax`. |
| `noisemode` | string | `"zoh"` | Noise generation algorithm: `"zoh"` (zero-order hold, default) or `"sde"` (stochastic differential equations). |
| `oversample` | integer | `6` | Oversampling factor relative to the Nyquist rate at `noisefmax`. Must be >= 1. Higher values improve accuracy. |
| `noiseseed` | integer | `0` | Seed for the random number generator. Fixing the seed makes the noise sequence reproducible. |
| `noisescale` | real | `1.0` | Scaling factor applied to all noise source amplitudes. |

## Maximum timestep

When noise is enabled, the integrator automatically limits the maximum timestep to

$$h_\text{max} = \frac{1}{2 \cdot \text{oversample} \cdot \text{noisefmax}}$$

This ensures that the simulation resolves the noise bandwidth. The limit is applied on top of any user-specified `maxstep`.

To avoid excessive runtime, choose `noisefmax` as the highest frequency relevant to your analysis, not larger. If the circuit also requires a small `maxstep` for accuracy (e.g., to resolve a fast waveform), set it explicitly; the tighter of the two limits applies.

## Noise modes

### SDE mode

In SDE mode the noise sources are driven by stochastic differential equations (SDEs). White noise at each timestep $h$ is drawn as $\mathcal{N}(0,1)/\sqrt{2h}$ (where $\mathcal{N}(0,1)$ is a normally distributed random number with zero mean and variance 1), which gives a one-sided PSD of 1. Flicker noise is approximated by a sum of $k$ Lorentzian (Ornstein-Uhlenbeck) processes with corner frequencies $f_i = f_\text{max}/2^i$ for $i=0,\ldots,k-1$, covering the range from `noisefmax` down to approximately `noisefmin` one octave per Lorentzian; the weights are set analytically and then optimized numerically to reproduce the target 1/f^alpha spectral shape. Because the SDE state advances continuously with the simulation time, SDE mode is compatible with the adaptive timestep integrator. Computationally SDE mode is slightly more expensive than ZOH mode.

### ZOH mode (default)

In ZOH mode (zero-order hold) noise samples are generated on a uniform time grid with spacing equal to the maximum timestep, and each sample is held constant until the next grid point. White noise draws an independent Gaussian sample at each grid point. Flicker noise uses a Voss-McCartney style algorithm with $k$ rows: at each grid point exactly one row is randomly selected for update and redrawn from a scaled Gaussian, with row $i$ chosen with probability $1/2^{i+1}$ (determined by the trailing-zero count of a random 64-bit integer); with probability $1/2^k$ no row is updated. Each row produces a Lorentzian PSD with a corner frequency that halves from one row to the next. The output sample is the sum of all row values, and the row weights are set analytically and then optimized numerically so that the sum of Lorentzians approximates the target 1/f^alpha PSD. ZOH mode is simpler but the piecewise-constant noise waveform can cause additional LTE-driven step reductions.

## Save directives

Same as [Transient Analysis](cmd-analysis-tran.md).

## Output

Same as [Transient Analysis](cmd-analysis-tran.md). The output file contains time-domain waveforms including the effect of noise.

## Example

**RC circuit with thermal noise:**

```text
RC circuit thermal noise

load "resistor.osdi"
load "capacitor.osdi"

model r resistor
model c capacitor
model i isource

r1 (1 0) r r=1k
c1 (1 0) c c=100u
i1 (0 1) i dc=0

control
  var fmin=0.1
  var fmax=100.0

  analysis tran1 tran stop=20/fmin step=1m noisefmax=fmax noisefmin=fmin oversample=6
  postprocess(PYTHON, "plot.py")
endc

embed "plot.py" <<<FILE
import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import welch
from rawfile import rawread

fmin = 0.1
fmax = 100.0

plot = rawread('tran1.raw').get()
t = plot['time']
v = plot['1']

# The simulation uses an adaptive timestep, so resample to a uniform grid
# before computing the PSD.
dt = t[-1] / t.shape[0]
ts = np.arange(t[0], t[-1], dt)
vs = np.interp(ts, t, v)

# Choose nperseg so that the frequency resolution is fmin/2.
nperseg = int(np.ceil(1.0 / (dt * fmin / 2)))
f, Pxx = welch(vs, fs=1.0/dt, window='hann', nperseg=nperseg,
               noverlap=nperseg//2, detrend=False,
               return_onesided=True, scaling='density')

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(6, 6), dpi=100)
fig.suptitle('RC circuit thermal noise')

ax1.set_ylabel('V(1) [V]')
ax1.set_xlabel('Time [s]')
ax1.plot(t, v)

ax2.set_ylabel('PSD [V^2/Hz]')
ax2.set_xlabel('f [Hz]')
ax2.loglog(f, Pxx)
ax2.set_xlim(fmin, fmax)
ax2.grid(True)

fig.tight_layout()
plt.show()
>>>FILE
```

The resistor contributes thermal noise across the frequency band `[fmin, fmax]`. The capacitor integrates the noise, giving a low-pass shaped output spectrum.

## Options

- [Transient Analysis Options](cmd-options-tran.md)
- [Newton-Raphson Solver](cmd-options-nr.md)
