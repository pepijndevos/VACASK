# Masters: Models and Subcircuits

A *master* is a named template that instances reference. VACASK has two kinds of masters: models and subcircuits. Both can be defined at the toplevel or inside a subcircuit definition.

## Models

A model binds a `name` to a loaded device type and optionally sets parameter values that are common to all instances of a model:

```text
model name device_type [parameters]
```

`device_type` must be the name of a device module that has been loaded with the `load` directive or the name of a builtin device model. Parameters given on the `model` line are common to all instances that references this model. In some cases an instance can further override a subset of these parameters.

```text
model r resistor
model r10k resistor r=10k
model d sp_diode is=1e-12 n=2
```

## Subcircuits

A subcircuit is a reusable block of models and instances:

```text
subckt name(terminal1 terminal2 ...)
  [parameters keyword]
  [models and instances]
ends
```

The terminals listed in parentheses are the connection points exposed to the parent. Subcircuits can contain models, instances, nested subcircuit definitions, and blocks of`parameters`.

```text
subckt divider(in out)
  parameters r1=1k r2=1k
  r_hi (in out) resistor r=r1
  r_lo (out 0) resistor r=r2
ends
```

Subcircuit instances are created the same way as device instances — the subcircuit name is used as the master name.

## Scope

Models and subcircuits are resolved by searching first in the current scope (the enclosing subcircuit), then in the toplevel scope. A locally defined master shadows a same-named master from the toplevel scope.
