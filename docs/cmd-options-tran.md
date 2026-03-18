# Transient Analysis Options

## Integration method

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `tran_method` | string | `trap` | `am`, `bdf`, `gear`, `euler`, `trap`, `am2`, `gear2`, `bdf2` | Numerical integration algorithm.  |
| `tran_maxord` | int | 2 | — | Maximum integration order for variable-order `am` and `bdf`/`gear` methods. |
| `tran_xmu` | real | 0.5 | 0–0.5 | Euler/trapezoidal mixture for Adams-Moulton order 2. 0 = pure Euler, 0.5 = pure trapezoidal. |
| `tran_trapltefilter` | boolean | 1 | 0, 1 | Enable trap ringing filter for predictor and LTE computation (AM order 2 only). |
| `tran_spicelte` | boolean | 0 | 0, 1 | Use SPICE-compatible (incorrect) LTE handling. |

- `am` = Adams-Moulton (AM),  
- `bdf`/`gear` = backward differentiation (Gear, BDF), 
- `euler` forces AM order 1, 
- `trap`/`am2` force AM order 2, 
- `gear2`/`bdf2` force BDF order 2

## Timestep control

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `tran_fs` | real | 0.25 | 0 < x ≤ 0.5 | Fraction of the simulation interval used as the initial timestep, and as the maximum step between consecutive breakpoints. |
| `tran_ffmax` | real | 0.25 | ≥0 | Limits the initial timestep to this fraction of the maximum excitation period. 0 disables. |
| `tran_fbr` | real | 0.2501 | 0 < x ≤ 1/3 | Maximum fraction of a breakpoint interval for the timestep. Must be ≤ 1/3 to guarantee at least three points between any two breakpoints. |
| `tran_rmax` | real | 0.0 | — | Upper limit on timestep expressed as a ratio to the simulation step. Values < 1 disable this limit. |
| `tran_minpts` | int | 50 | — | Minimum number of output timepoints from start to stop, excluding the start point. Values < 1 disable. |

## Newton-Raphson per timepoint

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `tran_itl` | int | 10 | >0 | Maximum NR iterations per timepoint. |
| `tran_ft` | real | 0.25 | 0 < x < 1 | Timestep reduction factor applied when `tran_itl` is exceeded. |
| `tran_predictor` | boolean | 0 | 0, 1 | 1 = use predictor to compute the initial NR guess; 0 = use the previous solution. |

## LTE-based step rejection

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `tran_redofactor` | real | 2.5 | ≥0 | Reject a timepoint if the ratio of the actual timestep to the LTE-derived optimal timestep exceeds this value. 0 disables LTE-based rejection. |
| `tran_lteratio` | real | 3.5 | >1 | LTE overestimation factor. Larger values produce a looser (more permissive) LTE tolerance. |

## Debugging

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `tran_debug` | int | 0 | ≥0 | Debug verbosity. 0 disables debug messages. |

- ≥1 = step control messages, 
- ≥2 = solver details