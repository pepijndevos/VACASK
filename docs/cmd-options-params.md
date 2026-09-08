# Parameter Handling Options

These options control what the simulator does with parameters written on a model
or instance that the corresponding master does not declare.

## Undeclared parameters

| Name | Type | Default | Allowed | Description |
|------|------|---------|---------|-------------|
| `unknownparam` | string | `"error"` | `"error"`, `"warn"`, `"ignore"` | What to do with a model or instance parameter the master does not declare. `error` aborts, `warn` reports the parameter and its source location and drops it, `ignore` drops it silently. |

Only a failed parameter name lookup is affected. An expression that fails to
evaluate and a value of the wrong type remain errors under every setting, and a
parameter dropped under `warn` or `ignore` is never evaluated at all.

The option applies to device models, device instances, and subcircuit instances.
It does not apply to simulator options, analysis parameters, or the `alter`
command, where a name that does not resolve is always an error.

Any value other than `"warn"` or `"ignore"` behaves as `"error"`, so a typo in
the option value cannot quietly disable the check.

## When to relax it

Foundry model cards routinely carry parameters that belong to a different
simulator's parameter table. Sky130's bipolar cards, for instance, set `dcap`,
`gap1`, and `gap2`, none of which exist in the `sp_bjt` master, and Ngspice
itself ignores them with an `unrecognized parameter` message. With the default
setting such a card stops elaboration and the whole model library fails to load.

```text
control
  options unknownparam="warn"
  analysis op1 op
endc
```

Relaxing the check is a compatibility measure, not a correctness one. A dropped
parameter that does matter produces wrong results instead of a stop, so prefer
`"warn"` over `"ignore"` and read what it reports.
