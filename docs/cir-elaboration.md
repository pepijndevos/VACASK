# Circuit Elaboration

Elaboration builds the simulation-ready instance hierarchy from the parsed netlist. It resolves master names, propagates parameters, evaluates conditional blocks, and allocates nodes and unknowns. Elaboration must complete before any analysis can run.

## Default elaboration

Default elaboration is triggered automatically the first time an analysis, `print`, or `alter` command is encountered. It can also be requested explicitly in the control block:

```text
elaborate circuit
```

This instantiates the default toplevel circuit definition (named `__topdef__` internally) as a single toplevel instance (`__topinst__`). All instances, models, and parameters defined outside any `subckt`/`ends` block belong to `__topdef__`. 

The names of toplevel instances, models, and nodes are not prefixed with the enclosing subcircuit instance name (i.e. `__topinst__`). 

The names of the subcircuit definitions in the toplevel circuit are not prefixed with the enclosing subcircuit definition name (i.e. `__topdef__`). 

## Elaboration with additional toplevel subcircuits

One or more named subcircuits can be promoted to toplevel status alongside the default toplevel circuit by issuing the following command in the control block:

```text
elaborate circuit("sub1", "sub2")
```

Each named subcircuit is instantiated as an additional toplevel instance. Its locally defined models and nested subcircuit definitions are promoted to the global scope, behaving as if they had been defined at the toplevel. This is useful for simulating multiple testbenches or circuit variants in a single session without reloading the netlist.

When a subcircuit named `sub` is promoted its internal definition name becomes `__topdef__(sub)` and the corresponding instance is named `__topinst__(sub)`. 

When searching for parameters referenced in the hierarchies of these additional toplevel instances VACASK uses the following search order: 

- the enclosing subcircuit's own scope 
- the scope of the additional toplevel instance
- the scope of the default toplevel instance
- circuit variables 
- constants 
- special identifiers (see [Special Identifiers](expr-special.md))

## Partial elaboration

When the circuit is already elaborated and parameters, options, or circuit variables change, only the parts of the hierarchy affected by those changes need to be re-elaborated. This happens automatically before each analysis. It can also be requested explicitly in the control block:

```text
elaborate changes
```

The simulator checks each instance for topology changes — cases where a conditional block (`@if`) evaluates differently than it did at the previous elaboration. Only instances where topology changed, and their descendants, are torn down and rebuilt. Instances unaffected by the change are left intact. 

If there are no topology changes parameter changes are propagated down the hierarchy. If a parameter change causes a change in node collapsing the system of equations is rebuilt. 

Changes in variables and special identifiers are propagated across the whole hierarchy triggering hierarchy tear downs and rebuilds, if necessary. 

If the circuit has not yet been elaborated when `elaborate changes` is issued, a full default elaboration is performed instead.

## Controlling toplevel names

The `topdef` and `topinst` keyword arguments to the `elaborate` control block command override the internal names used for the toplevel definition and instance (`__topdef__` and `__topinst__` by default):

```text
elaborate circuit topdef="mytop" topinst="myinst"
```
