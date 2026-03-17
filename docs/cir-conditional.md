# Conditional Netlist Blocks

Conditional blocks let you include or exclude portions of a netlist based on the value of a parameter expression. They are the primary mechanism for creating configurable netlists where topology or device choices depend on parameters.

## Syntax

```text
@if condition
  // netlist statements
@elseif condition2
  // netlist statements
@else
  // netlist statements
@end
```

`@elseif` and `@else` branches are optional. There may be any number of `@elseif` branches. The `@end` keyword closes the block.

Each `condition` is a scalar expression using any combination of parameters, circuit variables, builtin constants, and operators. The result is interpreted as a boolean: any nonzero number or non-empty string is true; zero or an empty string is false.

## Allowed contents

Inside a conditional branch you may place:

- Instance declarations
- Model declarations
- Nested `@if` ... `@end` blocks

## Evaluation

Conditions are evaluated during circuit elaboration, after parameters have been resolved. Branches are tested in order; the first branch whose condition is true is activated and the rest are skipped. If no branch is true and there is no `@else`, the block contributes nothing.

Because evaluation happens at elaboration time, the topology actually assembled — nodes, instances, models — depends on the parameter values in effect at that moment. If elaboration is re-triggered (e.g., because a swept parameter crosses a boundary), conditions are re-evaluated.

## Example

```text
parameters fast=0

R1 in out r=fast ? 50 : 1k

@if fast
  // bypass capacitor only in fast mode
  C1 out 0 c=100f
@end
```

```text
parameters corner="tt"

@if corner == "ff"
  model nmos_fast nmos ...
@elseif corner == "ss"
  model nmos_slow nmos ...
@else
  model nmos_tt nmos ...
@end
```

## Nesting

Conditional blocks may be nested to any depth:

```text
@if en_block_a
  @if variant == 1
    X1 ...
  @else
    X2 ...
  @end
@end
```
