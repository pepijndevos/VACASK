# Circuit Analyses

VACASK supports a range of circuit analyses, from DC operating point to frequency-domain small-signal and time-domain transient simulation. Each analysis is requested by an `analysis` statement in the control block and writes its results to a SPICE raw file.

## Available analyses

| Analysis | Type keyword | Description |
|----------|-------------|-------------|
| [Operating Point](cmd-analysis-op.md) | `op` | Computes the DC steady-state solution. |
| [DC Small-Signal](cmd-analysis-dcinc.md) | `dcinc` | Linearizes the circuit at the operating point and computes small-signal node voltages and branch currents for a given excitation. |
| [DC Small-Signal Transfer Function](cmd-analysis-dcxf.md) | `dcxf` | Computes DC small-signal transfer functions, input and output impedances. |
| [AC Small-Signal](cmd-analysis-ac.md) | `ac` | Sweeps frequency and computes the small-signal response at each point. |
| [AC Small-Signal Transfer Function](cmd-analysis-acxf.md) | `acxf` | Computes AC small-signal transfer functions, input and output impedances as a function of frequency. |
| [Small-Signal Noise](cmd-analysis-noise.md) | `noise` | Computes noise spectral densities referred to a chosen output or input. |
| [Transient](cmd-analysis-tran.md) | `tran` | Integrates the circuit equations over time. |
| [Harmonic Balance](cmd-analysis-hb.md) | `hb` | Computes the periodic steady-state response in the frequency domain. |
