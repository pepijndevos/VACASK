# Circuit Description and Elaboration

The circuit description is the part of the netlist that defines what the circuit looks like: its nodes, device instances, models, and subcircuits. It is written outside the `control`/`endc` block.

## Structure of a netlist file

A netlist file begins with a title line, followed by the circuit description, and optionally a control block:

```text
Title of the circuit

// Load device models
load "resistor.osdi"
load "spice/bjt.osdi"

// Define models and instances
model r resistor
model q2n2222 sp_bjt type=1 ...

vin (in 0) vsource dc=5
r1 (in base) r r=10k
q1 (out base 0) q2n2222

control
  analysis op1 op
endc
```

## Key concepts

**[Loading devices](cir-loading.md)** — Device modules (OSDI files) are loaded with the `load` directive before they can be used. VACASK can also compile Verilog-A source files on the fly.

**[Nodes](cir-nodes.md)** — Nodes are the connection points of the circuit. Node `0` is the default ground. Additional ground and global nodes can be declared with `ground` and `global`.

**[Masters](cir-masters.md)** — A master is a named template. `model` binds a name to a loaded device type. `subckt`/`ends` defines a reusable subcircuit block.

**[Instances](cir-instance.md)** — An instance places a master into the circuit and connects it to nodes. Instance and model parameters accept arithmetic expressions.

**[Subcircuits and hierarchy](cir-hierarchy.md)** — Subcircuits can be nested and parameterized, enabling hierarchical design.

**[Parallel devices](cir-mfactor.md)** — The `$mfactor` instance parameter scales a device's contribution to represent multiple identical devices in parallel.

**[Conditional blocks](cir-conditional.md)** — `@if`/`@elseif`/`@else`/`@end` blocks include or exclude parts of the netlist based on parameter values, enabling configurable topologies.

**[Circuit elaboration](cir-elaboration.md)** — Elaboration resolves all names, propagates parameters, evaluates conditions, and builds the simulation-ready instance hierarchy. It runs automatically before the first analysis.
