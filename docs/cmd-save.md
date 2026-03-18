# Saving Results

The `save` statement controls what quantities are written to the output file. Save directives accumulate across the control block and apply to all analyses that follow until cleared.

## Syntax

```text
save directive [directive ...]
```

Multiple directives may appear on a single line, or `save` may be used multiple times:

```text
save v(out) v(in)
save i(v1)
```

## Directive syntax

| Form | Saves |
|------|-------|
| `default` | Default set of quantities for the analysis (node voltages and branch flows). |
| `full` | All unknowns, including those of collapsed nodes. |
| `v(node)` | Voltage at `node`. |
| `i(instance)` | Branch flow through `instance` (only instances that introduce a flow variable, e.g. voltage sources and inductors). Equivalent to `v('instance:flow(br)')`.  |
| `p(instance, outvar)` | Output variable `outvar` from `instance`. |

There are many different directives available. Refer to the sections on individual analyses for details. 

Save directives accept hierarchical names (see [Defining a Subcircuit](cir-subckt.md)), just make sure jue put them in single quotes since `:` is not a valid character in identifiers, unless they are quoted:

```text
save v('x1:out')
save i('x1:l1')
save p('x1:m1', id)
```

Also quote identifiers that contain parenthesis, e.g. 'v1:flow(br)'. 

## Default behavior

If no `save` statement appears before an analysis, the analysis writes its default set of results (equivalent to `save default`). Each analysis type defines its own default; see the individual analysis documentation.

## Clearing saves

```text
clear saves
```

Removes all accumulated save directives. Subsequent analyses revert to their defaults. `clear` with no arguments also clears saves along with variables and options.

## Analysis-specific directives

Each analysis type recognizes its own set of save directives. Directives not recognized by an analysis are silently ignored unless the `strictsave` option is set. See the individual analysis sections for the full list of supported directives.
