# Subcircuits and Hierarchy

Subcircuits let you build circuits from reusable, named blocks. A subcircuit definition groups instances, models, and parameters under a name and a set of terminals. Each time that subcircuit is instantiated the simulator creates a fresh copy of its internal structure and connects the terminals to the surrounding circuit.

## Example

```text
Two-stage RC low-pass filter

load "resistor.osdi"
load "capacitor.osdi"
load "inductor.osdi"

model r resistor
model c capacitor
model l inductor

subckt stage (in out)
parameters r=1k cap=1n lparasitic= 1u
  lpar (in in1) l l=lparasitic
  r1 (in1 out) r r=r
  c1 (out 0) c c=cap
ends

// Instantiate two stages
x1 (vin mid)  stage r=2k cap=500p
x2 (mid vout) stage r=1k cap=1n

vin (vin 0) vsource dc=0 type="sine" ampl=1 freq=1k

control
  analysis ac1 ac start=1k stop=100meg mode="dec" points=20
endc
```

`x1` and `x2` are independent instances of `stage`. Each has its own internal nodes and parameter values. The node `mid` connects the two stages; `vin` and `vout` are the overall circuit terminals.

## Hierarchy

Instances inside a subcircuit are named with the instance path separator `:`. After elaboration the hierarchy of the example above looks like:

```
__topinst__
  x1
    x1:r1
    x1:c1
  x2
    x2:r1
    x2:c1
  vin
```

Internal node `in1` inside `x1` becomes `x1:in1`; inside `x2` it becomes `x2:in1`. The global node `0` is shared.

See [Defining a Subcircuit](cir-subckt.md), [Instance Hierarchy](cir-hier.md), and [Nested Subcircuit Definitions](cir-subckt-nested.md) for detailed reference.
