# Setting Simulator Options

## Syntax

```text
options name=value [name=value ...]
```

Multiple options can appear on one line or across multiple `options` statements:

```text
options temp=85 tnom=27
options reltol=1e-4 abstol=1e-13
```

## Expressions

Option values accept arithmetic expressions. Circuit variables and constants can be used:

```text
var tbase=30
options temp=tbase+50 reltol=1e-3
```

## Scope and persistence

Options accumulate across the control block. Once set, an option remains in effect for all subsequent analyses until changed or cleared. Setting an option a second time overwrites the previous value:

```text
options temp=27
analysis op1 op    // runs at 27 °C

options temp=125
analysis op2 op    // runs at 125 °C
```

## Clearing options

```text
clear options
```

Resets all options to their defaults. `clear` with no arguments also clears options together with variables and saves.

To reset a single option to its default, set it explicitly:

```text
options temp=27
```

## When options take effect

Options are applied before each analysis runs, not immediately when the `options` statement executes. If an option affects circuit elaboration (see [Options Causing Circuit Reelaboration](cmd-options-reelaborate.md)), the affected parts of the hierarchy are re-elaborated automatically before the next analysis.
