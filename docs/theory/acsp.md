# AC S-Parameter Analysis

AC S-parameter analysis computes the small-signal scattering parameters of a multi-port network as functions of frequency. Each port is realized by a voltage source in series with a reference-impedance resistor; the analysis excites the ports one at a time and extracts incident and reflected power waves at every port.

## Overview

About a DC operating point $x^*$ the circuit is linearized as in [AC analysis](ac.md):

$$[J_r(x^*) + j\omega\, J_c(x^*)]\,\tilde X = \Delta U.$$

For an $N$-port network the analysis solves $N$ such systems per frequency, each with a different excitation pattern (one port driven, others left at their idle excitation), reads off port voltages and currents, forms the incident and reflected wave matrices $A$ and $B$, and recovers the scattering matrix $S$ from

$$B \;=\; S\,A \quad\Longleftrightarrow\quad S \;=\; B\,A^{-1}.$$

Frequency is swept across a user-specified range as in [AC analysis](ac.md). The operating point and the frozen Jacobians at $x^*$ are computed once; per frequency the cost is one reactive load, one factorization of $J_r + j\omega J_c$, and $N$ right-hand-side solves.

## Port definition

- The voltage source's **positive** terminal is shared with one terminal of the resistor.
- The resistor's **other** terminal is the **positive port terminal** at the interface plane.
- The voltage source's **negative** terminal is the **negative port terminal** at the interface plane.

The series resistor sets the **characteristic (reference) impedance** of the port,

$$Z_{0,k} \;=\; \frac{R_k}{m_k},$$

where $R_k$ is the resistor's `r` parameter and $m_k$ is its `$mfactor` (parallel-instance multiplier). The resistor model must expose the `r`, `$mfactor`, and `noisy` parameters and must have two terminals; the source model must be a voltage source. Reference impedances may differ from port to port.

Ports are listed in the analysis parameter `ports` as a flat sequence of (source, resistor) instance names; the number of ports is therefore half the length of that list. Port indices follow that listing order, starting at $1$.

## Per-port excitation

For each port $i \in \{1, \ldots, N\}$ the analysis builds an excitation that drives only the $i$-th port's voltage source (with that source's scaled unit excitation) and leaves the others at zero. Solving the AC system at frequency $\omega$ yields a full phasor solution $\tilde X^{(i)}(\omega)$.

At every port $j$ the **interface-plane voltage** and the **into-port current** are then read off:

$$V_j^{(i)} \;=\; V_{\text{port+},j}^{(i)} \;-\; V_{\text{port-},j}^{(i)},$$

$$I_j^{(i)} \;=\; -\,I_{\text{src},j}^{(i)} \cdot m_j,$$

where $I_{\text{src},j}^{(i)}$ is the branch current of the $j$-th port's voltage source (positive when flowing into the source's positive terminal). The negation converts source-branch current into current flowing **into** the port at the interface plane, and the $m_j$ scaling accounts for the source's `$mfactor` so that $I_j^{(i)}$ is the total current of $m_j$ parallel instances.

## Incident and reflected waves

The standard real-reference-impedance power waves are formed at each port:

$$a_j^{(i)} \;=\; \frac{V_j^{(i)} + Z_{0,j}\, I_j^{(i)}}{2\sqrt{Z_{0,j}}}, \qquad
  b_j^{(i)} \;=\; \frac{V_j^{(i)} - Z_{0,j}\, I_j^{(i)}}{2\sqrt{Z_{0,j}}}.$$

With this normalization $|a_j^{(i)}|^2$ is the incident power at port $j$, $|b_j^{(i)}|^2$ is the reflected power, and the net power delivered to the port is $|a_j^{(i)}|^2 - |b_j^{(i)}|^2$.

Collect the waves into two $N \times N$ matrices indexed by *(observation port, excitation port)*:

$$A_{ji} \;=\; a_j^{(i)}, \qquad B_{ji} \;=\; b_j^{(i)}.$$

Rows are the port where the wave is observed; columns are the port driven by the excitation.

## Solving for $S$

By definition the scattering matrix maps incident to reflected waves at all ports simultaneously:

$$B \;=\; S\,A.$$

The analysis stores transposed copies $A^T, B^T$ during accumulation (each excitation fills a row), so the system solved is

$$A^T\, S^T \;=\; B^T,$$

a dense $N \times N$ linear system per frequency. 
