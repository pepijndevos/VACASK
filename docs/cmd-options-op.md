# Operating Point Options

## Iteration limits

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `op_itl` | int | 100 | >0 | Maximum NR iterations before declaring failure in non-continuation mode. |
| `op_itlcont` | int | 50 | >0 | Maximum NR iterations per step in continuation mode. |

## Homotopy control

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `op_skipinitial` | boolean | 0 | 0, 1 | Skip the initial direct NR solve and go straight to homotopy. |
| `op_homotopy` | list | `["gdev","gshunt","src"]` | — | Ordered list of homotopy algorithms to try when the initial solve fails. |
| `op_srchomotopy` | list | `["gdev","gshunt"]` | — | Homotopy sequence used when source stepping itself fails at source factor = 0. |

## Nodesets

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `op_nsiter` | int | 1 | ≥0 | Number of NR iterations during which nodeset constraints are enforced. |

## Debugging

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `op_debug` | int | 0 | ≥0 | Debug verbosity. 0 disables debug messages. |

- ≥1 = iteration type, homotopy info, convergence report, 
- ≥2 = continuation mode details, 
- ≥3 = evaluation error/abort reporting
