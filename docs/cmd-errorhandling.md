# Error Handling

The `abort` command controls whether a failed step stops the entire run or lets the control block continue. It can be changed at any point in the control block and takes effect immediately.

## Syntax

```text
abort always
abort never
abort on cmd1 [cmd2 ...]
abort except cmd1 [cmd2 ...]
```

The valid command names are: `analysis`, `alter`, `clear`, `elaborate`, `options`, `postprocess`, `print`, `save`, `sweep`, `var`.

## Default behavior

By default the simulator aborts on any error except a failed `analysis`. A failed analysis is reported to standard error and the control block continues with the next statement. All other command failures stop the run immediately.

## Variants

**`abort always`** — stop the run on any error, including failed analyses. Use this when all steps must succeed for the result to be valid.

**`abort never`** — print errors to standard error and continue regardless. Useful for exploratory runs where some analyses are expected to fail.

**`abort on cmd1 cmd2 ...`** — abort only when the listed commands fail; ignore errors in everything else.

**`abort except cmd1 cmd2 ...`** — abort on every error except failures in the listed commands.

Each `abort` statement replaces the previous policy completely.

## Examples

Stop on every error, including analysis failures:

```text
control
  abort always
  analysis op1 op
  analysis ac1 ac from=1 to=1G mode="dec" points=10
endc
```

Allow analyses to fail but stop on any other error (default behavior):

```text
control
  abort except analysis
  analysis op1 op
  analysis ac1 ac from=1 to=1G mode="dec" points=10
endc
```

Run everything and never stop:

```text
control
  abort never
  analysis op1 op
  analysis tran1 tran stop=10u step=10n
endc
```

Change policy mid-run — abort on analysis failures during a critical section, then relax it:

```text
control
  abort always
  analysis op1 op           // must succeed

  abort except analysis
  analysis ac1 ac from=1 to=1G mode="dec" points=10   // allowed to fail
  analysis noise1 noise out=["out"] in="v1" from=1 to=1G mode="dec" points=10
endc
```
