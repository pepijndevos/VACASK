# Monte Carlo Analysis

A Monte Carlo loop repeats a portion of the control block many times, drawing new random values for parameters marked with a distribution function on every repetition. It is used to study how manufacturing variation (device mismatch, process spread) affects circuit behavior across a population of randomly generated samples.

## Syntax

```text
mc name samples=n [seed=s]
  ... control statements, analyses ...
endmc
```

`name` identifies the loop. It is used to build the file name prefix of analyses run inside the loop (see [Output](#output) below) and must be unique among `mc` loops in the run.

| Keyword | Type | Required | Description |
|---------|------|----------|-------------|
| `samples` | integer | yes | Number of Monte Carlo samples (loop iterations). Must be greater than zero. |
| `seed` | integer | no | Seed for the random number generator. Default: `0`. |

Everything between `mc` and the matching `endmc` is executed once per sample. `mc` loops cannot be nested.

## Distribution functions

Instance parameters, model parameters, and subcircuit `parameters` expressions may call one of four distribution functions in place of a plain value. Each call site draws its own independent random number.

| Function | Distribution | Meaning |
|----------|--------------|---------|
| `gauss(nom, rvar, sigma)` | Gaussian | Mean `nom`, with `rvar` a relative variation (fraction of `nom`) that corresponds to `sigma` standard deviations. Standard deviation is `nom*rvar/sigma`. |
| `agauss(nom, avar, sigma)` | Gaussian | Mean `nom`, with `avar` an absolute variation that corresponds to `sigma` standard deviations. Standard deviation is `avar/sigma`. |
| `unif(nom, rvar)` | Uniform | Uniformly distributed in `[nom*(1-rvar), nom*(1+rvar)]`. |
| `aunif(nom, avar)` | Uniform | Uniformly distributed in `[nom-avar, nom+avar]`. |

For `gauss` and `agauss`, setting `rvar<=0`, `avar<=0`, or `sigma<=0` turns the random generator off: the call always returns `nom`, without consuming a random draw.

```text
r1 (in out) resistor r=gauss(1k, 0.05, 3)     // 1k, 5% tolerance at 3 sigma
r2 (out 0)  resistor r=agauss(1k, 50, 3)      // 1k +- 50 ohm at 3 sigma
r3 (a b)    resistor r=unif(1k, 0.01)         // 1k +- 1%, uniform
r4 (b 0)    resistor r=aunif(1k, 10)          // 1k +- 10 ohm, uniform
```

`gauss`/`agauss` use Wichura's Algorithm AS 241 to convert a uniform draw to a standard normal deviate, so tails are not clipped: with `sigma=3` a small fraction of samples will fall outside `nom +- nom*rvar` (resp. `nom +- avar`).

Outside a Monte Carlo loop these functions return `nom` unchanged, so a netlist that uses them can also be run as a plain (non-MC) simulation.

## Where distributions are evaluated

A distribution function only draws a random value when the expression that contains it is evaluated in one of these contexts:

- an instance parameter,
- a model parameter,
- a `parameters` statement (subcircuit or toplevel).

In any other context (for example inside a conditional netlist block's condition expression) the call is accepted syntactically but simply returns `nom`, without consuming a random draw.

Each call site is identified by the enclosing instance/model/subcircuit-instance name, the parameter name, and the position of the call within the expression. This identity is stable across the netlist's conditional branches and re-elaborations, so the same call site always gets the same sample value within one Monte Carlo iteration, however many times its expression happens to be re-evaluated.

## The `MCSAMPLE` variable

Inside the loop, the circuit variable `MCSAMPLE` holds the current 1-based sample index (`1` on the first iteration, `2` on the second, and so on). It can be used like any other circuit variable, e.g. in `print` statements or expressions.

## Output

Each `analysis` statement executed inside the loop writes its own raw file per sample, named:

```text
name.i.analysisname.raw
```

where `name` is the `mc` loop's name and `i` is the 1-based sample index. For example, `mc mc1 samples=500` running `analysis op1 op` produces `mc1.1.op1.raw`, `mc1.2.op1.raw`, ..., `mc1.500.op1.raw`.

## Example

**Resistor divider, Gaussian tolerance, output histogram:**

```text
Monte Carlo demo: resistor divider with gaussian tolerances

ground 0

load "resistor.osdi"

model r resistor
model v vsource

v1 (in 0)   v dc=10
r1 (in out) r r=gauss(1k, 0.05, 3)
r2 (out 0)  r r=gauss(1k, 0.05, 3)

control
  abort always
  save v(out)

  mc mc1 samples=500 seed=1
    analysis op1 op
  endmc

  postprocess(PYTHON, "runme.py")
endc

embed "runme.py" <<<FILE
from rawfile import rawread
import numpy as np
import matplotlib.pyplot as plt

nsamples = 500
vout = np.array([rawread(f"mc1.{i}.op1.raw").get()["out"] for i in range(1, nsamples+1)])

print(f"Mean:          {vout.mean():.4f} V")
print(f"Std deviation: {vout.std():.4f} V")

fig1, ax1 = plt.subplots(1, 1, figsize=(6, 4), dpi=100, tight_layout=True)
fig1.axes[0].set_title('Monte Carlo distribution of divider output voltage')
fig1.axes[0].set_xlabel('Vout [V]')
fig1.axes[0].set_ylabel('Count')
fig1.axes[0].hist(vout, bins=30, color="steelblue", edgecolor="black")
plt.show()
>>>FILE
```
