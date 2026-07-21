# SPICE Model Binning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Lower SPICE geometry-binned `.model` groups into VACASK `@if`/`@elseif` conditional model definitions inside the netlistrs C++ adapter, so binned FET subcircuits (e.g. Sky130 `nshort_model.1..48`) elaborate and simulate.

**Architecture:** In `lib/netlistrs.cpp`, when filling a `PTSubcircuitDefinition`, detect model cards that share a base name and carry `lmin/lmax/wmin/wmax`. Instead of emitting them as N independent `PTModel`s (leaving the bin-less name the instance references unresolved), emit one `PTBlockSequence` (an `@if`/`@elseif` chain) whose branches each define a model under the collapsed base name, guarded by that bin's scaled L/W range. VACASK's existing conditional-block elaborator (`recomputeBlockConditions`) then selects the active bin per subcircuit instance from its `l`/`w`. The Rust parser is untouched; there are zero VACASK core changes.

**Tech Stack:** C++17, VACASK ParserTables API (`PTModel`, `PTBlock`, `PTBlockSequence`, `PTSubcircuitDefinition`), cxx bridge types (`netlist::SpiceModel`, `netlist::Param`), CMake/CTest, OpenVAF/OSDI device modules.

## Global Constraints

- All changes confined to `lib/netlistrs.cpp` (plus test fixtures + `demo/api/CMakeLists.txt`). Do NOT modify the Rust parser or VACASK core.
- SPICE-origin identifiers/expressions are lowercased in this adapter (existing `lc`/`spiceId` convention). New emitted text (base model name, guard expression) must stay lowercase-consistent: `l`, `w`, `$scale` are already lowercase.
- Guard format is fixed (matches the proven Cadnip-generated `.scs`): `l*$scale >= LMIN && l*$scale < LMAX && w*$scale >= WMIN && w*$scale < WMAX`. Half-open intervals `[min, max)`. `$scale` maps to the `scale` option (`lib/ciroptions.cpp:125`).
- Bins sorted by `(lmin, wmin)` ascending; first matching branch wins.
- Emitted bin model card keeps `binunit` (a real BSIM4 param) and drops only the four selection bounds `lmin/lmax/wmin/wmax` (plus the existing dispatch-only `level` strip). No `@else` fallback: out-of-range geometry → unresolved model → loud elaboration error.
- Model-master resolution stays with the existing `spiceModelMaster` (e.g. nmos level=54 → `bsim4`).
- Build: `cmake --build build`. Test: `ctest --test-dir build -R <name> --output-on-failure`.

---

### Task 1: Extract `buildSpiceModelCard` (pure refactor, no behavior change)

Refactor the existing `addSpiceModelCard` so the PTModel-building logic is reusable by the binning path, with a name override and an extensible param-exclude set. Behavior for existing (non-binned) callers must be identical.

**Files:**
- Modify: `lib/netlistrs.cpp` (add includes; add `paramStringExcludingSet`, `spiceParamValue`, `buildSpiceModelCard`; rewrite `addSpiceModelCard:251-276` to delegate)

**Interfaces:**
- Produces:
  - `static std::string paramStringExcludingSet(const rust::Vec<netlist::Param>&, const std::set<std::string>& exclude)`
  - `static std::string spiceParamValue(const rust::Vec<netlist::Param>&, const std::string& key)` — value of first case-insensitive name match (brace/quote-stripped), else `""`
  - `static std::optional<PTModel> buildSpiceModelCard(const netlist::SpiceModel& m, const std::string& nameOverride, const std::set<std::string>& extraExclude, Parser& p)` — `std::nullopt` if no known master (warning already emitted by `spiceModelMaster`)
- Consumes (existing): `spiceModelMaster`, `spiceId`, `lc`, `stripExprQuoting`, `sv`.

- [ ] **Step 1: Add includes**

At the top include block (after `#include <sstream>`, line 9), add:

```cpp
#include <optional>
#include <cstdlib>
#include <cctype>
```

- [ ] **Step 2: Add the two param helpers**

Insert immediately before `addSpiceModelCard` (before line 245's doc comment):

```cpp
// Build a "name=value …" param string excluding a set of keys (case-insensitive).
static std::string paramStringExcludingSet(const rust::Vec<netlist::Param>& params,
                                           const std::set<std::string>& exclude) {
    std::ostringstream os;
    bool first = true;
    for (const auto& p : params) {
        std::string key = sv(p.name);
        std::string keylower = key;
        std::transform(keylower.begin(), keylower.end(), keylower.begin(), ::tolower);
        if (exclude.count(keylower)) continue;
        if (!first) os << " ";
        os << key << "=" << stripExprQuoting(sv(p.value));
        first = false;
    }
    return os.str();
}

// Value of the first parameter whose name matches `key` (case-insensitive),
// brace/quote-stripped; "" if absent.
static std::string spiceParamValue(const rust::Vec<netlist::Param>& params,
                                   const std::string& key) {
    for (const auto& p : params) {
        std::string k = sv(p.name);
        std::transform(k.begin(), k.end(), k.begin(), ::tolower);
        if (k == key) return stripExprQuoting(sv(p.value));
    }
    return "";
}
```

- [ ] **Step 3: Add `buildSpiceModelCard` and rewrite `addSpiceModelCard` to delegate**

Replace the whole existing `addSpiceModelCard` definition (`lib/netlistrs.cpp:251-276`) with:

```cpp
// Build a PTModel from a SPICE `.model` card. Returns nullopt if there is no
// known OSDI master (warning already emitted). `nameOverride` (if non-empty)
// replaces the card name — used to collapse binned cards to their base name.
// `extraExclude` names are dropped from the emitted params in addition to the
// dispatch-only `level` — used to strip binning bounds lmin/lmax/wmin/wmax.
static std::optional<PTModel> buildSpiceModelCard(const netlist::SpiceModel& m,
                                                  const std::string& nameOverride,
                                                  const std::set<std::string>& extraExclude,
                                                  Parser& p) {
    std::string mt_raw = sv(m.model_type);
    std::string master = spiceModelMaster(mt_raw, sv(m.level), "");
    if (master.empty()) return std::nullopt;

    std::string modelName = nameOverride.empty() ? sv(m.name) : nameOverride;
    PTModel mod(spiceId(modelName), Id(master.c_str()));

    std::set<std::string> excl = extraExclude;
    excl.insert("level");
    auto ps = paramStringExcludingSet(m.params, excl);
    // sp_diode has a real `level` model param (junction-cap selector); re-append.
    if (master == "sp_diode") {
        std::string lvl = sv(m.level);
        if (!lvl.empty()) ps += (ps.empty() ? "" : " ") + std::string("level=") + lvl;
    }
    if (!ps.empty()) mod.add(p.parseParameters(lc(ps)));

    std::string mt = mt_raw;
    std::transform(mt.begin(), mt.end(), mt.begin(), ::tolower);
    if      (mt == "nmos") mod.add(p.parseParameters("type=1"));
    else if (mt == "pmos") mod.add(p.parseParameters("type=-1"));
    else if (mt == "npn")  mod.add(p.parseParameters("type=1"));
    else if (mt == "pnp")  mod.add(p.parseParameters("type=-1"));

    return mod;
}

// Project one SPICE `.model` card into a PTModel and add it to `into`.
static void addSpiceModelCard(const netlist::SpiceModel& m,
                              PTSubcircuitDefinition& into, Parser& p) {
    auto mod = buildSpiceModelCard(m, "", {}, p);
    if (mod) into.add(std::move(*mod));
}
```

- [ ] **Step 4: Build**

Run: `cmake --build build`
Expected: compiles clean (no warnings about unused/undeclared).

- [ ] **Step 5: Run existing regression tests to verify no behavior change**

Run: `ctest --test-dir build -R "demo_netlistrs" --output-on-failure`
Expected: all previously-passing cases still PASS — in particular `demo_netlistrs_spice_diode` (exercises the `sp_diode` level re-append path), `demo_netlistrs_spice_res_model`, and the MOSFET case. No regressions.

- [ ] **Step 6: Commit**

```bash
git add lib/netlistrs.cpp
git commit -m "refactor(netlist): extract buildSpiceModelCard from addSpiceModelCard"
```

---

### Task 2: Detect binned model groups and emit `@if` chains

Add detection + emission and wire it into both model-emission loops. Deliverable: a binned MOSFET subcircuit elaborates, and the generated ParserTables contain the `@if` chain with correct guards.

**Files:**
- Modify: `lib/netlistrs.cpp` (add `isBinnedModel`, `binBaseName`, `emitBinnedModelGroup`, `emitSpiceModels`; replace model loops at `fillSpiceSubDef` and `spiceBlockToTables`)
- Create: `demo/api/spice_mos_bin.cir`
- Modify: `demo/api/CMakeLists.txt` (register `demo_netlistrs_spice_mos_bin`)

**Interfaces:**
- Consumes: `spiceParamValue`, `buildSpiceModelCard`, `addSpiceModelCard` (Task 1); `PTBlock::add(PTModel&&)` (`parseroutput.h:369`), `PTBlockSequence::add(Rpn&&, PTBlock&&)` (`:405`), `PTSubcircuitDefinition::add(PTBlockSequence&&)` (`:462`), `Parser::parseExpression`.
- Produces: `static void emitSpiceModels(const rust::Vec<netlist::SpiceModel>&, PTSubcircuitDefinition&, Parser&)`.

- [ ] **Step 1: Write the failing test — fixture + CTest case**

Create `demo/api/spice_mos_bin.cir` (geometry in SI meters; `$scale` defaults to 1.0, so `l*$scale == l`):

```spice
* Synthetic MOSFET binning test: two bins with distinct vth0.
* X1 geometry l=7e-7 selects bin 2 (l in [5e-7, 1e-5)).
.subckt nbin d g s b
.param l=1 w=1
M0 d g s b nbin_model l={l} w={w}
.model nbin_model.1 nmos level=49 lmin=1e-7 lmax=5e-7 wmin=1e-7 wmax=1e-3 vth0=0.40
.model nbin_model.2 nmos level=49 lmin=5e-7 lmax=1e-5 wmin=1e-7 wmax=1e-3 vth0=0.80
.ends
X1 d g s b nbin l=7e-7 w=2e-6
V1 d 0 1
V2 g 0 1
.end
```

Append to `demo/api/CMakeLists.txt` (after the existing `demo_netlistrs_spice_subckt` block, ~line 126):

```cmake
# ── SPICE MOSFET binning (@if lowering) ─────────────────────────────────────
# nbin_model.1/.2 collapse to one @if chain; X1 (l=7e-7) must bind bin 2.
# Asserts the generated ParserTables contain the @if guard for bin 2's range
# (l*$scale >= 5e-07). Program also exits 0 only if elaboration resolves the
# bin-less model name nbin_model (i.e. a bin actually bound).
add_test(NAME demo_netlistrs_spice_mos_bin
         COMMAND demo_netlistrs "${CMAKE_CURRENT_SOURCE_DIR}/spice_mos_bin.cir"
                                "${VACASK_MOD_DIR_DEFAULT}")
set_tests_properties(demo_netlistrs_spice_mos_bin PROPERTIES
    WORKING_DIRECTORY "/tmp"
    PASS_REGULAR_EXPRESSION "@if")
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build && ctest --test-dir build -R demo_netlistrs_spice_mos_bin --output-on-failure`
Expected: FAIL. Without binning support, the four `.model nbin_model.1/.2` cards are emitted as independent models named `nbin_model.1`/`nbin_model.2` (dropping the `lmin`… bounds onto the card), the instance `M0` references unresolved `nbin_model`, so either elaboration fails (nonzero exit) or the dump contains no `@if`. Either way the case does not pass.

- [ ] **Step 3: Add detection + emission helpers**

Insert after the refactored `addSpiceModelCard` (from Task 1), before `spiceSourceParams`:

```cpp
// True if the card carries all four numeric binning bounds.
static bool isBinnedModel(const netlist::SpiceModel& m) {
    return !spiceParamValue(m.params, "lmin").empty()
        && !spiceParamValue(m.params, "lmax").empty()
        && !spiceParamValue(m.params, "wmin").empty()
        && !spiceParamValue(m.params, "wmax").empty();
}

// Strip a trailing ".N" or "_N" bin suffix. "nshort_model.7" -> "nshort_model".
static std::string binBaseName(const std::string& name) {
    auto pos = name.find_last_of("._");
    if (pos != std::string::npos && pos + 1 < name.size()) {
        bool digits = true;
        for (size_t i = pos + 1; i < name.size(); ++i)
            if (!std::isdigit(static_cast<unsigned char>(name[i]))) { digits = false; break; }
        if (digits) return name.substr(0, pos);
    }
    return name;
}

// Emit one bin group as an @if/@elseif PTBlockSequence. Each branch defines a
// model under `baseName`, guarded by that bin's scaled L/W range. Bins are
// sorted by (lmin, wmin); first matching branch wins. No @else fallback.
static void emitBinnedModelGroup(std::vector<const netlist::SpiceModel*>& bins,
                                 const std::string& baseName,
                                 PTSubcircuitDefinition& into, Parser& p) {
    std::sort(bins.begin(), bins.end(),
              [](const netlist::SpiceModel* a, const netlist::SpiceModel* b) {
        double la = std::atof(spiceParamValue(a->params, "lmin").c_str());
        double lb = std::atof(spiceParamValue(b->params, "lmin").c_str());
        if (la != lb) return la < lb;
        double wa = std::atof(spiceParamValue(a->params, "wmin").c_str());
        double wb = std::atof(spiceParamValue(b->params, "wmin").c_str());
        return wa < wb;
    });

    PTBlockSequence seq;
    bool any = false;
    for (const auto* m : bins) {
        auto mod = buildSpiceModelCard(*m, baseName, {"lmin", "lmax", "wmin", "wmax"}, p);
        if (!mod) continue; // no master; warning already emitted
        std::string guard =
            "l*$scale >= "  + spiceParamValue(m->params, "lmin") +
            " && l*$scale < " + spiceParamValue(m->params, "lmax") +
            " && w*$scale >= " + spiceParamValue(m->params, "wmin") +
            " && w*$scale < " + spiceParamValue(m->params, "wmax");
        PTBlock block;
        block.add(std::move(*mod));
        seq.add(p.parseExpression(guard), std::move(block));
        any = true;
    }
    if (any) into.add(std::move(seq));
}

// Emit all `.model` cards for a block. Non-binned cards emit unchanged; binned
// cards are grouped by base name (first-seen order) into @if chains, emitted
// after the non-binned cards but before any instances (caller ordering).
static void emitSpiceModels(const rust::Vec<netlist::SpiceModel>& models,
                            PTSubcircuitDefinition& into, Parser& p) {
    std::vector<std::string> order;
    std::map<std::string, std::vector<const netlist::SpiceModel*>> groups;
    for (const auto& m : models) {
        if (isBinnedModel(m)) {
            std::string base = binBaseName(sv(m.name));
            if (!groups.count(base)) order.push_back(base);
            groups[base].push_back(&m);
        } else {
            addSpiceModelCard(m, into, p);
        }
    }
    for (const auto& base : order) emitBinnedModelGroup(groups[base], base, into, p);
}
```

- [ ] **Step 4: Wire into the two model-emission loops**

In `fillSpiceSubDef` (`lib/netlistrs.cpp:641`) replace:

```cpp
    for (const auto& m : s.models) addSpiceModelCard(m, def, p);
```

with:

```cpp
    emitSpiceModels(s.models, def, p);
```

In `spiceBlockToTables` (`lib/netlistrs.cpp:674`) replace:

```cpp
    for (const auto& m : sb.models) addSpiceModelCard(m, into, p);
```

with:

```cpp
    emitSpiceModels(sb.models, into, p);
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `cmake --build build && ctest --test-dir build -R demo_netlistrs_spice_mos_bin --output-on-failure`
Expected: PASS. `tab.dump` output contains `@if l * $scale >= 5e-07 …` (and the bin-1 branch), the program elaborates X1 to a bound `nbin_model`, and exits 0.

- [ ] **Step 6: Pin the selection assertion (run-and-observe)**

Run the demo directly to inspect the dump and confirm the *correct* bin is rendered first-match:
Run: `./build/demo/api/demo_netlistrs demo/api/spice_mos_bin.cir "$(cat build/.../VACASK_MOD_DIR)"` (use the mod dir the CTest passes; check `demo/api/CMakeLists.txt` for `VACASK_MOD_DIR_DEFAULT`).
Expected: two `@if`/`@elseif` branches printed, bin-1 guard `>= 1e-07 … < 5e-07` then bin-2 guard `>= 5e-07 … < 1e-05`, in that sorted order; `dumpHierarchy` shows `x1:m0` bound. If `dumpHierarchy` prints the bound model's `vth0`, tighten the CTest `PASS_REGULAR_EXPRESSION` to include `0.8` to assert bin 2 specifically. If `vth0` is not shown, leave the assertion at `@if` plus exit-0 (elaboration success already proves a bin bound) and note it in the commit message.

- [ ] **Step 7: Verify no regressions**

Run: `ctest --test-dir build -R "demo_netlistrs" --output-on-failure`
Expected: all cases PASS (non-binned model paths unchanged — `emitSpiceModels` routes non-binned cards through the unchanged `addSpiceModelCard`).

- [ ] **Step 8: Commit**

```bash
git add lib/netlistrs.cpp demo/api/spice_mos_bin.cir demo/api/CMakeLists.txt
git commit -m "feat(netlist): lower SPICE model binning to @if chains in adapter"
```

---

### Task 3: End-to-end validation on real Sky130 nfet_01v8

Validate against the real 48-bin `sky130_fd_pr__nfet_01v8` model + BSIM4 OSDI. Registered only when the PDK and `bsim4v8.osdi` are present at configure time (follows the existing skip-if-absent norm), so CI without the PDK stays green.

**Files:**
- Create: `demo/api/sky130_nfet_bin.cir`
- Modify: `demo/api/CMakeLists.txt` (conditionally register `demo_netlistrs_sky130_nfet_bin`)

**Interfaces:**
- Consumes: the full binning path from Task 2; the demo harness's existing sky130 branch (`demo_netlistrs.cpp:56` sets `scale=1e-6`; `:244` runs `op` and prints `Analysis OK (sky130 op)` for paths containing `sky130`).

- [ ] **Step 1: Create the E2E fixture**

Create `demo/api/sky130_nfet_bin.cir`. Set `SKY130_PDK` (below) to the absolute path of the PDK checkout (`/home/pepijn/code/nyanodide/skywater-pdk-libs-sky130_fd_pr`). The include pulls in the wrapper subckt + 48 binned models; X1 instantiates one FET at a mid-range geometry:

```spice
* Sky130 nfet_01v8 binning E2E (path contains "sky130" → demo sets scale=1e-6, runs op)
.include SKY130_PDK/combined_models/continuous/models_fet/sky130_fd_pr__nfet_01v8.spice
X1 d g 0 0 sky130_fd_pr__nfet_01v8 l=0.15 w=1.0
V1 d 0 1.8
V2 g 0 1.8
.end
```

(Geometry in microns per Sky130 convention: `l=0.15`, `w=1.0`; with `scale=1e-6` the guard sees `l*$scale = 1.5e-7`, `w*$scale = 1e-6`, selecting the matching bin. The nfet_01v8 `op` is known to converge.)

- [ ] **Step 2: Conditionally register the CTest**

Append to `demo/api/CMakeLists.txt`:

```cmake
# ── Sky130 nfet_01v8 binning E2E (only when PDK + BSIM4 OSDI are present) ────
set(SKY130_NFET "/home/pepijn/code/nyanodide/skywater-pdk-libs-sky130_fd_pr/combined_models/continuous/models_fet/sky130_fd_pr__nfet_01v8.spice")
if(EXISTS "${SKY130_NFET}" AND EXISTS "${VACASK_MOD_DIR_DEFAULT}/bsim4v8.osdi")
    add_test(NAME demo_netlistrs_sky130_nfet_bin
             COMMAND demo_netlistrs "${CMAKE_CURRENT_SOURCE_DIR}/sky130_nfet_bin.cir"
                                    "${VACASK_MOD_DIR_DEFAULT}")
    set_tests_properties(demo_netlistrs_sky130_nfet_bin PROPERTIES
        WORKING_DIRECTORY "/tmp"
        PASS_REGULAR_EXPRESSION "Analysis OK \\(sky130 op\\)")
else()
    message(STATUS "Skipping demo_netlistrs_sky130_nfet_bin (Sky130 PDK or bsim4v8.osdi absent)")
endif()
```

Then replace the literal `SKY130_PDK` token in `sky130_nfet_bin.cir` with the same absolute PDK path (relative `.include` resolves against the fixture's directory otherwise; use the absolute path for determinism).

- [ ] **Step 3: Configure + build + run**

Run: `cmake -S . -B build && cmake --build build && ctest --test-dir build -R demo_netlistrs_sky130_nfet_bin --output-on-failure`
Expected: PASS with `Analysis OK (sky130 op)` — the 48 bins collapse to one `@if` chain, X1's geometry selects the correct bin, BSIM4 elaborates, and the DC operating point converges. If the case is skipped, confirm the STATUS message names the missing dependency and resolve it (build/install `bsim4v8.osdi`, or point `SKY130_NFET` at the checkout).

- [ ] **Step 4: Manual sanity check of bin selection (optional, documented)**

Run the demo directly and inspect `dumpHierarchy` for `x1` to confirm a single BSIM4 model bound (not 48). Confirm switching X1's `l`/`w` to a different bin's range changes which branch is active (re-run, observe the `op` still converges). This is a manual confirmation; no CTest.

- [ ] **Step 5: Commit**

```bash
git add demo/api/sky130_nfet_bin.cir demo/api/CMakeLists.txt
git commit -m "test(netlist): E2E Sky130 nfet_01v8 binning through adapter"
```

---

## Self-Review

**Spec coverage:**
- Detection (`lmin/lmax/wmin/wmax` + base-name strip) → Task 2 (`isBinnedModel`, `binBaseName`). ✓
- Emission as `@if` `PTBlockSequence`, base-name collapse, sort by `(lmin,wmin)`, first-match, guard format, no `@else`, strip four bounds / keep `binunit` → Task 2 (`emitBinnedModelGroup`, `buildSpiceModelCard` exclude set). ✓
- Subckt-body-only scope → Task 2 wires exactly the two subckt/spice-block model loops; top-level `mergeNetlist` model loop (`makeModel`, Spectre-origin) is untouched. ✓
- `$scale` reference risk → resolved: `lib/ciroptions.cpp:125` maps `$scale`→`scale`; guard uses `l*$scale`/`w*$scale`. ✓
- BSIM4 OSDI availability risk → Task 3 is guarded on `bsim4v8.osdi` presence. ✓
- Testing: synthetic (Task 2) + real E2E (Task 3). ✓

**Placeholder scan:** No TBD/TODO. The only "run-and-observe" steps (Task 2 Step 6, Task 3 Step 4) are legitimate empirical pins/manual checks on a text-dump harness, each with a concrete default assertion (`@if` + exit-0) that stands without them.

**Type consistency:** `buildSpiceModelCard` returns `std::optional<PTModel>` and is consumed via `if (mod) … std::move(*mod)` in both `addSpiceModelCard` and `emitBinnedModelGroup`. `spiceParamValue`/`paramStringExcludingSet` signatures match all call sites. `emitSpiceModels(const rust::Vec<netlist::SpiceModel>&, …)` matches both `s.models` and `sb.models` field types.
