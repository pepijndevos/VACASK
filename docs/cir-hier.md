# Instance Hierarchy

## Hierarchical names of nodes, instances, and models

Every instance, internal node, and locally defined model is given a hierarchical name that reflects its position in the instance tree. These names are used for referring to particular models, instances, and internal nodes. 

The separator between levels is `:`. toplevel instances have no prefix; instances inside a subcircuit instance are prefixed with that instance's name:

| Path | Meaning |
|------|---------|
| `rtop` | toplevel instance `rtop` |
| `x1` | toplevel instance `x1` |
| `x1:r1` | Instance `r1` inside instance `x1` |
| `x1:xa:r1` | Instance `r1` inside `xa`, which is inside `x1` |

Internal nodes and models defined inside a subcircuit definition follow the same rule: a node named `mid` inside instance `x1` is referred to as `x1:mid`. A model named `bjtmod` inside instance `x1` is referred to as `x1:bjtmod`.

Global and ground nodes (declared with `global` or `ground`) are never prefixed — their name is the same everywhere in the hierarchy. 

Nodes for terminals are inherited from the enclosing instances or connected to global/ground nodes according to the list specified when the subcircuit instance is created. 

For the naming rules that apply to nested subcircuit definitions see [Nested Subcircuit Definitions](cir-subckt-nested.md).

### Output variables

Output variables of an instance (currents, charges, and device-specific quantities exposed by a device model) are referred to as `instance.outvar`, for example `x1:m1.id` for the drain current of device `m1` inside instance `x1`.

## Scope

A subcircuit has its own scope for models. When resolving a model name, the simulator searches the local scope, followed by the enclosing toplevel scope. A master defined in a the local scope shadows a same-named master from the toplevel scope.

Parameter expressions inside a subcircuit are resolved first in the subcircuit's own scope followed by the enclosing toplevel scope. 

When VACASK encounters an identifier in a parameterized expression it searches for a value in the following order

- the subcircuit's own scope 
- the toplevel scope 
- circuit variables 
- constants 
- special identifiers (see [Special Identifiers](expr-special.md))

A parameter defined in a lower scope shadows a same-named parameter from the toplevel scope. 
