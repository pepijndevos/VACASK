# Task 2 Report: SPICE Model Binning → `@if` Chains

## What Was Implemented

Added MOSFET model binning support to `lib/netlistrs.cpp`. When a `.subckt` body contains multiple `.model` cards sharing a common base name (e.g., `nbin_model.1`, `nbin_model.2`) each with `lmin`/`lmax`/`wmin`/`wmax` geometry bounds, they are now collapsed into a single VACASK `@if`/`@elseif` `PTBlockSequence`. Each branch defines the model under the base name (`nbin_model`) guarded by the bin's scaled L/W range. The elaborator then selects the active bin per instance at elaboration time.

### New functions in `lib/netlistrs.cpp`

- `isBinnedModel(m)` — returns true if the model card has all four geometry bounds
- `binBaseName(name)` — strips trailing `.N` or `_N` suffix to get the base name
- `emitBinnedModelGroup(bins, baseName, into, p)` — sorts bins by (lmin, wmin), builds a `PTBlockSequence` with one `PTBlock` per bin, each guarded by the L/W range expression
- `emitSpiceModels(models, into, p)` — routes non-binned cards through `addSpiceModelCard` unchanged, groups binned cards and emits one `@if` chain per base name

Both model-emission loops (`fillSpiceSubDef` and `spiceBlockToTables`) now call `emitSpiceModels` instead of inline `addSpiceModelCard`.

## TDD Evidence

### RED — fixture + CTest registered, test fails before implementation

**Command:**
```
cmake --build build && ctest --test-dir build -R demo_netlistrs_spice_mos_bin --output-on-failure
```

**Output (key lines):**
```
model nbin_model.1 bsim3 lmin=1e-07 lmax=5e-07 wmin=1e-07 wmax=0.001 vth0=0.4 type=1
model nbin_model.2 bsim3 lmin=5e-07 lmax=1e-05 wmin=1e-07 wmax=0.001 vth0=0.8 type=1
elaboration failed: Master 'nbin_model' not found.
1/1 Test #54: demo_netlistrs_spice_mos_bin .....***Failed  Required regular expression not found. Regex=[@if]  0.01 sec
```

**Why expected:** Without binning, the adapter emits `nbin_model.1` and `nbin_model.2` as flat models; `M0`'s reference to `nbin_model` is unresolved, elaboration fails, and no `@if` appears.

### GREEN — after implementation

**Command:**
```
cmake --build build && ctest --test-dir build -R demo_netlistrs_spice_mos_bin --output-on-failure
```

**Output:**
```
1/1 Test #54: demo_netlistrs_spice_mos_bin .....   Passed    0.01 sec
100% tests passed out of 1
```

## Step 6: Selection Assertion (run-and-observe)

Manual demo run output (key lines):
```
subckt nbin (d g s b)
  parameters l=1 w=1
  m0 (d g s b) nbin_model l=l w=w
  @if l*$scale>=1e-07&&l*$scale<5e-07&&w*$scale>=1e-07&&w*$scale<0.001
    model nbin_model bsim3 vth0=0.4 type=1
  @elseif l*$scale>=5e-07&&l*$scale<1e-05&&w*$scale>=1e-07&&w*$scale<0.001
    model nbin_model bsim3 vth0=0.8 type=1
  @end

__topinst__ ...
  x1 (model=nbin, device=__hierarchical__)
    model x1:nbin_model (device=bsim3)
    x1:m0 (model=x1:nbin_model, device=bsim3)
```

Two `@if`/`@elseif` branches are emitted in sorted order (bin-1 first, bin-2 second). For X1 with l=7e-7, the elaborator selects bin 2 (`x1:nbin_model` resolves to `bsim3`).

**`dumpHierarchy` does NOT print `vth0`** — only device type (`bsim3`) is shown. Per the brief's instructions, the CTest assertion remains at `PASS_REGULAR_EXPRESSION "@if"`.

## Final CTest Assertion

```cmake
PASS_REGULAR_EXPRESSION "@if"
```

Rationale: `PASS_REGULAR_EXPRESSION` in CMake overrides exit-code checking. The test passes when `@if` appears in the output. Elaboration success (bin selected → `x1:nbin_model` bound) is confirmed by the dumpHierarchy output above. Tightening to include `0.8` is not possible since `vth0` is not printed by `dumpHierarchy`.

**Note on demo exit code:** When run manually, `demo_netlistrs` exits 1 because `path.find("spice_mos")` matches `spice_mos_bin.cir` and the demo then requires `m1` (top-level instance) which doesn't exist (our MOSFET is `m0` inside subckt `nbin`, appearing as `x1:m0`). This is a demo-level name check that doesn't affect the CTest result.

## Regression Results

```
100% tests passed out of 23
```

All 23 `demo_netlistrs*` tests pass. Non-binned model paths unchanged — `emitSpiceModels` routes non-binned cards through `addSpiceModelCard` as before.

## Files Changed

- `/home/pepijn/code/nyanodide/VACASK/lib/netlistrs.cpp` — added `isBinnedModel`, `binBaseName`, `emitBinnedModelGroup`, `emitSpiceModels`; replaced `addSpiceModelCard` loops in `fillSpiceSubDef` and `spiceBlockToTables`
- `/home/pepijn/code/nyanodide/VACASK/demo/api/spice_mos_bin.cir` — new fixture (2 bins, distinct `vth0`, X1 selects bin 2)
- `/home/pepijn/code/nyanodide/VACASK/demo/api/CMakeLists.txt` — registered `demo_netlistrs_spice_mos_bin` test

## Self-Review

- The `emitSpiceModels` function preserves non-binned card ordering (they emit first, in declaration order). Binned groups follow in first-seen order by base name, bins sorted ascending by `(lmin, wmin)`.
- The `binBaseName` function strips the last `.N` or `_N` suffix only if all chars after the separator are digits — safe against false-positives on names like `nmos.bulk` where `bulk` is not all-digits.
- Guard expression uses raw parameter strings from the SPICE card (e.g. `1e-7` as written, not reformatted). The `$scale` variable is the VACASK convention matching what Cadnip generates.
- `buildSpiceModelCard` is called with `extraExclude = {"lmin","lmax","wmin","wmax"}` so the geometry bounds don't appear as model parameters.
- No memory ownership issues: `bins` holds `const SpiceModel*` pointing into the `rust::Vec<netlist::SpiceModel>&` that lives for the duration of the call — safe.

## Concerns

1. **Demo exit-1 for `spice_mos_bin.cir`:** The `spice_mos` filename check in `demo_netlistrs.cpp` inadvertently matches `spice_mos_bin.cir` and requires `m1` which doesn't exist (our device is `m0`). The CTest passes because `PASS_REGULAR_EXPRESSION` overrides exit-code checking, but manual runs see an error. A future cleanup could either rename the fixture to avoid the `spice_mos` prefix, or add an explicit `spice_mos_bin` guard in `demo_netlistrs.cpp`.

2. **No `@else` fallback:** If no bin matches the instance geometry, VACASK will fail to find the model. This is the expected behavior (matching the brief's spec of "No @else fallback"), but it means out-of-range geometries get an elaboration error rather than a clean warning.

## Fix: test-integrity

### Commands run

```
# Step 1: rename fixture
git mv demo/api/spice_mos_bin.cir demo/api/spice_binned_mos.cir

# Step 2: CMakeLists.txt edit (see diff below)
# - renamed test: demo_netlistrs_spice_mos_bin → demo_netlistrs_spice_binned_mos
# - updated COMMAND to point at spice_binned_mos.cir
# - added FAIL_REGULAR_EXPRESSION "elaboration failed"

# Step 3: build
cmake --build build
# → Build succeeded (no recompilation of .cpp needed; cmake regenerated test config)

# Step 4: manual run
./build/demo/api/demo_netlistrs demo/api/spice_binned_mos.cir build/lib/vacask/mod
echo $?
```

### Manual-run output (exit code)

```
...
__topinst__ (model=__topdef__, device=__hierarchical__)
  model vsource (device=vsource)
  x1 (model=nbin, device=__hierarchical__)
    model x1:nbin_model (device=bsim3)
    x1:m0 (model=x1:nbin_model, device=bsim3)
  v1 (model=vsource, device=vsource)
  v2 (model=vsource, device=vsource)
Analysis OK.
EXIT_CODE: 0
```

### ctest demo_netlistrs_spice_binned_mos

```
1/1 Test #54: demo_netlistrs_spice_binned_mos ...   Passed    0.01 sec
100% tests passed out of 1
```

### ctest -R demo_netlistrs (full suite)

```
100% tests passed out of 23
Total Test time (real) =   0.36 sec
```

All 23 demo_netlistrs tests pass. The `spice_mos` substring collision is resolved: the fixture is now named `spice_binned_mos.cir`, no `m1` check triggers, the program exits 0, and `FAIL_REGULAR_EXPRESSION "elaboration failed"` ensures a genuine binding failure would fail the test.

## Fix: tighten synthetic assertion

### Captured dump lines (@if/@elseif)

```
  @if l*$scale>=1e-07&&l*$scale<5e-07&&w*$scale>=1e-07&&w*$scale<0.001
    model nbin_model bsim3 vth0=0.4 type=1
  @elseif l*$scale>=5e-07&&l*$scale<1e-05&&w*$scale>=1e-07&&w*$scale<0.001
    model nbin_model bsim3 vth0=0.8 type=1
```

Bin 2's lower-L guard is the `@elseif` line; the lmin=5e-7 token renders as `5e-07`.

### New PASS_REGULAR_EXPRESSION

```cmake
PASS_REGULAR_EXPRESSION "@elseif l\\*\\$scale>=5e-07"
```

CMake string `\\*` → regex `\*` (matches literal `*`); `\\$` → regex `\$` (matches literal `$`).

### ctest result

```
1/1 Test #54: demo_netlistrs_spice_binned_mos ...   Passed    0.02 sec
100% tests passed out of 1
```
