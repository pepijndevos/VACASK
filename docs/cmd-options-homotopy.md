# Homotopy Algorithms

These options control the gmin stepping and source stepping homotopy algorithms used to find an operating point when direct Newton-Raphson iteration fails.

## Gmin stepping

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `homotopy_gminsteps` | int | 100 | >1; 0 = off | Maximum number of gmin stepping steps. |
| `homotopy_startgmin` | real | 1e-3 | >mingmin | Starting gmin value for dynamic gmin stepping. |
| `homotopy_maxgmin` | real | 1e2 | >mingmin | If gmin stepping cannot converge above this gmin level, give up. |
| `homotopy_mingmin` | real | 1e-15 | >0 | Target gmin value; stepping stops when gmin reaches this level. |
| `homotopy_gminfactor` | real | 10.0 | >0 | Initial gmin step reduction factor. |
| `homotopy_maxgminfactor` | real | 10.0 | ≥gminfactor | Maximum gmin step factor. |
| `homotopy_mingminfactor` | real | 1.00005 | 1 < x < gminfactor | Give up when the factor falls below this value. |

## Source stepping

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `homotopy_srcsteps` | int | 100 | >1; 0 = off | Maximum number of source stepping steps. |
| `homotopy_srcstep` | real | 0.01 | >0 | Initial source step size. |
| `homotopy_srcscale` | real | 3.0 | >1 | Step scaling factor: multiply on success, divide on failure. |
| `homotopy_minsrcstep` | real | 1e-7 | 0 < x < srcstep | Source step size at which dynamic source stepping gives up. |
| `homotopy_sourcefactor` | real | 1.0 | any | Manual homotopy source scale factor. Normally 1.0; can be swept for manual source stepping via a DC sweep. |

## Debugging

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `homotopy_debug` | int | 0 | ≥0 | Debug verbosity for homotopy algorithms. 0 disables debug messages. |

- ≥1 = enabled