# Periodic Steady-State Analysis

Periodic steady-state (PSS) analysis computes the periodic large-signal steady state of a circuit in the time domain using the single-shooting Newton method. It handles driven circuits, where the period is dictated by the excitation, and autonomous circuits (oscillators), where the oscillation period is unknown and is computed along with the waveform. The frequency-domain alternative is [harmonic balance](cmd-analysis-hb.md); shooting is preferred for strongly nonlinear circuits with sharp waveforms and for finding the exact oscillation frequency of oscillators.

## Syntax

```text
analysis name pss [parameters]
```

## How it works

The circuit equations are

$$f(x(t)) + \frac{d}{dt} q(x(t)) = 0$$

A periodic steady state is a solution satisfying $x(t+T) = x(t)$. Let $\phi_T(x_0)$ denote the state reached by integrating the circuit equations over one period $T$ starting from state $x_0$. The shooting method solves

$$F(x_0) = x_0 - \phi_T(x_0) = 0$$

with Newton-Raphson iteration. The analysis proceeds in three stages:

1. **Operating point.** Unless initial conditions are given with `ic`, the DC operating point is computed (honoring `nodeset`) and used as the starting state.

2. **Stabilization transient.** A plain transient of length `tstab` lets the initial transients decay (driven circuits) or lets the oscillation build up toward the limit cycle (oscillators). Its endpoint becomes the initial guess $x_0$ for the Newton loop. 

3. **Shooting Newton loop.** Each iteration integrates the circuit over one period with the ordinary transient integrator. Alongside the circuit, the variational (linearized) system

   $$C(t)\,\frac{d\Phi}{dt} + G(t)\,\Phi = 0, \qquad \Phi(0) = I$$

   is integrated with the same integration scheme, where $G(t)$ and $C(t)$ are the resistive and reactive Jacobians along the trajectory. The result $\Phi_T = d\phi_T/dx_0$ is the monodromy (state-transition) matrix. It yields the Newton step

   $$(I - \Phi_T)\, \Delta x_0 = x_0 - x_T, \qquad x_0^{(l+1)} = x_0^{(l)} - \Delta x_0$$

   where $x_T = \phi_T(x_0)$ is the endpoint of the shot.

After convergence one final period is integrated with output enabled and written to the results file.

### Autonomous circuits

For oscillators (`driven=0`, the default) the period $T$ is an additional unknown and `tper` is only its initial guess. Each Newton iteration solves the augmented $(n+1) \times (n+1)$ system

$$\begin{pmatrix} I - \Phi_T & \Psi_T \\ \alpha^T & 0 \end{pmatrix} \begin{pmatrix} \Delta x_0 \\ \Delta T \end{pmatrix} = \begin{pmatrix} x_0 - x_T \\ 0 \end{pmatrix}, \qquad x_0^{(l+1)} = x_0^{(l)} - \Delta x_0, \quad T^{(l+1)} = T^{(l)} - \Delta T$$

where $\Psi_T = -dx_T/dT$ is the period sensitivity vector and $\alpha$ is a phase constraint vector. The phase constraint removes the underdetermination caused by time-shift invariance (any time-shifted copy of a periodic solution is also a solution); $\alpha$ is chosen proportional to the circuit velocity $\dot{x}(t_0)$ so the Newton correction does not move along the orbit.

The DC operating point of an oscillator is itself a valid (but unstable) periodic solution. The circuit must be kicked away from it, with `ic`. Otherwise the stabilization transient stays at the operating point and the analysis converges to the degenerate DC solution.

### Convergence

The shooting residual is checked per unknown against the same tolerances used by the transient analysis:

$$|x_{0,i} - x_{T,i}| < \mathrm{pss\_tolscale} \cdot \max(\mathrm{reltol} \cdot |x_{0,i}|,\ \mathrm{abstol}_i)$$

The tolerance can be tightened or relaxed with the `pss_tolscale` option. The loop gives up after `pss_itl` iterations. See [Periodic Steady-State Options](cmd-options-pss.md).

### Timestep control

The shooting transient uses an initial and maximum timestep of `T`/`pss_minpts`, where `T` is the current period estimate. The stabilization transient uses `stabstep` (or `tper`/`pss_minpts` when not given). `pss_minpts` defaults to 1000; see [Periodic Steady-State Options](cmd-options-pss.md). When `maxacfreq` is positive, the maximum timestep of both transients is additionally limited to $1/(2 \cdot \mathrm{maxacfreq})$; values of `maxacfreq` below 40/`tper` are raised to 40/`tper`. The integration method and maximum order are selected with the `tran_method` and `tran_maxord` simulator options (see [Transient Analysis Options](cmd-options-tran.md)).

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `tper` | real | `0` | Period (s). For driven circuits the exact excitation period; for autonomous circuits the initial period guess. Required, must be > 0. |
| `driven` | boolean | `0` | Set to 1 for driven (non-autonomous) circuits; the period is then fixed at `tper`. With 0 the circuit is treated as autonomous and the period is solved for. |
| `tstab` | real | `0` | Length of the stabilization transient (s). With 0 no stabilization is performed and the operating point (or `ic`) is used directly as the initial guess. |
| `stabstep` | real | `0` | Timestep of the stabilization transient (s). If 0, `tper`/`pss_minpts` is used. If given, it must be smaller than `tstab`. |
| `maxacfreq` | real | `0` | Highest frequency to resolve (Hz). Limits the maximum timestep to $1/(2 \cdot \mathrm{maxacfreq})$. Values below 40/`tper` are raised to 40/`tper`. 0 disables the limit. |
| `icmode` | string | `"op"` | Initial condition mode for the stabilization transient: `"op"` solves an operating point first with `ic` applied as forced constraints; `"uic"` skips the operating point and applies `ic` directly. |
| `ic` | string or list | `""` | Initial conditions for the stabilization transient. A stored solution name or a list of node/value pairs. How the values are applied is governed by `icmode`. When a stored PSS solution name is given, the saved period is used as the initial period estimate, if `tper` is not given. See [Transient Analysis](cmd-analysis-tran.md). |
| `nodeset` | string or list | `""` | Nodeset for the operating point solve. See [Operating Point Analysis](cmd-analysis-op.md). |
| `store` | string | `""` | Save the converged PSS solution and period under the given name. The stored entry can be passed back as `ic` to a subsequent PSS analysis to warm-start from the previous result, supplying both the initial state vector and the converged period. |
| `writestab` | boolean | `0` | Write the stabilization transient results to a separate file. |
| `write` | boolean | `1` | Write the steady-state waveform (one period) to a file. |

## Save directives

| Directive | Description |
|-----------|-------------|
| `default` | Save all node values and branch flows (default behavior). |
| `full` | Saves all unknowns (even those belonging to collapsed nodes). |
| `v(node)` | Save the value at the given node. |
| `i(instance)` | Save the branch flow through the given instance. Only instances that introduce a current variable in the MNA system are valid (e.g. voltage sources, inductors). Equivalent to `v('instance:flow(br)')`. |
| `p(instance,outvar)` | Save the output variable `outvar` from the given instance. |

## Output

- A file `<analysis>.*` containing one period of the steady-state waveform. Time runs from 0 to the period; for autonomous circuits the converged period (and hence the oscillation frequency) is read off the last timepoint.
- A file `<analysis>.tran.*` containing the stabilization transient (written when `writestab=1`).

| Variable | Description |
|----------|-------------|
| `time` | Time sweep variable (s). Always present. |
| `node` | Value at the given node. Saved by `v(node)` or `default`. |
| `instance:flow(br)` | Branch flow through the given instance. Saved by `i(instance)` or `default`. |
| `instance.outvar` | Output variable `outvar` from the given instance. Saved by `p(instance,outvar)`. |

## Examples

**Driven circuit excited at 50 Hz (period fixed at 20 ms):**

```text
analysis pss1 pss driven=1 tper=20m tstab=200m
```

**Autonomous oscillator with initial period guess and a kick via initial conditions:**

```text
analysis pss1 pss tper=1.1n tstab=150n ic={"vout", 1.0}
```

**Full circuit with embedded postprocessing:**

```text
Van der Pol LC oscillator

load "resistor.osdi"
load "capacitor.osdi"
load "inductor.osdi"
load "vdp_nl.va"

model resistor  resistor
model capacitor capacitor
model inductor  inductor
model vdp_nl    vdp_nl

c2 (vout 0)  capacitor c=4.5p
l1 (vout lp) inductor  l=7n
r1 (lp 0)    resistor  r=0.5
c1 (lp 0)    capacitor c=1p
d1 (vout 0)  vdp_nl

control
  save default
  // ic kicks the circuit away from its unstable DC fixed point
  analysis pss1 pss tper=1.1n tstab=150n ic={"vout", 1.0} writestab=1
  postprocess(PYTHON, "plot.py")
endc

embed "vdp_nl.va" <<<FILE
`include "constants.vams"
`include "disciplines.vams"

// Van der Pol nonlinear conductance: I = -1mS*V + 100u*V^3
module vdp_nl(p, n);
    inout p, n;
    electrical p, n;
    analog begin
        I(p, n) <+ -1e-3*V(p, n) + 100e-6 * V(p, n) * V(p, n) * V(p, n);
    end
endmodule
>>>FILE

embed "plot.py" <<<FILE
import numpy as np
import matplotlib.pyplot as plt
from rawfile import rawread

pss1 = rawread('pss1.raw').get()
t = np.real(pss1['time'])
v = np.real(pss1['vout'])

T0 = t[-1]
print(f"Oscillation period {T0*1e9:.5f} ns, frequency {1e-6/T0:.3f} MHz")

fig1, ax1 = plt.subplots(1, 1)
fig1.suptitle('Van der Pol oscillator steady state')
ax1.set_ylabel('V(vout) [V]')
ax1.set_xlabel('Time [ns]')
ax1.plot(t*1e9, v)
ax1.grid(True)
plt.show()
>>>FILE
```

## See also

- [Harmonic Balance Analysis](cmd-analysis-hb.md) — frequency-domain periodic steady-state analysis.
- [Transient Analysis](cmd-analysis-tran.md)

## Options

- [Periodic Steady-State Options](cmd-options-pss.md)
- [Transient Analysis Options](cmd-options-tran.md)
- [Newton-Raphson Solver](cmd-options-nr.md)
- [Homotopy Algorithms](cmd-options-homotopy.md)
