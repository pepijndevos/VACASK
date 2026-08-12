# Nonlinear Behavioral Sources

A behavioral source defines a two-terminal voltage or current source directly from an
expression, without writing a Verilog-A module. VACASK translates the expression into a
small synthesized Verilog-A module, compiles it with the same OSDI pipeline used for
ordinary `.va` files, and instantiates it in place of the source line. Unlike the other
builtin devices, a behavioral source is not loaded with `load` or declared with `model`;
it is a distinct netlist syntax recognized directly by the parser.

## Syntax

```text
<name> (p n) v=<expr>
<name> (p n) potential=<expr>                              // same as v=
<name> (p n) i=<expr>
<name> (p n) flow=<expr>                                   // same as i=
<name> (p n) v=<expr> discipline=["<discipline>", "<potential accessor>", "<flow accessor>"]
```

A behavioral source instance takes exactly two terminals, `p n`, and no master/model name.
The absence of a master name after the terminal list is what distinguishes a behavioral
source line from a regular instance line. Exactly one of `v`/`potential` or `i`/`flow` must
be given.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `v`, `potential` | expression | - | Voltage-defining source: $V(p) - V(n) = \text{expr}$. Mutually exclusive with `i`/`flow`. |
| `i`, `flow` | expression | - | Current-defining source: the current flowing from `p` to `n` through the device equals `expr`. Mutually exclusive with `v`/`potential`. |
| `discipline` | constant string vector[3] | `["electrical", "V", "I"]` | `[discipline name, potential accessor, flow accessor]`. See [Setting the Discipline](#setting-the-discipline). |

## How It Works

The expression is compiled once, before circuit elaboration, into a Verilog-A module named
`__behavioral_<name>` (prefixed with the subcircuit definition path and conditional block 
path for instances inside a subcircuit). The synthesized module is written to a `.va` file 
alongside the netlist and loaded automatically; it is then instantiated as an ordinary OSDI 
device. A `v=`/`potential=` expression becomes a branch potential contribution (`<+`); 
an `i=`/`flow=` expression becomes a branch flow contribution, so the resulting instance is 
a fully nonlinear, possibly reactive, device like any other compiled Verilog-A source.

Inside the expression, in addition to the referenced parameters, the following node/branch
accessors are recognized:

| Form | Meaning |
|------|---------|
| `v(node)` | Potential of `node` relative to ground |
| `v(nodeA, nodeB)` | Potential difference $V(\text{nodeA}) - V(\text{nodeB})$ |
| `i(instance)` | Branch current (`flow(br)`) of another instance, e.g. a `vsource` |

Each node or instance referenced this way is silently wired to the synthesized module as an
extra terminal; it does not need to be one of the source's own two terminals.

### Model and Instance Names

The behavioral source instance itself keeps the name written on its source line (e.g. `b2`)
and is addressed like any other instance, including the usual hierarchical path when it sits
inside a subcircuit. The synthesized module and the `model` declaration bound to it, however,
get a separate generated name:

```text
__behavioral<subcircuit path>_<conditional block path>_<name>
```

- `<subcircuit path>` is empty at the top level. For a behavioral source written inside a
  subcircuit *definition*, it is that definition's name, prefixed with `_` (and further
  prefixed the same way for each enclosing definition, for nested subcircuit definitions).
- `<conditional block path>` is empty unless the behavioral source is written inside a
  conditional (`@if`/`@elseif`/`@else`) block. It is then a chain of `s<seq>b<block>` segments,
  one per nesting level, identifying which branch of which conditional block sequence the
  source line belongs to.
- `<name>` is the instance name exactly as written on the behavioral source line.

Because this name is derived from where the source line appears in the netlist text - not
from where it ends up in the elaborated circuit hierarchy - every instantiation of a given
subcircuit definition shares the same synthesized module and model. The name only matters
for diagnostics, such as `print device(...)`/`print model(...)`; a top-level behavioral
source named `b2` produces a device and model both named `__behavioral_b2`:

```text
print device("__behavioral_b2")
print model("__behavioral_b2")
```

### Limitations

Because the expression is compiled to Verilog-A rather than evaluated at run time, only a
restricted subset of the expression language survives translation:

- Only real, integer, and string literals translate; vector and list literals have no
  Verilog-A equivalent and are rejected.
- Every free identifier in the expression becomes a Verilog-A `parameter`, typed `real` by
  default. Only scalar `int`/`real` parameters can be forwarded this way; string- or
  vector-typed circuit parameters cannot be used. See
  [Forcing Parameter Type for Verilog-A](#forcing-parameter-type-for-verilog-a) to select
  `integer` instead of `real`.
- The bitwise and shift operators (`&`, `|`, `^`, `~`, `<<`, `>>`) and the indexing operator
  `[]` have no Verilog-A equivalent and are rejected.
- Only the functions listed in [Functions](#functions) can be translated; any other builtin
  function raises an error.
- `$mfactor` is not supported for behavioral sources.
- Builtin named constants such as `M_PI` are inlined as numeric literals, since the
  corresponding Verilog-A constants may differ slightly depending on which standard files
  are included.

## Forcing Parameter Type for Verilog-A

Every free identifier in a behavioral source expression is forwarded as a `real`
Verilog-A parameter by default. `$intparam(<name>)` forces the identifier `<name>` to be
declared `integer` instead; `$realparam(<name>)` forces (or confirms) `real`. Both take a
single bare identifier as their only argument and otherwise evaluate to that identifier.

```text
parameters ip=1 rp=9.9 p=15.5
b5 (e 0) potential=$intparam(ip) + $realparam(rp) + p
```

Here `ip` is declared as an `integer` parameter of the synthesized module, while `rp` and
`p` are declared `real` (`p` uses the default).

## Setting the Discipline

By default a behavioral source's terminals use the `electrical` discipline, with `V` and
`I` as the potential and flow access functions. `discipline` overrides this with a
3-element constant string vector `[discipline, potential accessor, flow accessor]`; any
discipline defined in the standard `disciplines.vams` file may be named. The value must be
a literal - it cannot be computed from an expression or a parameter.

```text
// Thermal domain: potential is temperature (Temp), flow is power (Pwr)
b4 (d 0) potential=M_PI*v(a) discipline=["thermal", "Temp", "Pwr"]
```

The discipline applies to the source's own two terminals and to any extra node introduced
by the Verilog-A compiler. It has no effect on control nodes: VACASK never instantiates 
a control node - it rebinds it directly to existing nodes, which already carry
whatever discipline was assigned to them. The `discipline` declaration written in
the synthesized module is syntactically present for every terminal (Verilog-A requires a
port to have one), but for a control node it has no effect on the elaborated circuit.

## Functions

| VACASK function | Verilog-A equivalent | Notes |
|------------------|----------------------|-------|
| `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh` | same name | 1 argument |
| `exp`, `sqrt`, `abs`, `floor`, `ceil` | same name | 1 argument |
| `ln` | `ln` | Natural logarithm |
| `log` | `ln` | VACASK `log` is the natural logarithm; renamed to avoid Verilog-A's base-10 `log` |
| `log10` | `log` | Verilog-A's `log` is base-10 |
| `pow`, `hypot`, `atan2` | same name | 2 arguments |
| `min`, `max` | same name | 2-argument form only; the 1-argument vector-aggregate form is not supported |
| `int` | `$rtoi` | 1 argument |
| `real` | `$itor` | 1 argument |
| `ddt` | `ddt` | See [Operators](#operators) |
| `idt` | `idt` | See [Operators](#operators) |
| `white_noise`, `flicker_noise` | same name | See [Noise](#noise) |

All other functions documented in [Builtin Functions](expr-functions.md) (`round`, `fmod`,
`sgn`, `sign`, `isinf`, `isnan`, `isfinite`, `string`, `vector`, `range`, `gauss`, `unif`,
and the rest) have no Verilog-A equivalent and cannot be used in a behavioral source
expression.

## Special Identifiers

`$temp` and `$scale` behave as documented in [Special Identifiers](expr-special.md) and can
be used like in any other parameter expression. `$abstime` is available only inside
behavioral source expressions; it maps to Verilog-A's `$abstime` and is meaningful only in
a time-domain analysis. Referencing `$abstime` disables the bypass optimization for the
underlying OSDI instance.

| Identifier | Description |
|------------|-------------|
| `$temp` | Ambient temperature (degC). |
| `$scale` | Global instance length scaling factor. |
| `$abstime` | Absolute simulation time (s). Behavioral source only. |

```text
b1 (t 0) v=$temp        // tracks the "temp" option
b2 (s 0) v=$scale       // tracks the "scale" option
b3 (abst 0) v=$abstime  // tracks simulation time in a transient analysis
```

## Operators

`ddt` and `idt` are only meaningful in a time-domain (transient) analysis; they pass
through directly to Verilog-A's own `ddt()`/`idt()` operators.

- `ddt(x)` - time derivative of `x`. At the DC operating point, before time-stepping
  begins, there is no derivative to compute, so `ddt(x)` evaluates to `0` there.
- `idt(x)` - time integral of `x` from `t = 0`, with an implicit initial condition of `0`.
- `idt(x, ic)` - time integral of `x` with explicit initial condition `ic`.

```text
b1 (d 0) v=ddt($abstime)     // == 1 at every transient point except t=0
b2 (i0 0) v=idt(1)           // == time
b3 (i1 0) v=idt(1, 5)        // == time + 5
```

## Noise

Two functions inject noise into a behavioral source expression; both map directly onto
Verilog-A's own noise system functions and are only meaningful during
[noise analysis](cmd-analysis-noise.md):

| Function | Description |
|----------|-------------|
| `white_noise(pwr, "name")` | Noise source with constant power spectral density `pwr`. |
| `flicker_noise(pwr, exp, "name")` | Flicker noise source with PSD $\text{pwr}/f^{\text{exp}}$. |

The last argument names the noise contribution and must be a constant string literal; it is
used in the noise output-variable syntax `n(instance,contrib)`.

```text
bwn (n1 0) i=white_noise(pwr_w, "mynoise")
bfn (n2 0) i=flicker_noise(pwr_f, ef, "myflicker")
```

## Example

```text
Behavioral diode-like nonlinear resistor

ground 0
load "resistor.osdi"

model resistor resistor
model vsource vsource

vin (in 0) vsource type="pulse" val0=0 val1=0.6 rise=1u fall=1u width=1u period=4u
rin (in a) resistor r=100

// Shockley-like nonlinear current source: i = Is*(exp(v/Vt)-1)
bdiode (a 0) i=1e-14*(exp(v(a,0)/0.026)-1)

control
  abort always
  analysis tran1 tran stop=8u step=10n
endc
```

`bdiode` behaves like an ideal diode driven through `rin`. VACASK compiles the expression
into a synthesized Verilog-A module and simulates it exactly like any other nonlinear OSDI
device.
