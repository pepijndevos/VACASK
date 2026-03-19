# Inductive Coupling

Module name: `mutual`

A `mutual` instance couples two inductors by adding the off-diagonal terms of the mutual inductance M to the reactive MNA stamp. It takes no terminals. The coupled inductors are identified by instance name.

The mutual inductance is computed from the coupling coefficient and the individual inductances:

```text
M = k × sqrt(L1 × L2)
```

L1 and L2 are divided by the `$mparam` value of the respective inductor. 

## Terminals

None. The instance must be declared with empty parentheses:

```text
m12 () mutual k=0.9 ind1="l1" ind2="l2"
```

## Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `k` | real | 0 | Coupling coefficient. Must be in [0, 1]. |
| `ind1` | string | — | Name of the first inductor instance. Required. |
| `ind2` | string | — | Name of the second inductor instance. Required. |
| `ctlnode1` | string | `"flow(br)"` | Internal flow node of `ind1` that carries its branch current. |
| `ctlnode2` | string | `"flow(br)"` | Internal flow node of `ind2` that carries its branch current. |

The default `ctlnode1` and `ctlnode2` values work with inductors that introduce a branch-current unknown, which is the case for the VACASK Verilog-A inductor (`inductor.osdi`). Verilog-A models that do not expose a `flow(br)` node cannot be coupled with `mutual`. The model also assumes the coupled inductors expose `l` (inductance) and `$mfactor` parameters. The inductive coupling module does not expose a `$mfactor` parameter. 

## Output variables

| Variable | Description |
|----------|-------------|
| `mutual` | Computed mutual inductance M (H) |

## Example

```text
Transformer with mutual coupling

ground 0
load "inductor.osdi"
load "resistor.osdi"

model inductor inductor
model mutual mutual
model vsource vsource
model resistor resistor

vs (1 0) vsource type="sine" sinedc=0 ampl=1 freq=50
rs (1 2) resistor r=1
l1 (2 0) inductor l=10m
l2 (3 0) inductor l=40m
m12 () mutual k=0.95 ind1="l1" ind2="l2"
rload (3 0) resistor r=100

control
  abort always
  analysis ac1 ac from=10 to=1k mode="dec" points=20
  analysis tran1 tran stop=40m step=0.1m
endc
```

`l1` and `l2` form a transformer with a turns ratio of approximately 1:2 (since `sqrt(L2/L1) = 2`). The coupling coefficient `k=0.95` sets the leakage. `m12` adds the mutual terms; the computed mutual inductance is `M = 0.95 × sqrt(10m × 40m) ≈ 19 mH`.
