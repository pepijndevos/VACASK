# Modifying Parameters

The `alter` command changes one or more parameters of a named model or instance from the control block. Changes take effect before the next analysis; no explicit re-elaboration is required.

## Syntax

```text
alter model("name" [, "name2" ...]) param1=val1 [param2=val2 ...]
alter instance("name" [, "name2" ...]) param1=val1 [param2=val2 ...]
```

The keyword selects whether the target is a model or an instance. One or more names are supplied as string expressions inside parentheses. The parameter assignments use keyword-argument syntax: the right-hand side of each `=` is an expression evaluated in the current variable context.

If the circuit has not yet been elaborated when `alter` runs, it performs a default elaboration first.

## Modifying instance parameters

```text
alter instance("r1") r=2k
```

Changes parameter `r` on instance `r1`. The new value is used starting from the next analysis.

Multiple instances can be updated in one statement:

```text
alter instance("r1", "r2") r=1k
```

## Modifying model parameters

```text
alter model("nmos1") tox=5n
```

Changes parameter `tox` on model `nmos1`.

## Multiple parameters

Several parameters can be set in a single `alter`:

```text
alter instance("v1") type="sine" freq=1k ampl=1
```

## Example: switching a source type between analyses

```text
Switched-source example

ground 0
load "vsource.osdi"
model vs vsource
v1 (in 0) vs type="dc" dc=5
r1 (in 0) vs r=1k

control
  abort always
  analysis op1 op

  alter instance("v1") type="pulse" val0=0 val1=5 delay=5n rise=1n fall=1n width=500n period=1u
  analysis tran1 tran stop=3u step=1n
endc
```

The DC operating point uses `v1` as a DC source. After `alter`, the same instance becomes a pulse source for the transient run. The circuit topology is unchanged, so no re-elaboration is triggered by `alter` itself; the next analysis (`tran1`) picks up the parameter change automatically.
