# Transient Analysis

Transient analysis computes the large-signal time-domain response of a circuit. It integrates the circuit's differential equations from $t=0$ to a specified stop time using an adaptive timestep integrator.

## Syntax

```text
analysis name tran [parameters]
```

## How it works

The circuit equations are

$$f(x(t)) + \frac{d}{dt} q(x(t)) = 0$$

where $f$ is the resistive residual and $q$ is the reactive residual. At each timestep the integrator approximates the time derivative and reduces the problem to a nonlinear algebraic system, which is solved with Newton-Raphson iteration. The timestep is adapted based on a local truncation error (LTE) estimate.

### Initial conditions

Two modes are available via the `icmode` parameter:

- **`op`** (default) — Solves an operating point at $t=0$ with any specified initial conditions (`ic`) applied as forced constraints. This gives a consistent initial state.
- **`uic`** — Skips the operating point solve and directly sets reactive component states from `ic`. Equivalent to SPICE3 `uic` transient. If the initial state is not consistent the timepoint at $t=0$ is not correct (like in SPICE). 

Initial conditions use the same format as nodesets: a list alternating node names and values. Single-node: `["node"; value; ...]`. Differential: `["node1"; "node2"; value; ...]`. A stored solution name may also be given as a string.

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `stop` | real | `0` | Simulation end time (s). |
| `step` | real | `0` | Initial timestep (s). |
| `start` | real | `0` | Time at which results start being recorded (s). |
| `maxstep` | real | `0` | Maximum allowed timestep (s). If 0, no limit is imposed beyond `step`. |
| `icmode` | string | `"op"` | Initial condition mode: `"op"` or `"uic"`. |
| `ic` | string or list | `""` | Initial conditions. A stored solution name or a list of node/value pairs. See [Operating Point Analysis](cmd-analysis-op.md) for list syntax. |
| `nodeset` | string or list | `""` | Nodeset for the internal operating point solve (`icmode="op"` only). See [Operating Point Analysis](cmd-analysis-op.md). |
| `store` | string | `""` | Save the final transient solution under the given name for use as an initial condition in subsequent analyses. |
| `write` | boolean | `1` | Write the transient results to a file. |

The integration method is selected via the simulator option `tran_method`. Available methods: `"trap"` (trapezoidal, i.e. Adams-Moulton of order 2), `"euler"` (Adams-Moultin/Gear of order 1), `"bdf"`/`"gear"` (BDF of arbitrary order), `"bdf2"`/`"gear2"` (BDF order 2), `"am"` (Adams-Moulton of arbitrary order). The maximum order of the integration algorithm is set by the `tran_maxord` simulator option. 

## Save directives

| Directive | Description |
|-----------|-------------|
| `default` | Save all node values and branch flows (default behavior). |
| `full` | Saves all unknowns (even those belonging to collapsed nodes). |
| `v(node)` | Save the value at the given node. |
| `i(instance)` | Save the branch flow through the given instance. Only instances that introduce a current variable in the MNA system are valid (e.g. voltage sources, inductors). Equivalent to `v('instance:flow(br)')`. |
| `p(instance,outvar)` | Save the output variable `outvar` from the given instance. |

## Output

- A file `<analysis>.*` containing the time-domain results at each recorded timestep.

| Variable | Description |
|----------|-------------|
| `time` | Time sweep variable (s). Always present. |
| `node` | Value at the given node. Saved by `v(node)` or `default`. |
| `instance:flow(br)` | Branch flow through the given instance. Saved by `i(instance)` or `default`. |
| `instance.outvar` | Output variable `outvar` from the given instance. Saved by `p(instance,outvar)`. |

## Examples

**Basic transient with operating point initial condition:**

```text
analysis tran1 tran stop=10m step=1u icmode="op"
```

**With explicit initial conditions:**

```text
analysis tran1 tran stop=10m step=1u icmode="op" ic=["node1"; 2.0; "node2"; "node3"; 1.0]
```

**UIC mode:**

```text
analysis tran1 tran stop=10m step=1u icmode="uic" ic=["node1"; 2.0]
```

**Full circuit with embedded postprocessing:**

```text
Transient response of an RC circuit

load "resistor.osdi"
load "capacitor.osdi"

model resistor resistor
model capacitor capacitor
model vsource vsource

v1 (1 0) vsource type="pulse" val0=1 val1=2 rise=1u delay=1m
r1 (1 2) resistor r=1k
c1 (2 0) capacitor c=1u

control
  analysis tran1 tran stop=10m step=1u icmode="op"
  postprocess(PYTHON, "plot.py")
endc

embed "plot.py" <<<FILE
import matplotlib.pyplot as plt
from rawfile import rawread

plot = rawread('tran1.raw').get()

fig1, ax1 = plt.subplots(1, 1)
fig1.suptitle('RC transient response')
ax1.set_ylabel('Voltage [V]')
ax1.set_xlabel('Time [ms]')
ax1.plot(plot['time']*1e3, plot['1'], label='V(1)')
ax1.plot(plot['time']*1e3, plot['2'], label='V(2)')
ax1.legend(loc='lower right')
ax1.grid(True)
plt.show()
>>>FILE
```
