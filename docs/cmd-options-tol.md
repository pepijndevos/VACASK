# Tolerances

## Tolerance mode, scaling, and relative tolerance

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `tolmode` | string | `spice` | `spice`, `va`, `mixed` | Tolerance assignment strategy. `spice` uses `vntol`/`abstol`/`chgtol`/`fluxtol` for all unknowns. `va` uses Verilog-A natures and disciplines where available and enforces no tolerance where they are absent. `mixed` uses VA natures where available, falling back to SPICE tolerances. |
| `tolscale` | real | 1.0 | >0 | Global scaling factor applied to all absolute tolerances. |
| `reltol`   | real | 1e-3 | 0 < x < 1 | Relative convergence tolerance. |

## Absolute tolerances in SPICE and mixed tolerance mode

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `abstol` | real | 1e-12 | >0 | Absolute current tolerance (A). Used for flow unknowns. |
| `vntol` | real | 1e-6 | >0 | Absolute voltage tolerance (V). Used for potential unknowns. |
| `chgtol` | real | 1e-15 | >0 | Charge tolerance (As). Default corresponds to 1 mV across 1 pF. |
| `fluxtol` | real | 1e-14 | >0 | Flux tolerance (Vs). Default corresponds to 1 µA across 10 nH. |
