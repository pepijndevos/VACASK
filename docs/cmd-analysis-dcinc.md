# DC Incremental Small-Signal Analysis (dcinc)

The DC incremental small-signal analysis (dcinc) computes the incremental linear
response of a circuit around its operating point. It solves the linearized
resistive Jacobian for small perturbations of independent sources.

This is useful for studying the incremental behaviour of circuits with
nonlinear devices without performing a full AC frequency sweep.

## Syntax

```text
analysis name dcinc [parameters]
```

## How it works

1. VACASK first performs an operating point (OP) analysis to obtain the DC
   operating point (node voltages, currents, device states).
2. It linearizes the circuit by computing the resistive Jacobian (derivatives of
   the resistive residuals) at the operating point.
3. It solves

   ```text
   Jr * dx = du
   ```

   where `dx` is the incremental change of unknowns and `du` are the incremental
   excitations set by the `mag` parameters of independent sources.

## Parameters

DC incremental analysis exposes the operating point parameters and adds a few
of its own.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `nodeset` | string or list | `""` | Initial guess for the operating point. Can be a stored solution name or explicit node voltages. |
| `store` | string | `""` | Save the computed operating point under the given name. |
| `write` | boolean | `1` | Whether to write the incremental analysis results to a file. |
| `writeop` | boolean | `0` | Additionally write the operating point results to `<analysis>.op.*` output file. |

## Save directives

DC incremental analysis supports the following save directives:

| Directive | Description |
|-----------|-------------|
| `default` | Save all incremental node voltages and branch currents (default). |
| `full` | Save all incremental node voltages only. |
| `dv(node)` | Save the incremental voltage at the given node as `node`. |
| `di(instance)` | Save the incremental current through the given instance. Equivalent to `dv('instance:flow(br)')` |

In addition, dcinc supports all operating point save directives (such as
`v(node)`, `i(instance)`, and `p(instance,outvar)`), because it reuses the
operating point core.

## Example

```text
analysis dc1 dcinc
```

```text
save dv(node1)
save di(Vdd)
analysis dc2 dcinc
```

## Output

- A file `<analysis>.*` containing the requested incremental results.
- If `writeop=1`, an additional `<analysis>.op.*raw*` file containing the operating
  point solution.

## Notes

- The incremental solution is linear; it does not include nonlinear terms.
- The sign of the `mag` parameter of independent sources determines the direction of the
  incremental excitation.
