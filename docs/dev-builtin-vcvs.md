# Voltage-Controlled Voltage Source

Module name: `vcvs`

A `vcvs` instance enforces the voltage constraint

```text
V(p) − V(n) = gain × (V(cp) − V(cn))
```

It introduces one internal unknown (the branch current) and contributes no reactive terms, so it is purely resistive in the MNA stamp.

## Terminals

Four terminals must be connected: `p n cp cn`.

| Terminal | Role |
|----------|------|
| `p` | Positive output terminal |
| `n` | Negative output terminal |
| `cp` | Positive controlling terminal |
| `cn` | Negative controlling terminal |

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `gain` | real | 1 | Voltage gain (dimensionless). Output voltage = gain × controlling voltage. |
| `$mfactor` | real | 1 | Number of parallel instances. |

## Output variables

| Variable | Description |
|----------|-------------|
| `v` | Output voltage V(p) − V(n) |
| `ctl` | Controlling voltage V(cp) − V(cn) |
| `i` | Branch current through one parallel instance, positive when flowing into terminal `p` |

## Example

```text
Voltage-controlled voltage source

ground 0
load "resistor.osdi"

model resistor resistor
model vsource vsource
model vcvs vcvs

vin (in 0) vsource dc=1
rin (in 0) resistor r=1k

e1 (out 0 in 0) vcvs gain=10
rload (out 0) resistor r=1k

control
  abort always
  analysis op1 op
endc
```

`rin` sets the input impedance. `e1` amplifies the input voltage by a factor of 10 so V(out) = 10 V. Because `vcvs` is ideal the controlling terminals draw no current.
