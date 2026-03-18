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

The inner subcircuit can be instantiated freely anywhere within the enclosing
definition. It is not accessible by name from outside.

## Example

```text
subckt ota (inp inn outp outn vdd vss)
  parameters ibias=100u

  // Local helper subcircuit — not visible outside ota
  subckt diff_pair (inp inn tail outp outn vdd vss)
    parameters w=4u l=180n
    mn1 (outp inn tail vss) nmos w=w l=l
    mn2 (outn inp tail vss) nmos w=w l=l
  ends

  // Local helper subcircuit — not visible outside ota
  subckt load_mirror (in out vdd)
    mp1 (in  in  vdd vdd) pmos w=2u l=180n
    mp2 (out in  vdd vdd) pmos w=2u l=180n
  ends

  // Instantiate local helper subcircuits
  xdp  (inp inn tail outp outn vdd vss) diff_pair
  xlm  (outp outn vdd) load_mirror

  ibias_src (tail vss) isource dc=ibias
ends
```

## Naming of nested definitions

Subcircuit definitions use `::` as the separator between nesting levels. A subcircuit defined at the top level has just its name; one defined inside another is qualified with its enclosing definition's name:

| Path | Meaning |
|------|---------|
| `ota` | Subcircuit `ota` defined at top level |
| `ota::diff_pair` | Subcircuit `diff_pair` defined inside `ota` |
| `inv::tristate::cell` | Subcircuit `cell` defined inside `tristate` inside `inv` |

Nested subcircuit definitions do not affect how instance hierarchy is handled. Defining a subcircuit within a subcircuit only limits the visibility of that subcircuit definition. 

## Scope

An inner subcircuit definition is resolved only within the scope of its enclosing definition. It shadows any same-named definition from the top-level for instances within that scope. The top-level scope is always searched as a fallback if a subcircuit definition name is not found locally.
