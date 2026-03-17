# Builtin Functions

## Trigonometric

| Function | Description |
|----------|-------------|
| `sin(x)` | Sine |
| `cos(x)` | Cosine |
| `tan(x)` | Tangent |
| `asin(x)` | Arcsine |
| `acos(x)` | Arccosine |
| `atan(x)` | Arctangent |
| `atan2(y, x)` | Arctangent of $y/x$, using signs of both arguments to determine the quadrant |
| `sinh(x)` | Hyperbolic sine |
| `cosh(x)` | Hyperbolic cosine |
| `tanh(x)` | Hyperbolic tangent |
| `asinh(x)` | Inverse hyperbolic sine |
| `acosh(x)` | Inverse hyperbolic cosine |
| `atanh(x)` | Inverse hyperbolic tangent |

## Exponential, logarithmic, square root, power

| Function | Description |
|----------|-------------|
| `exp(x)` | Exponential $e^x$ |
| `log(x)` | Natural logarithm $\ln x$ (same as `ln`) |
| `ln(x)` | Natural logarithm $\ln x$ |
| `log10(x)` | Base-10 logarithm $\log_{10} x$ |
| `sqrt(x)` | Square root $\sqrt{x}$ |
| `pow(x, y)` | Power $x^y$. Equivalent to `x ** y`. |
| `hypot(x, y)` | Euclidean distance $\sqrt{x^2 + y^2}$ |

## Rounding and sign

| Function | Description |
|----------|-------------|
| `abs(x)` | Absolute value $\|x\|$ |
| `floor(x)` | Largest integer not greater than `x` |
| `ceil(x)` | Smallest integer not less than `x` |
| `round(x)` | Round to nearest integer, halfway cases away from zero |
| `integer(x)` | Truncate toward zero |
| `sgn(x)` | Sign: $-1$ or $1$ |
| `sign(x, y)` | Magnitude of `x` with the sign of `y` |
| `fmod(x, y)` | Floating-point remainder of `x / y` |

## Numeric checks

| Function | Description |
|----------|-------------|
| `isinf(x)` | `1` if `x` is infinite, `0` otherwise |
| `isnan(x)` | `1` if `x` is NaN, `0` otherwise |
| `isfinite(x)` | `1` if `x` is finite and not NaN, `0` otherwise |

## Aggregation

These functions operate on vectors. `min` and `max` also accept two scalar or vector arguments for component-wise comparison.

| Function | Description |
|----------|-------------|
| `min(x)` | Minimum element of vector `x` |
| `min(x, y)` | Component-wise minimum of `x` and `y` |
| `max(x)` | Maximum element of vector `x` |
| `max(x, y)` | Component-wise maximum of `x` and `y` |
| `sum(x)` | Sum of all elements of `x` |
| `prod(x)` | Product of all elements of `x` |
| `all(x)` | `1` if all elements of `x` are nonzero |
| `any(x)` | `1` if any element of `x` is nonzero |

## Type checking

| Function | Description |
|----------|-------------|
| `isint(x)` | `1` if `x` is a scalar integer |
| `isreal(x)` | `1` if `x` is a scalar real |
| `isstring(x)` | `1` if `x` is a scalar string |
| `isvector(x)` | `1` if `x` is a vector |
| `islist(x)` | `1` if `x` is a list |

## Type conversion

| Function | Description |
|----------|-------------|
| `int(x)` | Convert `x` to integer |
| `real(x)` | Convert `x` to real |
| `string(x)` | Convert `x` to string |

## Vector construction

| Function | Description |
|----------|-------------|
| `len(x)` | Number of elements in vector or list `x` |
| `vector(n)` | Create an integer vector of length `n` filled with zeros |
| `vector(n, v)` | Create a vector of length `n` filled with value `v`. The element type matches the type of `v`. |
| `range(to)` | Integer vector `[0, 1, ..., to-1]` |
| `range(from, to)` | Integer vector `[from, from+1, ..., to-1]` |
| `range(from, to, step)` | Vector from `from` to `to` (exclusive) with increment `step` |
