# SPICE Model Binning in the netlistrs Adapter

**Date:** 2026-07-21
**Branch:** `fix/spice-resistor-model-param`
**Status:** Design approved, pending implementation plan

## Problem

SPICE MOSFET models are commonly *binned*: a base model name (e.g. `nshort_model`)
is backed by many `.model` cards (`nshort_model.1` … `nshort_model.48`), each valid
for a device-geometry range expressed via `lmin`/`lmax`/`wmin`/`wmax`. The simulator
must select the card whose L/W range contains the instance geometry.

VACASK has **no native binning engine**. The `paramset`-based selector
(`paramset <group> model <name> (selectors)` with `par()`/`modpar()`) exists only as
a design comment in `simulator/cmd.cpp:1145-1242` and is unimplemented. There is also
no geometry-based selection in the OSDI/BSIM4 layer — models are resolved by exact name.

This blocks Sky130 FET simulation through the Rust-parser → netlistrs adapter path:
the wrapper subckt instantiates the bin-less name `nshort_model`, which resolves to
nothing today.

## Chosen approach

Lower binning in the **netlistrs C++ adapter** (`lib/netlistrs.cpp`) by synthesizing
VACASK's supported `@if`/`@elseif` conditional netlist blocks — the same mechanism the
Cadnip.jl converter already uses and proved correct. This keeps the Rust parser a pure
pass-through, requires **zero VACASK core changes**, and reuses machinery already
present in the adapter (`makeConditional` builds a `PTBlockSequence`; `PTBlock::add(PTModel&&)`
lets a conditional branch carry a model card).

`@if` blocks are evaluated **per subcircuit instance at elaboration time, after
parameters resolve** (`recomputeBlockConditions`, `lib/hierdevice.cpp:483`), reading the
wrapper subckt's `l`/`w`. That is exactly the semantics binning needs.

Alternatives rejected:
- **Native VACASK binning** (`paramset`): cleanest user-facing semantics and would serve
  the native `.sim` parser too, but a large effort plus an architectural change (VACASK
  builds models before instances, so per-instance geometry selection would require
  reworking that ordering). Left open as a possible future upstream improvement.
- **Generate `@if` in a Rust transform pass**: violates the parser's explicit
  no-evaluation / pass-through contract.

## Structural fact that makes this clean

The real Sky130 file `combined_models/continuous/models_fet/sky130_fd_pr__nfet_01v8.spice`
is a single wrapper subckt:

```
.subckt sky130_fd_pr__nfet_01v8 d g s b mult=1
.param l=1 w=1 ...                         ; l and w are subckt-scope params
Msky130_fd_pr__nfet_01v8 d g s b nshort_model l={l} w={w} ...   ; references bin-less name
.model nshort_model.1 nmos + level=54 lmin=8E-6 lmax=2.02E-5 wmin=7E-6 wmax=1.01E-3 binunit=2 ...
.model nshort_model.2 nmos + ...
... (48 cards)
.ends sky130_fd_pr__nfet_01v8
```

`l` and `w` are already subckt-scope parameters, so the `@if` guard has the right scope
with no cross-scope movement. The adapter replaces the 48 independent model cards with
one `@if` chain inside the same `PTSubcircuitDefinition`; the existing instance resolves
to whichever branch is live.

## Design

### Location
`lib/netlistrs.cpp`, in the subckt-fill path (`fillSpiceSubDef`, which already emits
subckt-local `.model` cards via `addSpiceModelCard`). A pre-pass groups the subckt's
model cards before emission.

### Detection (mirrors Cadnip `is_binned_model` / base-name regex)
- A model card is a *bin* if it carries numeric `lmin`, `lmax`, `wmin`, `wmax`.
- Base name = strip trailing `.N` (and `_N`) via `^(.+)[._](\d+)$`
  (`nshort_model.7` → `nshort_model`).
- Cards sharing a base name form a bin group. Non-binned cards emit unchanged
  (current behavior preserved).

### Emission
For each bin group, instead of N `PTModel`s, build one `PTBlockSequence` (`@if` chain):
- Sort bins by `(lmin, wmin)`; **first match wins** (Cadnip semantics).
- Each branch condition (Cadnip's exact guard, half-open intervals, scaled geometry):
  `l*$scale >= lmin && l*$scale < lmax && w*$scale >= wmin && w*$scale < wmax`
- Each branch body: one `PTModel` named with the **base name** (`nshort_model`), device
  master resolved as today (`spiceModelMaster`), carrying that bin's params **minus** the
  four selection bounds `lmin`/`lmax`/`wmin`/`wmax` (selection-only). Keep `binunit`
  (a real BSIM4 parameter-interpolation input), plus `level`/`version` as handled by
  existing master logic.
- **No `@else` fallback** (matches Cadnip): out-of-range geometry → no model → loud
  elaboration error, which is the correct failure mode.

### Scope
**Subckt-body only.** Every Sky130 binned group lives inside its wrapper subckt.
Top-level / lib-level binning is out of scope (YAGNI); add only if a real netlist needs it.

## Risks / to confirm during implementation
1. **`$scale` reference** — the guard must compare against the same scaled L/W the device
   sees. Cadnip emits `l*$scale`. Confirm `$scale`/`scale` is referenceable inside a
   VACASK `@if` condition (docs say conditions may read "circuit variables"). If not
   directly referenceable, use whatever scaled quantity VACASK exposes.
2. **BSIM4 OSDI availability** — E2E validation needs the `bsim4` `.osdi` module loadable
   and `spiceModelMaster` mapping `.model … nmos level=54` to it. This is a *test*
   dependency, independent of the binning logic.

## Testing
- **Synthetic unit/demo:** a 2–3-bin model group; assert the transform produces the
  `@if` chain and that two different geometries select the expected bins.
- **E2E:** real `sky130_fd_pr__nfet_01v8` → parse → adapter → elaborate → `op` at a known
  geometry; assert exactly one bin binds and the analysis converges (README FET `op`
  already converges).

## References
- Cadnip converter: `Cadnip.jl/SpiceArmyKnife.jl/src/cg_spectre.jl:399-571`
  (detection, base-name, `generate_binned_models` `@if` emission), `simulator_traits.jl:307-310`
  (`binningsupport(::VACASK)=false`).
- VACASK conditionals: `docs/cir-conditional.md`, `docs/cir-binning.md`;
  `lib/hierdevice.cpp:483` (`recomputeBlockConditions`); unimplemented `paramset`
  design comment `simulator/cmd.cpp:1145-1242`.
- Adapter machinery: `lib/netlistrs.cpp:107` (`makeConditional`),
  `include/parseroutput.h:369` (`PTBlock::add(PTModel&&)`), `:405` (`PTBlockSequence::add`).
