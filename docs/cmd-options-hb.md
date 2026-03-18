# Harmonic Balance Options

## Iteration limits

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `hb_itl` | int | 100 | >0 | Maximum iterations in non-continuation mode. |
| `hb_itlcont` | int | 50 | >0 | Maximum iterations per continuation step. |

## Homotopy control

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `hb_skipinitial` | boolean | 1 | 0, 1 | Skip the initial direct solve and go straight to homotopy. |
| `hb_homotopy` | list | `["src"]` | — | Ordered list of homotopy algorithms to try when the initial solve fails. |

## Debugging

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `hb_debug` | int | 0 | ≥0 | Debug verbosity. 0 disables debug messages. |

- ≥1 = iteration type, homotopy info, convergence report, 
- ≥2 = continuation mode details, 
- ≥3 = spectrum construction.