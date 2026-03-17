# Nested Subcircuit Definitions

A subcircuit definition may contain other subcircuit definitions. The inner definition is local to the enclosing one and is not directly visible from the top level.

## Syntax

```text
subckt outer (a b)
  subckt inner (x y)
    ...
  ends
  xi (a b) inner
ends
```

The inner subcircuit can be instantiated freely within the enclosing definition. It is not accessible by name outside of it.

## Naming of nested definitions

Subcircuit definitions use `::` as the separator between nesting levels. A subcircuit defined at the top level has just its name; one defined inside another is qualified with its enclosing definition's name:

| Path | Meaning |
|------|---------|
| `inv` | Subcircuit `inv` defined at top level |
| `inv::tristate` | Subcircuit `tristate` defined inside `inv` |
| `inv::tristate::cell` | Subcircuit `cell` defined inside `tristate` inside `inv` |

These qualified names appear in `print models` output and in `alter` commands that target a model within a specific definition context.

Models defined inside a subcircuit that has been instantiated are visible under the instance path using `:`, for example `x1:ressub` for model `ressub` defined inside the subcircuit instantiated as `x1`.

## Scope

An inner subcircuit definition is resolved only within the scope of its enclosing definition. It shadows any same-named definition from the top level for instances within that scope. The top-level scope is always searched as a fallback if a name is not found locally.
