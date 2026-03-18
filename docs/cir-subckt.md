# Defining a Subcircuit

A subcircuit is a reusable, named circuit block. It encapsulates nodes, device instances, models, and parameters behind a fixed set of terminals. Instances of a subcircuit connect to it through those terminals and may override its parameters.

## Syntax

```text
subckt name (terminal1 terminal2 ...)
[parameters p1=expr1 p2=expr2 ...]
[models, instances, conditional blocks, nested subcircuit definitions]
ends
```

The terminal list names the external connection points in the order that instance node lists must follow. There is no limit on the number of terminals.

The `ends` keyword closes the definition. Optionally the subcircuit name may follow `ends` for readability, but it has no effect.

## Parameters

Parameters declared inside a subcircuit are local to it. Each parameter must have amn expression. If the expression is a constant or can be evaluated to a constant during parsing it is the default value of the parameter and an instance can override the default value of the parameter. Such parameters are primary parameters:

```text
subckt lp_filter (in out)
parameters r=1k c=1n
  r1 (in mid) resistor r=r
  c1 (mid out) capacitor c=c
ends

// Instance with default values
x1 (sig filt) lp_filter

// Instance with overridden values
x2 (sig filt2) lp_filter r=10k c=100p
```

## Dependent parameters

A parameter whose expression references parameters declared earlier or circuit variables is a dependent parameter. This lets you derive secondary values from a small set of primary inputs:

```text
subckt amp (in out)
parameters gm=10m ro=10k
parameters av=gm*ro          // av = 100
parameters rin=av/gm         // rin = 10k (av already computed)
  ...
ends
```

Parameters with default values (right-hand side is a literal value or can be computed during parsing) are all committed to the context before any dependent parameters are evaluated, regardless of declaration order. Dependent parameters are then evaluated in declaration order; each one sees the fully resolved values of all constants and all dependent parameters declared before it.

An instance-supplied override for a primary parameter automatically propagates to derived parameters that depend on it:

```text
// x1 uses gm=20m; av and rin are recomputed from the new gm
x1 (sig out) amp gm=20m
```

When creating a subcircuit instance you cannot override the values of dependent parameters. 

## Internal nodes

Nodes that appear inside a subcircuit but are not listed as terminals and are not global/ground nodes are internal nodes. They are invisible outside the subcircuit and are created fresh for each instance. 

```text
...
global vdd vss

subckt and in1 in2 out
  // Global nodes  : vdd, vss
  // Terminals     : in1, in2, out
  // Internal nodes: n1, out1 
  // NAND gate
  mp1 (out1 in1 vdd vdd) pmos w=2u l=0.18u
  mp2 (out1 in2 vdd vdd) pmos w=2u l=0.18u
  mn1 (out1 in1 n1  vss) nmos w=1u l=0.18u
  mn2 (n1   in2 vss vss) nmos w=1u l=0.18u
  // Inverter
  mp3 (out out1 vdd vdd) pmos w=2u l=0.18u
  mn3 (out out1 vss vss) nmos w=1u l=0.18u
ends
```

## Defaut top-level circuit

Instances, models, and parameters defined outside `subckt`/`ends` blocks constitute the default top-level circuit. The default top-level circuit has no terminals. This is where the circuit elaboration starts by default - by creating an instance of the default top-level circuit. 

```text
Sample circuit

load "spice/resistor.osdi"
load "spice/bjt.osdi"

// These models are part of the top-level circuit definition
model vsrc vsource
model resistor resistor

// These instances are part of the top-level circuit definition
vdd (vp 0) vsrc dc=5
vin (exc 0) vsrc dc=0
rl (resp 0) resistor r=1k
xamp (exc resp vp 0) amplifier

subckt amplifier in out vcc vee
  // These models and instances are part of the amplifier subcircuit definition
  model t2n2222 sp_bjt type=1 ...
  q1 (out in vee) t2n2222
  rc (vcc out) resistor r=1k
ends
```

## Allowed contents

Inside a subcircuit definition you may use:

- `parameters` declarations
- Instance declarations
- Model declarations
- Nested subcircuit definitions
- Conditional blocks (`@if` ... `@end`)

The following are allowed only at the top level and are errors inside a subcircuit:

- `global` and `ground` directives
- `load` and `embed` directives
- The `control` block
