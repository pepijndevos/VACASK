# Instances

An instance places a master (model or subcircuit) into the circuit and connects it to nodes.

## Syntax

```text
name (node1 node2 ...) master [parameters]
```

- `name` — unique identifier for the instance within its scope.
- `(node1 node2 ...)` — node connections in the order defined by the master's terminal list.
- `master` — name of the model or subcircuit to instantiate.
- `parameters` — optional. 

## Parameter override order

Builtin devices have strictly separated instance and model parameters. Model parameters can be specified only in model definitions, while instance parameters can be specified only in instance definition. 

OSDI devices expose model and instance parameters. Model parameters can be specified 
only in a model definition. Instance parameters can be specified in both model 
definitions and instance definitions. For a particular instance a parameter specified 
in an instance definition overrides the parameter specified in the corresponding model 
definition. 

Subcircuits can be parameterized. Each subcircuit parameter has a default value specified in the subcircuit definition. If that parameter is specified for a subcircuit instance the instance value overrides the default value. 

## Unconnected terminal handling 

For builtin devices you must specify a node for each of its teminals. Failure to do so resuilts in an error. 

For OSDI devices you don't have to specify all nodes. Unconnected terminals are connected to internal nodes. 

For subcircuits you don't have to specify all nodes. Unconnected terminals are connected to internal nodes. 

## Examples

```text
v1 (in 0) vsource dc=5 type="sine" ampl=1 freq=1k
r1 (in out) resistor r=1k
c1 (out 0) capacitor c=100n
x1 (a b c) mysubckt gain=2
```

Subcircuit instances conventionally use the `x` prefix but this is not required by the simulator.

## Parameterized expressions

Instance parameters can be specified with arithmetic expressions. Expressions can reference parameters defined with the `parameters` keyword in the enclosing scope, constants, and circuit variables:

```text
parameters vcc=3.3 rin=1k

v1 (vdd 0) vsource dc=vcc
r1 (vdd out) resistor r=rin/2
```
