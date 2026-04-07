# Circuit Analyses

VACASK supports a range of circuit analyses, from DC operating point to frequency-domain small-signal and time-domain transient simulation. Each analysis is requested by an `analysis` statement in the control block and writes its results to a SPICE raw file.

## Circuit equation formulation

VACASK formulates circuit equations in the form

$$g(x) + \frac{d}{dt} q(x) = 0$$

where $x$ is the vector of circuit unknowns (node voltages and branch currents), $g(x)$ is the resistive residual (conductances, controlled sources, nonlinear device currents), and $q(x)$ is the reactive residual (charges and fluxes stored in capacitors and inductors). Each analysis derives its equations from this common formulation:

- **Operating point** sets $\frac{d}{dt} q = 0$ and solves $g(x) = 0$.
- **Transient** approximates $\frac{d}{dt} q$ with a numerical integration formula and solves the resulting algebraic system at each timestep.
- **Harmonic balance** assumes $x(t)$ is almost periodic and solves for its complex spectrum directly in the frequency domain.
- **DC and AC Small-signal analyses** linearize $g$ and $q$ at the operating point to obtain the resistive and reactive Jacobians $J_r$ and $J_c$.

The Jacobian matrices are defined as

$$J_r = \frac{\partial g}{\partial x}\bigg|_{x_0}, \qquad J_c = \frac{\partial q}{\partial x}\bigg|_{x_0}$$

where $x_0$ is the operating point. $J_r$ captures resistive behavior (conductances, transconductances) and $J_c$ captures reactive behavior (capacitances, inductances). Together they form the small-signal model used by AC, noise, and transfer function analyses.

## Available analyses

| Analysis | Type keyword | Description |
|----------|-------------|-------------|
| [Operating Point](cmd-analysis-op.md) | `op` | Computes the DC steady-state solution. |
| [DC Small-Signal](cmd-analysis-dcinc.md) | `dcinc` | Linearizes the circuit at the operating point and computes small-signal node voltages and branch currents for a given excitation. |
| [DC Small-Signal Transfer Function](cmd-analysis-dcxf.md) | `dcxf` | Computes DC small-signal transfer functions, input and output impedances. |
| [AC Small-Signal](cmd-analysis-ac.md) | `ac` | Sweeps frequency and computes the small-signal response at each point. |
| [AC Small-Signal Transfer Function](cmd-analysis-acxf.md) | `acxf` | Computes AC small-signal transfer functions, input and output impedances as a function of frequency. |
| [AC Small-Signal Stability](cmd-analysis-acstb.md) | `acstb` | Computes AC open-loop gain of a feedback circuit as a function of frequency. |
| [AC Small-Signal S-Parameter](cmd-analysis-acsp.md) | `acsp` | Computes small-signal S-parameters of a multi-port circuit as a function of frequency. |
| [Small-Signal Noise](cmd-analysis-noise.md) | `noise` | Computes small-signal noise spectral densities referred to a chosen output or input. |
| [Transient](cmd-analysis-tran.md) | `tran` | Integrates the circuit equations over time. |
| [Harmonic Balance](cmd-analysis-hb.md) | `hb` | Computes the periodic steady-state response in the frequency domain. |
