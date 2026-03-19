# Voltage-Controlled Current Source

Module name: `vccs`

A `vccs` instance drives a current proportional to a controlling voltage:

```text
I(p→n, through device) = gain × (V(cp) − V(cn))
```

It introduces no internal unknowns and contributes no reactive terms, so it is purely resistive in the MNA stamp.

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
| `gain` | real | 1 | Transconductance in siemens. Output current = gain × controlling voltage. |
| `$mfactor` | real | 1 | Number of parallel instances. |

## Output variables

| Variable | Description |
|----------|-------------|
| `v` | Output voltage V(p) − V(n) |
| `ctl` | Controlling voltage V(cp) − V(cn) |
| `i` | Current through one parallel instance, positive when flowing into terminal `p` |

## Example

```text
Voltage-controlled current source

ground 0
load "resistor.osdi"

model resistor resistor
model vsource vsource
model vccs vccs

vin (in 0) vsource dc=1
rin (in 0) resistor r=1k

g1 (0 out in 0) vccs gain=2m
rload (out 0) resistor r=1k

control
  abort always
  analysis op1 op
endc
```

`vin` sets V(in) = 1 V. `g1` drives a current of 2 mA from `p` to `n` through the device, which flows through `rload` and sets V(out) = 2 V. Because `vccs` is ideal the controlling terminals draw no current.
