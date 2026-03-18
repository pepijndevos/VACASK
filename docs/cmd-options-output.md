# Output and Sweep Options

## Raw file output

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `rawfile` | string | `binary` | `ascii`, `binary` | Raw file format. `binary` is more compact; `ascii` is human-readable. |
| `strictoutput` | int | 2 | 0, 1, 2 | Raw file cleanup on error. 0 = leave files in place. 1 = delete on error. 2 = delete before analysis starts. |

## Save binding

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `strictsave` | int | 1 | 0, 1, 2 | Behavior when a save directive cannot be bound. 0 = silently bind to constant 0. 1 = error if the first binding attempt fails; later failures bind to 0. 2 = error on every failed binding. |

## Sweep

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `sweep_pointmarker` | boolean | 0 | 0, 1 | Enable sweep point synchronization marker for co-simulation. When 1, the simulator pauses before each sweep point and returns `SweepPoint`; the digital co-simulator should then reset its state before calling `resume()`. |
| `sweep_debug` | int | 0 | ≥0 | Debug verbosity for the sweep engine. 0 disables debug messages. |

- ≥1 = basic debugging (TODO)
- ≥2 = print details (TODO)
