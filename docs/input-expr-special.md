# Special Identifiers

Special identifiers are predefined read-only names that reflect simulator state. They are available in all parameter expressions — instance parameters, model parameters, and subcircuit parameters.

| Identifier | Maps to option | Default | Description |
|------------|----------------|---------|-------------|
| `$temp` | `temp` | `27` | Ambient temperature (°C). |
| `$scale` | `scale` | `1.0` | Global instance length scaling factor. |

Both identifiers track their corresponding simulator option. When `temp` or `scale` is changed with the `options` command, all parameterized expressions that reference `$temp` or `$scale` are re-evaluated.

## Example

```text
model r resistor r=1k*(1 + 3.9e-3*($temp - 27))   // 1 kΩ with TCR 3900 ppm/°C
```

```text
control
  options temp=125
  analysis op1 op           // r evaluates at 125 °C
endc
```

See [Setting Simulator Options](cmd-options-set.md) for how to set `temp` and `scale`.
