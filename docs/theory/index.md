# VACASK Theory Documentation

Working notes on the mathematical and algorithmic foundations of VACASK.

## Contents

1. [Equations](equations.md) — DAE formulation and MNA system description
2. [Operating Point Analysis](op.md) — DC solution and Newton-Raphson solver with damping and limiting
3. [Incremental DC Analysis](dcinc.md) - DC solution change as a response to a small perturbation in excitation
4. [DC Transfer Function Analysis](dcxf.md) — DC transfer function from all independent sources to an output, adjoint system
5. [AC Analysis](ac.md) — Small-signal linearization, phasors, and frequency-domain response
6. [AC Transfer Function Analysis](acxf.md) — AC transfer function from all independent sources to an output
7. [AC Stability Analysis](acstb.md) — Probe-based loop breaking and open-loop gain
8. [AC S-Parameter Analysis](acsp.md) — Multi-port small-signal scattering parameters from per-port excitations
9. [AC Noise Analysis](acnoise.md) — Per-source and per-instance small-signal noise contributions and output PSD
10. Transient Analysis
    1. [Numerical Integration](numint.md) — Linear multistep ansatz, order conditions, and step-size sensitivities
    2. Local Truncation Error
    3. Transient Solver
11. Transient Noise Analysis
12. Harmonic Balance Analysis
13. Harmonic Balance-Based (Quasi)periodic AC Analysis
14. [Periodic Steady-State Analysis](pss.md) — Shooting Newton method
