# Bug Audit — `feature/absdelay`

Audit of the 12-commit `absdelay()` feature branch (delay-line support across
OP / DC / transient / AC-family / HB / HBAC, plus OSDI 0.5 descriptor plumbing).
~2000 lines added over `main`.

## Summary

| # | Severity | Area | One-line |
|---|----------|------|----------|
| 1 | ~~HIGH~~ LOW (FIXED) | `osdifile.cpp`, `osdiinstance.cpp` | OSDI 0.4 models: unguarded reads of 0.5-only descriptor fields (UB-on-paper; not a live crash for real files) |
| 2 | MEDIUM (FIXED) | `coretrannr.cpp` | Variable `absdelay()` in transient silently used a delay frozen at the operating point |
| 3 | NOT A BUG | `anhb.cpp` | HB Jacobian bucket `false` → `true` — intentional (avoids the 0-stride MatrixView hack) |
| 4 | LOW (FIXED) | `osdifile.h` | `OsdiFile::experimental` not initialized at declaration |
| 5 | LOW (FIXED) | `coretrannr.cpp` | Delay contributions loaded even after an eval failure (inconsistent with `OpNRSolver`) |
| 6 | INFO (FIXED) | `ansupport.h` | `CircularBuffer::upsize` signed/unsigned loop counter (pre-existing, benign) |

Items 1, 2, 4, 5 and 6 are fixed. 3 is intentional (not a bug). Nothing open.

---

## Findings

### 1. LOW (FIXED) — OSDI 0.4 models: unguarded reads of 0.5-only descriptor fields

- [x] **Fixed** — `osdifile.cpp` loop wrapped in `if (experimental)`; `osdiinstance.cpp:1625`
  switched to the `experimental`-checked `absdelayCount()` accessor. All other
  `absdelay_count` / `absdelays` accesses were already guarded (via the checked
  accessors, or loops with a bound of 0 for a v0.4 file).

**Original severity (HIGH) was wrong.** Empirical check: real in-use v0.4 files
(`IHP-Open-PDK/.../psp103.osdi`, `r3_cmc.osdi`, `mosvar.osdi`, `refpss/*`) all
report `descriptorSize == 352` exactly; `sizeof(OsdiDescriptor04) == 352`,
`offsetof(absdelay_count) == 348`. So the pre-fix `desc->absdelay_count` read
landed in the descriptor's own tail padding (reads 0), the loop never ran, and
`desc->absdelays` (offset 352, genuinely past the slot) was never dereferenced.
Older v0.4 files (`descriptorSize` 296-344, 68 in the tree) are rejected by the
size gate on both `main` and this branch - not a regression. The fix is still
worth keeping: it stops depending on "padding happens to be zero".

The branch adds OSDI **0.5** support and keeps 0.4 as a supported "legacy"
input (`include/osdi04legacy.h`, the `experimental` flag, version gate accepts
`0.4` and `0.5`). Before the fix, two paths read the 0.5-only descriptor fields
without an `experimental` check:

- `lib/osdifile.cpp` per-descriptor loop over `desc->absdelay_count` /
  `desc->absdelays[j]` — **now wrapped in `if (experimental)`**.
- `lib/osdiinstance.cpp:1625` `loadCore()`: `auto n = descr->absdelay_count;` —
  **now `auto n = model()->device()->absdelayCount();`** (the accessor returns 0
  for non-experimental files). Reachable for every OSDI instance because
  `loadSetup.delayLines_` is non-null on OP/HB solves.

All other `absdelay_count` / `absdelays` accesses were already guarded — via the
`experimental`-checked `absdelayCount()` / `absdelays()` accessors, or inside
loops whose bound is 0 for a v0.4 file. `circuit.delayHistoryCount()` stays 0 for
a pure-v0.4 circuit (it only grows through `allocateDelayHistory(absdelayCount())`),
so every downstream `if (nDelay>0)` block is skipped too.

---

### 2. MEDIUM (FIXED) — Variable `absdelay()` in transient used a frozen (DC) delay

- [x] **Fixed**

`TranNRSolver` (`lib/coretrannr.cpp`) stored `tranDelayLines_` but **never set
`loadSetup_.delayLines_`** — it passed `nullptr` to the `OpNRSolver` base, so
the inherited `loadSetup_` kept that null and `OsdiInstance::loadCore()`'s
per-iteration `setDelay()` path never ran during transient stepping.
`delay_[slot]` was written once, by the initial OP solve, then held constant —
so `absdelay(x, td, maxdelay)` with a non-constant `td` produced a wrong answer,
silently (constant `td`, including the branch's own tests, was unaffected).

**Fix applied (4 edits):**

1. `lib/coretrannr.cpp` ctor — pass `delayLines` (not `nullptr`) to the
   `OpNRSolver` base, keep `nullptr` for the bindings.
2. `lib/coreopnr.cpp` `buildSystem()` — the static passthrough delay stamp
   (`-out + in = 0`) is now gated on `delayLines_ && delayBindings_`, so with a
   null `delayBindings_` (the TranNRSolver case) `OpNRSolver` refreshes the delay
   values via `loadCore()` but does **not** load the stamp; `TranNRSolver::
   buildSystem()` loads the transient delay residual/Jacobian instead.
3. `lib/coretrannr.cpp` ctor body — `loadSetup_.firstTimepoint = false` for the
   per-timestep loads, so a variable delay's `td` keeps refreshing but `maxDelay`
   (and a no-maxdelay delay's frozen `td`) stay at the t=0 seed.
4. `lib/coretran.cpp` — `lsInit` now carries `.delayLines_` so the t=0 seed of
   `td` / `maxDelay` happens for both IC modes (OP and UIC), not just OP.

---

### 3. NOT A BUG — HB Jacobian bucket size changed `false` → `true`

- [x] **Intentional** (confirmed by author)

`lib/anhb.cpp` dropped the explicit `jacColoc(true)` / `jac(false)` initializers;
`jac` (`bsjac`, the HB Jacobian) now default-constructs with
`KluBlockSparseMatrixCore(largeBucket=true)`. This is deliberate: making every
block matrix use a full `nbRow_*nbCol_` bucket means `block()` on a missing
position returns a real-layout view instead of the 0-stride `DenseMatrixView`
hack, avoiding future surprises where every element of such a view aliases
`blockBucket_[0]`. The extra allocation is accepted. `klubsmatrix.h` documents
"All current users pass true".

---

### 4. LOW (FIXED) — `OsdiFile::experimental` uninitialized at declaration

- [x] **Fixed** — `include/osdifile.h` now declares `bool experimental {false};`.
  It is only assigned on the valid 0.4/0.5 version paths; the default keeps a
  half-constructed `OsdiFile` (early return, `descriptorArray == nullptr`) from
  holding an indeterminate flag.

---

### 5. LOW (FIXED) — Delay contributions loaded after an eval failure

- [x] **Fixed** — `TranNRSolver::buildSystem` now gates its delay block on
  `if (ok && tranDelayLines_ && nDelay>0)`, so an `ok == false` return from
  `OpNRSolver::buildSystem` (eval/load failure) skips the delay stamp, matching
  `OpNRSolver` (which returns before its own delay block) and the adjacent noise
  block (`if (ok && noiseEnabled)`). The iteration is discarded on `!ok` either
  way; the guard just avoids `getSample()` on a stale solution and writes into a
  partially-built Jacobian.

---

### 6. INFO (FIXED) — `CircularBuffer::upsize` signed/unsigned loop counter

- [x] **Fixed** — `include/ansupport.h` loop counter changed from `auto i=0`
  (signed `int`) to `decltype(nToMove) i=0`, so it matches `nToMove`'s
  `DepthIndex` (`uint32_t`). Was a `-Wsign-compare` nit only; logic unchanged
  (`nToMove` is non-negative under the `size_ > 0` guard, loop empty when 0).

---

## What checks out

- NR sign conventions for the delay stamp (`f = -out + delayed(in)`,
  `df/dout = -1`, residual into `delta[outU]`, update `x -= J^-1 f`) are
  consistent across OP / transient / AC / HB / HBAC.
- `addSample` / `getSample` history logic: the buffer-growth "is the
  second-oldest still within `maxDelay`?" test, newest-aligned index
  correspondence between `history_[slot]` and the shared `timepointHistory`
  (preserved across independent per-slot upsizes), binary-search bounds,
  degenerate-interval guards, and the `t=0` seeding in `TranCore::coroutine`
  are all correct.
- AC-family: the one-time `M[i] = Jr[i]` copy of the converged OP Jacobian into
  `acMatrix`'s real part means the delay's `(out,out) = -1` carries over
  correctly while `(out,in)` is overwritten per-frequency with
  `exp(-j w td)`; consistent across `ac` / `acsp` / `acstb` / `acxf` / `noise`.
- HB / HBAC bucket-convention migration (`n*nb` -> `(n+1)*nb`, unknown `u` at
  `u*nb`) is applied consistently in `evaluate` / `buildSystem` / `checkDelta` /
  `postRun` / `loadForces` / `dumpSolution`.
- Member declaration order (delay members before `core`) respects the
  reference-bind-in-init-list constraint in every `an*.h`.
- PSS and HB correctly reject unsupported delay forms up front.
- The `delta[outU] = ...` overwrite (vs `+=`) is safe: OSDI hands the `absdelay`
  z-node equation to the simulator with no device contribution to that row.
