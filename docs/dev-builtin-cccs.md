# Current-Controlled Current Source

Module name: `cccs`

A `cccs` instance drives a current proportional to the branch current of another instance:

```text
I(p→n, through device) = gain × I(ctlinst)
```

It introduces no internal unknowns and contributes no reactive terms, so it is purely resistive in the MNA stamp.

## Terminals

Two terminals must be connected: `p n`. The controlling current is taken from another instance in the circuit, not through additional terminals.

| Terminal | Role |
|----------|------|
| `p` | Positive output terminal |
| `n` | Negative output terminal |

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `gain` | real | 1 | Current gain (dimensionless). Output current = gain × controlling current. |
| `ctlinst` | string | — | Name of the instance that provides the controlling current. Required. |
| `ctlnode` | string | `flow(br)` | Internal node of the controlling instance whose unknown is used as the controlling current. The default reads the branch current of a voltage source. |
| `$mfactor` | real | 1 | Number of parallel instances. |

## Output variables

| Variable | Description |
|----------|-------------|
| `v` | Output voltage V(p) − V(n) |
| `ctl` | Controlling current (value of the controlling node unknown) |
| `i` | Current through one parallel instance, positive when flowing into terminal `p` |

## Example

```text
Current-controlled current source

ground 0
load "resistor.osdi"

model resistor resistor
model vsource vsource
model cccs cccs

vin (in 0) vsource dc=1
vprobe (in in1) vsource dc=0
r1 (in1 0) resistor r=1k

f1 (0 out) cccs ctlinst="vprobe" gain=10
rload (out 0) resistor r=1k

control
  abort always
  analysis op1 op
endc
```

`vprobe` is a zero-volt voltage source that acts as a current probe inserted in series with `r1`. `f1` mirrors the 1 mA current flowing through `vprobe` with a gain of 10, driving 10 mA through `rload` and setting V(out) = 10 V.
