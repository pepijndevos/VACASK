# runtest.py

`runtest` provides small helpers for writing postprocessing scripts that double as automated tests. The same script can produce plots during interactive use and perform pass/fail checks when run by a test harness.

```python
from runtest import *
```

## isTest

```python
isTest() → bool
```

Returns `True` when the environment variable `SIM_TEST` is set to `"yes"`. Use it to switch between test mode (exit with a status code) and interactive mode (show plots).

```python
if isTest():
    sys.exit(not all(tests))
else:
    ...
    plt.show()
```

## relDiff

```python
relDiff(a, b, abstol) → array
```

Computes the relative difference between `a` and `b` with an absolute tolerance floor:

```text
relDiff = |a − b| / max(max(|a|, |b|), abstol)
```

`abstol` prevents division by zero when both values are near zero. Accepts NumPy scalars and arrays. Returns an array of the same shape.

```python
status = (relDiff(v, exact, 1e-6) < 1e-3).all()   # pass if < 0.1 %
```

## absDiff

```python
absDiff(a, b) → array
```

Returns `|a − b|`. A convenience wrapper around `numpy.abs`.

```python
status = absDiff(v, exact).max() < 1e-3
```

## Example

```python
from rawfile import rawread
from runtest import *
import sys

op = rawread('op1.raw').get()
v = op['out']
exact = 5.0

tests = []
tests.append((relDiff(v, exact, 1e-9) < 1e-3).all())

if isTest():
    sys.exit(not all(tests))
else:
    print('V(out) =', v, '  expected:', exact)
```
