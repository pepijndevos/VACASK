# Bug Audit — `feature/absdelay`

Audit of the 12-commit `absdelay()` feature branch (delay-line support across
OP / DC / transient / AC-family / HB / HBAC, plus OSDI 0.5 descriptor plumbing).
~2000 lines added over `main`.

## Summary

| # | Severity | Area | One-line |
|---|----------|------|----------|
| 1 | HIGH | `osdifile.cpp`, `osdiinstance.cpp` | OSDI 0.4 models: unconditional reads of 0.5-only descriptor fields → OOB / segfault (regression) |
| 2 | MEDIUM | `coretrannr.cpp` | Variable `absdelay()` in transient silently uses a delay frozen at the operating point |
| 3 | LOW | `anhb.cpp` | HB Jacobian bucket size silently changed `false` → `true` |
| 4 | LOW | `osdifile.h` | `OsdiFile::experimental` not initialized at declaration |
| 5 | LOW | `coretrannr.cpp` | Delay contributions loaded even after an eval failure (inconsistent with `OpNRSolver`) |
| 6 | INFO | `ansupport.h` | `CircularBuffer::upsize` signed/unsigned loop counter (pre-existing, benign) |

Items 1 and 2 are genuine defects that affect real inputs. 3–5 are cleanups /
latent traps. 6 is noted for completeness.

---

## Findings

### 1. HIGH — OSDI 0.4 models: out-of-bounds descriptor reads (regression)

- [ ] **Fix**

The branch adds OSDI **0.5** support and keeps 0.4 as a supported "legacy"
input (`include/osdi04legacy.h`, the `experimental` flag, version gate accepts
`0.4` and `0.5`). Two paths read the **new 0.5-only descriptor fields
unconditionally**, i.e. even for a 0.4 descriptor whose in-memory layout ends at
`module_flags`:

- `lib/osdifile.cpp:231-232`, per-descriptor loop:

  ```cpp
  for(decltype(desc->absdelay_count) j=0; j<desc->absdelay_count; j++) {
      if (desc->absdelays[j].maxdelay_offset!=UINT32_MAX) { ... }
  ```

  `desc->absdelay_count` / `desc->absdelays` live past the end of a 0.4
  descriptor slot (`descriptorSize == sizeof(OsdiDescriptor04)`). For a
  multi-descriptor file this reads the *next* descriptor's `name` pointer as a
  `uint32` count, then dereferences a garbage `absdelays` pointer → runaway loop
  + wild deref. (The adjacent `absdelayCount(i)>0` call three lines up *is*
  correctly guarded via the `experimental` accessor — the new loop just doesn't
  use it.)

- `lib/osdiinstance.cpp:1625`, `loadCore()`:

  ```cpp
  if (loadSetup.delayLines_) {
      auto n = descr->absdelay_count;      // unguarded 0.5 field
  ```

  `loadSetup.delayLines_` is non-null for OP and HB solves regardless of whether
  any device uses delay, so this runs for every OSDI instance, including 0.4
  ones.

**Impact:** 80 `*.osdi` files in the working tree are v0.4 (including the
IHP-Open-PDK `psp103`, `r3_cmc`, ... models); only the freshly built `devices/`
are v0.5. Loading any existing 0.4 model on this branch will very likely
segfault or spuriously trip the "variable delay" / illegal-feature path.
Pre-branch, 0.4 files loaded fine.

**Fix:** gate both sites on `experimental`, or route through the existing
`absdelayCount(i)` / `absdelays(i)` accessors (which already return `0` /
`nullptr` for non-experimental descriptors).

---

### 2. MEDIUM — Variable `absdelay()` in transient uses a frozen (DC) delay

- [ ] **Fix**

`TranNRSolver` (`lib/coretrannr.cpp`) stores `tranDelayLines_` but **never sets
`loadSetup_.delayLines_`** — it passes `nullptr` to the `OpNRSolver` base, and
the inherited `loadSetup_` keeps that null. Consequently
`OsdiInstance::loadCore()`'s per-iteration `setDelay()` path
(`lib/osdiinstance.cpp:1623`) never executes during transient stepping:
`delay_[slot]` is written **once**, by the initial operating-point solve, then
held constant for the whole run.

- Constant `td` (e.g. the new `tline_ideal.va`, and every test on the branch):
  fine, the value never changes.
- Non-constant `td` (`absdelay(x, td, maxdelay)` with `td` a signal): the delay
  is frozen at its operating-point value for the entire transient. Wrong
  answer, **no warning** — unlike HB
  (`usesIllegalDeviceFeatures(VariableAbsdelay, ...)`) and PSS
  (`... Absdelay ...`) which explicitly reject.

The `loadCore` variable-delay branch calls `setDelay()` *unconditionally* (not
just on `firstTimepoint`), which only makes sense if it were meant to run each
transient load — so this reads as an oversight, not a deliberate limitation.

**Fix:** point `loadSetup_.delayLines_` at `tranDelayLines_` in `TranNRSolver`,
or reject `VariableAbsdelay` in `TranCore::rebuild()` the way HB does.

---

### 3. LOW — HB Jacobian bucket size silently changed `false` → `true`

- [ ] **Confirm intended / restore `jac(false)`**

`lib/anhb.cpp` dropped the explicit member initializers:

```cpp
jacColoc(true),   // removed
jac(false),       // removed  -> now defaults to true
```

`KluBlockSparseMatrixCore(bool largeBucket=true)`, so `jac` (`bsjac`, the HB
Jacobian) now allocates a full `nbRow_*nbCol_` bucket instead of a scalar. The
`klubsmatrix.h` comment was updated to "All current users pass true", suggesting
this is intended, but it rides along with the delay work with no commit-message
rationale and is a behavior/allocation change unrelated to delays.

---

### 4. LOW — `OsdiFile::experimental` uninitialized at declaration

- [ ] **Fix**

`include/osdifile.h:349` declares `bool experimental;` with no initializer; it
is only assigned on the valid-version branches of the constructor. Harmless
today (the object is unusable on the early-return paths) but a latent trap —
give it a default (`bool experimental{false};`).

---

### 5. LOW — Delay contributions loaded after an eval failure

- [ ] **Fix (or note as intentional)**

`TranNRSolver::buildSystem` loads delay residual/Jacobian contributions even
when `OpNRSolver::buildSystem` returned `ok == false` (eval failure);
`OpNRSolver` returns early before its own delay block. Harmless (the iteration
is discarded) but inconsistent.

---

### 6. INFO — `CircularBuffer::upsize` signed/unsigned loop counter

- [ ] **Optional cleanup**

`include/ansupport.h:57`: `for(auto i=0; i<nToMove; i++)` compares signed `int`
to `uint32_t nToMove`. Pre-existing style, benign (guarded by `size_ > 0` and
`nToMove` is small).

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
