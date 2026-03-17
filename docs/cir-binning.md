# Automated Binning

Binning is a technique used in process design kits to improve model accuracy across a range of device geometries. A PDK may supply several model variants — bins — each calibrated for a specific window of width and length. The circuit description selects the appropriate bin based on the actual device dimensions.

In VACASK, binning is implemented with [conditional netlist blocks](cir-conditional.md). A wrapper subcircuit receives the geometry parameters and uses `@if`/`@elseif`/`@else` to instantiate the correct binned model.

## Pattern

Define the binned models, then write a wrapper subcircuit that selects among them:

```text
// Binned model definitions
model nmos_0 bsim3v3 tox=6n   vth0=0.45 ...   // 1u <= W <  2u, 1u <= L <  2u
model nmos_1 bsim3v3 tox=6n   vth0=0.43 ...   // 1u <= W <  2u, 2u <= L <= 5u
model nmos_2 bsim3v3 tox=6.1n vth0=0.44 ...   // 2u <= W <= 5u, 1u <= L <  2u
model nmos_3 bsim3v3 tox=6.1n vth0=0.42 ...   // 2u <= W <= 5u, 2u <= L <= 5u

// Wrapper subcircuit
subckt nmos (d g s b)
parameters w=1.5u l=1u
  @if 1u <= w && w < 2u  && 1u <= l && l <  2u
    m (d g s b) nmos_0 w=w l=l
  @elseif 1u <= w && w < 2u  && 2u <= l && l <= 5u
    m (d g s b) nmos_1 w=w l=l
  @elseif 2u <= w && w <= 5u && 1u <= l && l <  2u
    m (d g s b) nmos_2 w=w l=l
  @elseif 2u <= w && w <= 5u && 2u <= l && l <= 5u
    m (d g s b) nmos_3 w=w l=l
  @else
    m (d g s b) nmos_not_found w=w l=l
  @end
ends
```

Instances use the wrapper name and pass actual geometry:

```text
m1 (drain gate source bulk) nmos w=1u l=1.5u
m2 (drain gate source bulk) nmos w=2u l=1u
```

During elaboration each instance evaluates the `@if` chain using its own `w` and `l` values and connects to exactly one binned model.

## Guidelines

- Keep binned model definitions in a separate include file and include it with the wrapper subcircuit.
- Cover all geometry combinations. Use `@else` as a catch-all for geometries outside the defined bins, or have it issue an elaboration error via referring to a nonexistent model. 
- The condition expressions may reference any parameters visible in the subcircuit scope, including computed quantities such as area or perimeter.
