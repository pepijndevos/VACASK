# Small-Signal Analysis Options

These options apply to all small-signal analyses: `dcinc`, `dcxf`, `ac`, `acxf`, and `noise`.

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `smsig_debug` | int | 0 | ≥0 | Debug verbosity. 0 disables debug messages. |

- ≥1 = general debug output, 
- ≥2 = instance being processed,
- ≥3 = solve failures,
- ≥100 = print the linear system, 
- ≥101 = print the matrix before matrix checks are applied