# Temperature, Scale, and Conductances

## Temperature

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `temp` | real | 27 | &ge;-273.15$ | Ambient temperature (°C). Exposed as `$temp`; re-evaluates all instance and model parameters that reference `$temp`. |
| `tnom` | real | 27 | &ge;-273.15 | Device parameter measurement temperature (°C). Used as the reference temperature for model parameter extraction. |

## Scale

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `scale` | real | 1.0 | >0 | Global device scale factor. Exposed as `$scale`; re-evaluates all expressions that reference `$scale`. |

## Conductances

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `gmin` | real | 1e-12 | ≥0 | Minimum conductance added across every p-n junction to aid convergence. |
| `gshunt` | real | 0.0 | ≥0 | Shunt conductance from every potential node to ground. 0 disables. |
| `minr` | real | 0.0 | ≥0 | Minimum resistance. Resistors smaller than this value are clamped to `minr`. 0 disables. |
