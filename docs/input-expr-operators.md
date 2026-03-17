# Operators

## Operator table

### Arithmetic

| Operator | Description |
|----------|-------------|
| `a + b` | Addition |
| `a - b` | Subtraction |
| `-a` | Unary negation |
| `a * b` | Multiplication |
| `a / b` | Division |
| `a ** b` | Power ($a^b$) |

### Comparison

| Operator | Description |
|----------|-------------|
| `a == b` | Equal |
| `a != b` | Not equal |
| `a < b` | Less than |
| `a <= b` | Less than or equal |
| `a > b` | Greater than |
| `a >= b` | Greater than or equal |

Comparison operators return `1` (true) or `0` (false).

### Logical

| Operator | Description |
|----------|-------------|
| `a && b` | Logical AND (short-circuit) |
| `a \|\| b` | Logical OR (short-circuit) |
| `!a` | Logical NOT |

`&&` and `||` use short-circuit evaluation: the right operand is not evaluated if the result is already determined by the left operand.

### Bitwise

| Operator | Description |
|----------|-------------|
| `a & b` | Bitwise AND |
| `a \| b` | Bitwise OR |
| `a ^ b` | Bitwise XOR |
| `~a` | Bitwise NOT |
| `a << b` | Left shift |
| `a >> b` | Right shift |

### Conditional

| Operator | Description |
|----------|-------------|
| `a ? b : c` | If `a` is nonzero, evaluates to `b`; otherwise `c`. |

### Selection

| Operator | Description |
|----------|-------------|
| `a[i]` | Element at index `i` of vector or list `a`. Zero-based. |

## Operator precedence

Operators are listed from lowest to highest precedence. Operators on the same row have equal precedence. Associativity applies when two operators of equal precedence appear together.

| Precedence | Operator(s) | Associativity |
|------------|-------------|---------------|
| 1 (lowest) | `? :` | Right |
| 2 | `\|\|` | Left |
| 3 | `&&` | Left |
| 4 | `\|` | Left |
| 5 | `^` | Left |
| 6 | `&` | Left |
| 7 | `== !=` | Left |
| 8 | `< > <= >=` | Left |
| 9 | `<< >>` | Left |
| 10 | `+ -` | Left |
| 11 | `* /` | Left |
| 12 | `**` | Right |
| 13 | unary `-`  `!`  `~` | Right |
| 14 (highest) | `()` `[]` | Left |
