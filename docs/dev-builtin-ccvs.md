# Current-Controlled Voltage Source

Module name: `ccvs`

A `ccvs` instance enforces the voltage constraint

```text
V(p) − V(n) = gain × I(ctlinst)
```

where `I(ctlinst)` is the branch current of the controlling instance. It introduces one internal unknown (the branch current) and contributes no reactive terms, so it is purely resistive in the MNA stamp.

## Terminals

Two terminals must be connected: `p n`. The controlling current is taken from another instance in the circuit, not through additional terminals.

| Terminal | Role |
|----------|------|
| `p` | Positive output terminal |
| `n` | Negative output terminal |

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `gain` | real | 1 | Transresistance in ohms. Output voltage = gain × controlling current. |
| `ctlinst` | string | — | Name of the instance that provides the controlling current. Required. |
| `ctlnode` | string | `"flow(br)"` | Internal node of the controlling instance whose unknown is used as the controlling current. The default reads the branch current of a voltage source. |
| `$mfactor` | real | 1 | Number of parallel instances. |

## Output variables

| Variable | Description |
|----------|-------------|
| `v` | Output voltage V(p) − V(n) |
| `ctl` | Controlling current (value of the controlling node unknown) |
| `i` | Branch current through one parallel instance, positive when flowing into terminal `p` |

## Example

```text
Current-controlled voltage source

ground 0
load "resistor.osdi"

model resistor resistor
model vsource vsource
model ccvs ccvs

vin (in 0) vsource dc=1
rsense (in 0) resistor r=1k

h1 (out 0) ccvs ctlinst="vin" gain=5k
rload (out 0) resistor r=1k

control
  abort always
  analysis op1 op
endc
```

`rsense` carries 1 mA. `h1` senses that current through `vin` (positive current flows into the positive terminal) and produces V(out) = 5k × -1 mA = -5 V. Use a `vsource` with zero DC value in series as a current probe, or use the Verilog-A inductor model whose branch current is available via `flow(br)`.
