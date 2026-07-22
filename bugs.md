# PSS branch bug audit

Bug audit of `analysis/pss-rebase-new` restricted to the diff introduced since
it forked from `main` (merge-base `ae9533635e26187bb57435189c09499014de59dd`).
Covers three areas: (1) the core PSS shooting-Newton algorithm
(`corepss`/`corepsstran`/`anpss`) against the agreed formulation in `pss.md`,
(2) the `densematrix`/`klumatrix`/`nrsolver` layer, (3) integration points in
the rest of the codebase (`coretran`, `corehb`/`corehbac`, `options`,
`circuit`, solution store/restore). Findings below are numbered consecutively
across all areas, ranked most severe first. Check a box once fixed.

- [ ] **1. `psiCurrent_` (Psi_T) is scaled by the whole period `T0_` instead of the local last-step size** *(Area 1: corepss/corepsstran, severe)*

  `lib/corepsstran.cpp:300-306` (the Psi update in `onTimestepAccepted`)
  computes the forcing term as `(alpha/T0_) * a[p] * qHistData_[p][...]` and
  `(alpha/T0_) * qSnap[...]`. Per `pss.md`'s "Computing Psi_T" section, the
  correct BDF-case coefficient is `1/h_{N-1}` — the *last step's own size* —
  not `1/T`. `T0_` is the full shooting period (set once per shoot in
  `clearTrajectory`), typically much larger than any single step (there are
  `pss_minpts` or more steps per period), so this divides by a quantity
  orders of magnitude too large.

  The class carries the *correct* scale as documented but unused fields:
  `include/corepsstran.h:220-224` states "alpha and b1... give 1/h =
  alpha*b1, which is the correct velocity scale for PsiT", and `lastAlpha_`/
  `lastB1_` are assigned every step (`lib/corepsstran.cpp:223-224`) but are
  **never read anywhere in the repo** (confirmed by grep) — the fix
  described in the class's own comment was never wired into the actual
  formula.

  Impact: for autonomous circuits (`params.driven=0`), `Jp.at(i,n) =
  -tmpPsiT[i]` (`lib/corepss.cpp:525`) feeds this wrongly-scaled Psi
  directly into the augmented Jacobian's ΔT column, corrupting the coupled
  `[Δx0,ΔT]` solve (the phase-vector use of Psi in `computePhaseConstraint`
  is immune since it normalizes to unit length, but the raw Jacobian column
  is not normalized). Expect slow/non-quadratic convergence or outright
  failure to converge on `T` for any autonomous PSS run.

- [ ] **2. Phase-condition residual is hardwired to 0 instead of `alpha^T·x0` — the autonomous Newton system's bottom equation is never actually enforced** *(Area 1: corepss, severe)*

  `lib/corepss.cpp:521-522`:
  ```cpp
  Fp[n] = 0;
  //for (decltype(n) i = 0; i <= n; i++) Fp[n] += alpha[i] * x0[i];
  ```
  Per `pss.md`'s boxed augmented system, the bottom RHS entry must be
  `-alpha^T x0^{(l)}` so that solving drives the *new* `x0` to satisfy
  `alpha^T x0 = 0`. With `Fp[n]` fixed at 0, the bottom equation reduces to
  `alpha^T·Δ = 0` where the code updates `x0 -= Δ`; algebraically this
  forces `alpha^T·x0_new = alpha^T·x0_old` — the phase offset present at the
  first iterate (from the stabilization transient, generally nonzero and
  arbitrary) is preserved unchanged by construction rather than driven to
  zero. The phase condition is not actually being solved, only the
  *direction* of the correction is constrained. The adjacent `/// TODO:
  rewrite computePhaseConstraint without weird approximations` comment
  (`lib/corepss.cpp:509`) shows this area is known-unfinished, but this
  specific defect (correct RHS computed in a comment, then discarded) is a
  concrete, fixable bug distinct from that TODO.

- [x] **3. `AnnotatedSolution::auxReal_` is never initialized by the default constructor — violates its own "≤0 means unset" contract** *(Area 3: ansolution/core, severe — fixed)*

  `include/ansolution.h:88` declares `double auxReal_;` with no
  default-member-initializer, and `lib/ansolution.cpp:9-10`
  (`AnnotatedSolution::AnnotatedSolution() {}`) left it untouched. The
  class comment explicitly states "aux real scalar (period), `<=0` means no
  period given," and `lib/corepss.cpp` relies on exactly that sentinel
  (`period = continueState->solution.auxReal(); if (period<=0) {
  setError(...) }`).

  Fixed by initializing `auxReal_(0)` in the constructor's member
  initializer list (`lib/ansolution.cpp:9`), so every
  default-constructed `AnnotatedSolution` — including `CoreStateStorage`
  slots created via `AnalysisCore::allocateStateStorage()`'s
  `vector::resize`, which bypasses `Circuit::newStoredSolution()`/`clear()`
  — now starts with the sentinel value instead of indeterminate garbage.

  The only place that reliably zeroes it is `Circuit::newStoredSolution()`
  (`lib/circuit.cpp`), via `.clear()`. But `CoreStateStorage::solution`
  objects (one per slot in `AnalysisCore::coreStates`, `include/core.h:31-50`)
  are default-constructed directly by
  `AnalysisCore::allocateStateStorage()` (`lib/core.cpp:14-25`, a
  `std::vector::resize`), bypassing `newStoredSolution()`/`clear()` entirely.
  `OperatingPointCore::storeState()` and `HBCore::storeState()` never call
  `setAuxReal()`. So for any core-state slot whose owning core never sets
  `auxReal_`, `auxReal()` returns indeterminate garbage instead of a safe
  default.

  Concrete failure: a `valid`/`coherent` state-storage slot whose owning
  core never calls `setAuxReal()` (e.g. an OP or HB slot, or a PSS slot
  before its own store path runs) can yield garbage from `auxReal()`. If
  that garbage happens to be `>0`, `corepss.cpp` (twice, ~lines 195 and 224)
  will treat an uninitialized period as real, seeding the
  shooting/stabilization loop with a bogus period instead of falling back
  to `params.tper` or failing cleanly. No longer possible now that the
  constructor initializes the field.

- [ ] **4. Aliasing hazard in new `product()`/`tproduct()` VectorView overloads** *(Area 2: klumatrix, medium, latent — undecided: a real runtime overlap check is non-trivial and takes time; may or may not do)*

  `lib/klumatrix.cpp:185,230` — both new view-based `KluMatrixCore::product()`/
  `tproduct()` carry a `// TODO: check for VectorView overlap` and a doc
  comment saying the views "must not overlap," but there is no runtime
  check. If ever called with `vec` and `res` aliasing the same buffer (e.g.
  `product(M.column(j), M.column(j))`), the result-zeroing loop runs over
  the entire destination first, silently zeroing `vec` before it is read —
  producing a silently wrong (near-zero) result rather than a crash. Not
  proven to occur today (call sites live in `corepsstran.cpp`), but worth
  enforcing given it's an explicitly-flagged precondition with no guard.

- [x] **5. `TranCore::rebuild()` hardcodes IC-force writes to slot 2** *(Area 3: coretran — not a bug, working as designed)*

  Originally flagged because `lib/coretran.cpp`'s `rebuild()` writes the
  base `ic=` forces into `opCore_.solver().forces(2)` (hardcoded), while
  `coroutine()`'s icmode=op setup reads from the configurable
  `icForcesSlot`, with no coupling enforcing the two match.

  Not a bug: slot 2 is `TranCore`'s own base `ic=` handling. `icForcesSlot`
  exists precisely so a derived core (e.g. PSS's `stabilTran_`, which
  redirects it to slot 3) can populate a *different* slot itself and point
  `coroutine()` there instead of slot 2 — that's the intended override
  mechanism, not a leftover coupling gap. A subclass supplying both a
  non-default `icForcesSlot` and a real base `params.ic` at the same time
  would be caller error (asking for two different IC sources at once), not
  a framework bug.

- [x] **6. `AnalysisCore::coreState(size_t)` returns a reference into a resizable vector with no invalidation contract** *(Area 3: core, minor, latent — fixed)*

  `include/core.h:142`: `CoreStateStorage& coreState(size_t ndx) { return
  coreStates.at(ndx); };`. `coreStates` is `std::vector<CoreStateStorage>`,
  and `allocateStateStorage()`/`deallocateStateStorage()` (`lib/core.cpp`)
  `resize()` it, invalidating existing references. `lib/corepss.cpp:181`
  held a local reference from `opCore_.coreState(0)`; it wasn't used across
  a subsequent resize, so nothing was broken, but the API gave no warning
  that a later allocate/deallocate call invalidates any reference obtained
  this way — a use-after-free trap for future
  callers.

  Fixed by removing the unused `opSlot` local (`auto& opSlot =
  opCore_.coreState(0);`, dead — never read afterward) from
  `PssCore::runStabilisation()` in `lib/corepss.cpp`. That was the only
  call site in the codebase holding a `coreState()` reference, so the
  concrete hazard is gone; `coreState()`'s signature in `core.h` is
  unchanged since no current caller is at risk.

- [x] **7. `tsolve`/`tsolveBlock` omit accounting instrumentation present in `solve`/`solveBlock`** *(Area 2: klumatrix, minor — fixed)*

  `lib/klumatrix.cpp:539-608` (`solve`/`solveBlock`) wrap the KLU call with
  `Accounting::wclk()`/`acct->acctNew.solve`/`tsolve` timing; the newly
  added `tsolve`/`tsolveBlock` (same file, ~610-660) did not increment any
  `acct` counters. Not a correctness bug — perf/diagnostic stats for
  transpose solves would simply undercount — but it's an inconsistency
  introduced by this branch.

  Fixed by adding the same `Accounting::wclk()`/count-increment/timing
  blocks to `tsolve`/`tsolveBlock` as `solve`/`solveBlock` already have,
  incrementing `acct->acctNew.solve`/`cxsolve` (count) and
  `acct->acctNew.tsolve`/`tcxsolve` (time) — `AcctData` (`include/acct.h`)
  has no separate transpose-solve field, so transpose solves are folded
  into the same aggregate counters as regular solves, consistent with how
  the rest of the accounting struct treats solve variants.

- [x] **8. Wrong matrix name in `GTProductFailed` error message** *(Area 1: corepsstran, cosmetic — fixed)*

  `lib/corepsstran.cpp:580-583` — the `GTProductFailed` case in
  `formatError()` printed `"PssTranCore: C^T product failed..."`,
  copy-pasted from the `CTProductFailed` case above it; should have said
  `G^T`. Misled debugging only, no functional impact. Already corrected in
  the working tree (uncommitted) to `"PssTranCore: G^T product failed..."`.

## Verified correct (no findings)

**Area 1 (corepss/corepsstran):** `Phi_0=I`/`Psi_0=0` reset and history
ramp-up in `clearTrajectory`/`onTimestepAccepted` given the "first step is
always order-1" invariant; the monodromy sign/coefficient derivation
(`gammaC[p]=alpha*a[p]` against the raw `abar_i` from `IntegratorCoeffs`) is
self-consistent with pss.md's boxed `M_k` once the sign-convention
difference is accounted for; `lastAlr_` is refactored fresh each step
before being used for both Phi and Psi solves (no stale-factorization
reuse); the `params.adjoint` guard in `convergedAdjointMonodromy()`
(`lib/corepss.cpp:673-679`) is correct; all of `veccopy.md`'s FIXED items
(1,2,4,5,6,7) are correctly applied in current `lib/corepsstran.cpp`/
`include/densematrix.h`.

**Area 2 (densematrix/klumatrix/nrsolver):** `veccopy.md` items 5
(`DenseMatrix::operator=(DenseMatrix&&)`), 6 (const-correctness of
`row(i)`/`column(i)`), and 7 (dead `pivot` variable) are all correctly
applied — `git show 6cfa1c5f` is a clean, complete fix for exactly these
three issues. The new `product()`/`tproduct()` overloads correctly index
through `VectorView::operator[]` (stride-safe) and `tproduct` correctly
accumulates the CSC-transpose pattern. KLU call signatures for
`solveBlock`/`tsolve`/`tsolveBlock` (`include/klumatrix.h:291-320`) match
`suitesparse/klu.h` exactly. `nrsolver.h`'s diff is two inert lines, no
correctness-relevant change. (Pre-existing, out of this branch's diff, not
a finding here: `DenseMatrix::addRow()` resizes `data_` without refreshing
`start_`, only reachable via `spurs.cpp`, not the PSS path.)

**Area 3 (coretran/corehb/corehbac/options/circuit):** Option registration
for the new `pss_*`/`icmode`/`continuePrevious` settings; `tstop=0`
early-exit placement (after the t=0 point is written, before any
divide-by-`tstop` arithmetic); `onTimestepAccepted`'s call site (strictly
inside the `accept` branch, before history advance — can't fire on a step
that's later rejected); store-key consolidation and `typeTag()` checks at
every solution-repository read site; member reordering for `opSolution`
(declaration order matches the fragile-bind-fix convention).
