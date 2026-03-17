# Scalar Data Types

VACASK expressions work with three scalar types: integer, real, and string.

## Types

| Type | Storage | Description |
|------|---------|-------------|
| `integer` | 32-bit signed | Whole numbers. Produced by integer literals and integer operations. |
| `real` | 64-bit float (double) | Floating-point numbers. Produced by floating-point literals and real-valued functions. |
| `string` | UTF-8 text | Character sequences. Produced by string literals. |

For literal syntax see [Numbers](input-numbers.md) and [Strings](input-strings.md).

## Type promotion in arithmetic

When an operator or function receives operands of mixed `integer` and `real` type, the result is `real`. This follows standard C arithmetic promotion. Integer-only operands produce an integer result.

| Left | Right | Result |
|------|-------|--------|
| `integer` | `integer` | `integer` |
| `integer` | `real` | `real` |
| `real` | `integer` | `real` |
| `real` | `real` | `real` |

Integer division truncates toward zero: `7 / 2` evaluates to `3`. To get a real result, use at least one real operand: `7.0 / 2` evaluates to `3.5`.

## Boolean semantics

There is no separate boolean type. Comparison and logical operators return `integer` `1` (true) or `0` (false). Any nonzero numeric value is treated as true in conditional contexts (`? :`, `&&`, `||`, `@if`). An empty string is treated as false; a nonempty string as true.

## Type conversion

The functions `int()`, `real()`, and `string()` convert between scalar types. See [Builtin Functions](expr-functions.md).
