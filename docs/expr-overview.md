# Data Types and Expressions

VACASK expressions are used wherever a parameter value appears — in instance and model parameters, subcircuit parameters, top-level parameters, sweep ranges, analysis parameters, and conditional block conditions.

## Scalar types

There are three scalar types:

| Type | Description |
|------|-------------|
| `integer` | 32-bit signed integer. Result of integer literals and integer operations. |
| `real` | 64-bit floating-point. Result of floating-point literals, SI-suffix numbers, and real-valued functions. |
| `string` | UTF-8 text. Result of string literals. |

When `integer` and `real` operands are mixed in an expression the result is `real`. Integer division truncates toward zero. There is no separate boolean type; zero and empty string are false, everything else is true.

See [Scalar Data Types](expr-scalars.md) for the full rules.

## Compound type construction

| Type | Syntax | Description |
|------|--------|-------------|
| Vector | `[a, b, c]` | Ordered sequence of elements sharing the same scalar type. |
| List | `[a; b; c]` | Ordered sequence of elements of mixed types. |
| Merged list | `[a: b: c]` | Like a list, but nested lists are flattened in. |

Vectors and lists are used for multi-value parameters such as sweep value sets and initial conditions.

See [Vectors and Lists](expr-vectors.md) for details.

## What can appear in an expression

- Integer and floating-point literals, including SI suffixes (`1k`, `100n`, `2.5meg`)
- String literals
- [Builtin constants](expr-constants.md) (`M_PI`, `P_K`, …)
- [Special identifiers](expr-special.md) (`$temp`, `$scale`)
- [Circuit variables](expr-cirvars.md) set with the `var` command
- Parameters declared with the `parameters` keyword in the enclosing scope
- [Operators](expr-operators.md) — arithmetic, comparison, logical, bitwise, ternary
- [Builtin functions](expr-functions.md) — math, type conversion, string, vector operations
