# AC Stability Analysis

AC stability analysis assesses the small-signal stability of a feedback circuit by computing forward, reverse, and total open-loop gain as functions of frequency. Nyquist's criterion can then be applied to the resulting gain to determine closed-loop stability.

## Overview

The user breaks the feedback loop by inserting a zero-valued voltage source (the **probe**) into the loop. Around a DC operating point $x^*$, the linearized circuit equations at angular frequency $\omega$ are given by

$$[J_r(x^*) + j\omega\, J_c(x^*)]\,\tilde X = \Delta U,$$

with $J_r$ and $J_c$ the resistive and reactive Jacobians (see [ac.md](ac.md)). Two complex solves per frequency, with two different right-hand sides driven through the probe, give the four two-port parameters that describe the **device under test** (DUT) as seen across the probe. The loop gain follows from those parameters in closed form.

The probe is oriented so that its positive terminal connects to the DUT input side and its negative terminal to the DUT output side. With this convention the probe sits in series with the feedback path, and the recovered two-port describes the DUT from its input port (side 1) to its output port (side 2).

## Probe and reference ground

Let $V_{\text{p}}$ denote the probe voltage (positive terminal minus negative terminal) and $I_{\text{p}}$ the current flowing **into** the positive terminal, i.e. into the DUT output. The probe is a voltage source, so $V_{\text{p}} = 0$ in the DC operating point; in the AC linearization $V_{\text{p}}$ becomes an independent excitation that can be driven freely.

A **local ground** node may be specified (defaulting to the circuit's global ground). All single-ended quantities below are measured with respect to it. The DUT input voltage is

$$V_1 \;=\; V_{\text{p+}} - V_{\text{lg}},$$

where $V_{\text{p+}}$ is the potential at the probe's positive (DUT-input-side) terminal and $V_{\text{lg}}$ at the local ground.

If the probe carries an $m$-factor different from unity (parallel instances), its measured branch current is scaled accordingly so that $I_{\text{p}}$ remains the total current flowing into the DUT output.

## Two excitations

Two complex right-hand sides are imposed per frequency, each producing a full small-signal solution $\tilde X(\omega)$.

### Excitation I: unit current injection, probe shorted

A unit phasor current is injected into the probe's positive node from the local ground, with the probe voltage held at zero ($V_{\text{p}} = 0$). The two measured responses are

$$A \;=\; I_{\text{p}}^{(\text{I})}, \qquad
  C \;=\; V_1^{(\text{I})}.$$

$A$ is therefore the current that flows into the DUT output when a unit current is forced at the DUT input port and the loop is shorted at the probe; $C$ is the resulting voltage at the DUT input port.

### Excitation II: unit voltage at the probe

A unit phasor voltage is imposed across the probe, $V_{\text{p}} = 1$, with no injected current. The two measured responses are

$$B \;=\; I_{\text{p}}^{(\text{II})}, \qquad
  D \;=\; V_1^{(\text{II})}.$$

$B$ is the current that flows into the DUT output when the loop is excited by a unit voltage across the probe; $D$ is the resulting input voltage.

The four numbers $\{A, B, C, D\}$ are functions of frequency only; they fully describe the linearized DUT seen across the probe.

## Admittance parameters

Eliminating the probe-side excitations yields the DUT's two-port $y$-parameters, with side 1 = input and side 2 = output:

$$y_{11} \;=\; \frac{1 + AD - BC - A - D}{C}, \qquad
  y_{12} \;=\; \frac{BC - AD + D}{C},$$

$$y_{21} \;=\; \frac{BC - AD + A}{C}, \qquad
  y_{22} \;=\; \frac{AD - BC}{C}.$$

These are the standard small-signal admittance parameters that would be measured by terminating one port and exciting the other; they are recovered here from a single pair of solves obtained without breaking the loop.

## Open-loop gain

The loop gain seen by the feedback path decomposes into a forward and a reverse component. In terms of the $y$-parameters,

$$W_f \;=\; \frac{y_{21}}{y_{11} + y_{22}}, \qquad
  W_r \;=\; \frac{y_{12}}{y_{11} + y_{22}}, \qquad
  W   \;=\; \frac{y_{21} + y_{12}}{y_{11} + y_{22}}.$$

Substituting the closed-form expressions for the $y$-parameters,

$$W_f \;=\; \frac{BC - AD + A}{1 + 2(AD - BC) - A - D}, \qquad
  W_r \;=\; \frac{BC - AD + D}{1 + 2(AD - BC) - A - D},$$

$$W \;=\; \frac{2(BC - AD) + A + D}{1 + 2(AD - BC) - A - D}.$$

$W_f$ is the gain a forward-traveling signal sees around the loop; $W_r$ is the gain a reverse-traveling signal sees; $W = W_f + W_r$ is their sum. Splitting forward and reverse contributions is what makes this formulation robust for bilateral circuits where the loop is not uni-directional.

## Stability criterion

The Nyquist criterion can be applied to $W(\omega)$ to assess closed-loop stability: the encirclements of $-1$ by the locus of $W(j\omega)$ as $\omega$ sweeps across the relevant range determine the number of right-half-plane closed-loop poles. Provided the open-loop circuit is itself stable (no right-half-plane poles in the broken-loop linearization), the closed-loop system is stable iff $W(j\omega)$ does not encircle $-1$.

For unilateral designs $W_r \approx 0$ and $W \approx W_f$; for the general bilateral case both contributions matter, and using $W$ rather than $W_f$ alone avoids the systematic bias that a one-sided probe injection would introduce.
