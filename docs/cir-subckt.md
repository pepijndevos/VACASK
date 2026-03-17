# Defining a Subcircuit

A subcircuit is a reusable, named circuit block. It encapsulates nodes, device instances, models, and parameters behind a fixed set of terminals. Instances of a subcircuit connect to it through those terminals and may override its parameters.

## Syntax

```text
subckt name (terminal1 terminal2 ...)
[parameters p1=default1 p2=default2 ...]
[models, instances, conditional blocks, nested subcircuit definitions]
ends
```

The terminal list names the external connection points in the order that instance node lists must follow. There is no limit on the number of terminals.

The `ends` keyword closes the definition. Optionally the subcircuit name may follow `ends` for readability, but it has no effect.

## Parameters

Parameters declared inside a subcircuit are local to it. Each parameter must have a default value. An instance can override any subset of the parameters:

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

A parameter whose default is an expression may reference parameters declared earlier
in the same `parameters` block. This lets you derive secondary values from a small
set of primary inputs:

```text
subckt amp (in out)
parameters gm=10m ro=10k
parameters av=gm*ro          // av = 100
parameters rin=av/gm         // rin = 10k (av already computed)
  ...
ends
```

Constant parameters (right-hand side is a literal value or can be computed from builtin constants) are all committed to the context before any dependent parameters are evaluated, regardless of declaration order. Dependent parameters are then evaluated in declaration order; each one sees the fully resolved values of all constants and all expression parameters declared before it.

An instance-supplied override for a primary parameter automatically propagates to
derived parameters that depend on it:

```text
// x1 uses gm=20m; av and rin are recomputed from the new gm
x1 (sig out) amp gm=20m
```

When creating a subcircuit instance you cannot override the values of dependent parameters. 

## Internal nodes

Nodes that appear inside a subcircuit but are not listed as terminals and are not global/ground nodes are internal nodes. They are invisible outside the subcircuit and are created fresh for each instance.

## Defaut top-level circuit

Instances, models, and parameters defined outside `subckt`/`ends` blocks constitute the default top-level circuit. The default top-level circuit has no terminals. This is where the circuit elaboration starts by default - by instantiating the default top-level circuit. 

## Hierarchical names of nodes, instances, and models

Every instance, internal node, and locally defined model is given a hierarchical name that reflects its position in the instance tree. These names are used in save directives, `alter`, `print hierarchy`, and output file column headers.

The separator between levels is `:`. Top-level instances have no prefix; instances inside a subcircuit instance are prefixed with that instance's name:

| Path | Meaning |
|------|---------|
| `rtop` | Top-level instance `rtop` |
| `x1` | Top-level instance `x1` |
| `x1:r1` | Instance `r1` inside instance `x1` |
| `x1:xa:r1` | Instance `r1` inside `xa`, which is inside `x1` |

Internal nodes and models defined inside a subcircuit definition follow the same rule: a node named `mid` inside instance `x1` is referred to as `x1:mid`.

Global and ground nodes (declared with `global` or `ground`) are never prefixed — their name is the same everywhere in the hierarchy.

For the naming rules that apply to nested subcircuit definitions see [Nested Subcircuit Definitions](cir-subckt-nested.md).

### Output variables

Output variables of an instance (currents, charges, and device-specific quantities exposed by a device model) are referred to as `instance.outvar`, for example `x1:m1.id` for the drain current of device `m1` inside instance `x1`.

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

## Scope

A subcircuit has its own scope for models and nested subcircuit definitions. When resolving a master name, the simulator searches the local scope first, then the enclosing top-level scope, and finally the scope of the top-level circuit definition. A master defined in a lower scope shadows a same-named master from a higher scope.

Parameter expressions inside a subcircuit are resolved first in the subcircuit's own scope followed by the parameters defined at the enclosing top-level scope, and finally the scope of the top-level circuit definition. A parameter defined in a lower scope shadows a same-named parameter from a higher scope.
