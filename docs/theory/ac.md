# AC Analysis

AC analysis computes the frequency-domain response of a circuit to sinusoidal excitations about a DC operating point. It assumes small-signal linear behavior and steady-state conditions.

## Overview

Given a DC operating point $x^*$, AC analysis applies a small sinusoidal perturbation to independent sources at a frequency $\omega$ and computes the resulting steady-state response. The analysis assumes:

- Sinusoidal sources: $u(t) = U_0 \cos(\omega t + \phi) = \text{Re}(U e^{j\omega t})$
- Small-signal approximation: perturbations are linear around $x^*$
- Steady-state: transients have died out, only the driven oscillation remains

The response is computed in phasor form: complex-valued, frequency-dependent amplitude and phase at each frequency of interest.

## Small-Signal Linearization

Starting from the DAE:

$$f(x, t) + \frac{d}{dt}\,q(x, t) = 0,$$

expand around the operating point $x^* = x(t) - x^*$ (where $x(t)$ is the total solution) at $t=t_0$:

$$f(x^*, t_0) + J_r(x^*, t_0)\,\tilde x + \frac{d}{dt}[q(x^*, t_0) + J_c(x^*, t_0)\,\tilde x] + \text{higher order} = 0.$$

At the operating point, $f(x^*, t_0) + \frac{d}{dt}\,q(x^*, t_0) = 0$ (the DC bias), so the linearized equation is:

$$J_r(x^*, t_0)\,\tilde x + J_c(x^*, t_0)\,\frac{d\tilde x}{dt} = 0,$$

where $\tilde x = x(t) - x^*$ is the small-signal perturbation and $J_r$, $J_c$ are the resistive and reactive Jacobians evaluated at the operating point.

## Phasor Representation

For sinusoidal steady-state at frequency $\omega$, represent perturbations as:

$$\tilde x(t) = \text{Re}(\tilde X e^{j\omega t}),$$

where $\tilde X \in \mathbb{C}^n$ is the phasor (complex amplitude). The time derivative is:

$$\frac{d\tilde x}{dt} = \text{Re}(j\omega \tilde X e^{j\omega t}).$$

Substitute into the linearized DAE and drop the real-part operator (work in complex amplitudes):

$$J_r(x^*, t_0)\,\tilde X + j\omega J_c(x^*, t_0)\,\tilde X = \Delta U,$$

where $\Delta U$ is the phasor of source excitations. Rearranging:

$$[J_r(x^*, t_0) + j\omega J_c(x^*, t_0)]\,\tilde X = \Delta U.$$

## The AC System Matrix

Define the AC Jacobian at frequency $\omega$:

$$J_{AC}(\omega) = J_r(x^*, t_0) + j\omega J_c(x^*, t_0).$$

This is a complex-valued matrix. The AC response equation becomes:

$$J_{AC}(\omega)\,\tilde X = \Delta U.$$

For each frequency $\omega$ of interest, solve this complex linear system to obtain the phasor response $\tilde X(\omega)$.

## Source excitation and $mfactor

The right-hand side $\Delta U$ is assembled from the AC excitations contributed by each independent source. Two source parameters drive the contribution: `mag` (the AC magnitude in the netlist) and `phase` (in degrees). The complex phasor of source $\ell$ is

$$\hat U_\ell \;=\; \mathrm{mag}_\ell \cdot e^{j\,\mathrm{phase}_\ell \cdot \pi/180}.$$

The source's `$mfactor` $m$ enters differently for the two source types:

- **Voltage source:** the AC excitation is loaded into the source's branch-equation row only - the constraint $v_+ - v_- = \hat U_\ell$ is imposed regardless of `$mfactor`. All $m$ parallel instances impose the same voltage, so $m$ does not appear in $\Delta U$:

  $$\Delta U \;\mathrel{+}=\; \hat U_\ell\,\big(e_{e_1^{(\ell)}} - e_{e_2^{(\ell)}}\big).$$

- **Current source:** the AC excitation is summed into the KCL equations of the two terminal nodes, scaled by `$mfactor` because $m$ parallel sources each push $\hat U_\ell$ amperes, for a total of $m\,\hat U_\ell$:

  $$\Delta U \;\mathrel{+}=\; -m\,\hat U_\ell\,\big(e_{p^{(\ell)}} - e_{n^{(\ell)}}\big).$$

The factored matrix is therefore solved once per frequency against a single RHS that already contains every source's contribution at its own `mag`, `phase`, and (for current sources) `$mfactor`. The resulting response phasor $\tilde X(\omega)$ is the superposition of the responses to all sources; isolating one source's contribution requires setting the others' `mag` to zero or using AC transfer function analysis (see [acxf.md](acxf.md)).

For interpretation: a voltage source at `mag = 1` always represents 1 V at the port. A current source at `mag = 1` with `$mfactor = m` represents $m$ A injected in total, so the response at any node is $m$ times what a single 1 A source would produce. 
