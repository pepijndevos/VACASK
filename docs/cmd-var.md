# Modifying Circuit Variables

Circuit variables can set and cleared from the control block. Changes take effect before the next analysis or elaboration step. For an overview of circuit variables and how they are used in parameter expressions see [Circuit Variables](expr-cirvars.md).

## Setting variables

```text
var name=expr [name2=expr ...]
```

Each right-hand side is an expression. All expressions in a `var` statement are evaluated first using the variable values that exist at the time the statement executes, then all results are written. A later assignment in the same statement therefore sees the value of a variable from before the statement, not from an earlier assignment in the same statement.

```text
var vdd=1.8 vss=0
var gain=5
var gain=10 rfb=gain*1k   // rfb = 5k (gain before this statement was 5)
var rin=gain*1k           // rin = 10k (gain is now 10)
```

## Clearing variables

```text
clear variables
```

Removes all user-defined circuit variables. The built-in `PYTHON` variable is restored automatically.
