# Circuit Elaboration

The `elaborate` command explicitly triggers circuit elaboration from the control block. Elaboration normally runs automatically before the first analysis, but `elaborate` lets you control when and how it happens.

For a conceptual description of what elaboration does see [Circuit Elaboration](cir-elaboration.md).

## Syntax

```text
elaborate circuit [("sub1", "sub2", ...)] [topdef="name"] [topinst="name"]
elaborate changes
```

## Elaborating the default circuit

```text
elaborate circuit
```

Instantiates the default toplevel circuit (everything defined outside any `subckt`/`ends` block) and builds the simulation-ready hierarchy. Equivalent to what happens automatically before the first analysis.

## Elaborating with additional subcircuits

```text
elaborate circuit("sub1", "sub2")
```

Promotes the named subcircuits to toplevel status alongside the default circuit. Each becomes an additional toplevel instance whose internal models and subcircuit definitions are visible in the global scope. Useful for simulating multiple testbenches or circuit variants in a single session.

## Elaborating changes

```text
elaborate changes
```

Re-elaborates only the parts of the hierarchy affected by parameter, option, or variable changes since the last full elaboration. If the circuit has not yet been elaborated a full default elaboration is performed instead.

This command is called automatically before each analysis, so it is rarely needed explicitly. Use it when you want to force re-elaboration at a specific point in the control block without running an analysis.

## Controlling toplevel names

The `topdef` and `topinst` keyword arguments override the internal names used for the toplevel definitions and instances:

```text
elaborate circuit topdef="mytop" topinst="myinst"
```

The defaults are `__topdef__` and `__topinst__`.
