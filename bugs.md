# Bug audit — `feature/solvers` (`main...HEAD`)

KLU→CSC matrix/solver-abstraction refactor, 8 commits. Each finding verified against the tree.

## Confirmed bugs

- [x] **1. Uninitialized `solver_` in `NRSolver`** — `include/nrsolver.h:249`
  New member `RealSparseSolver* solver_` is not in the constructor init list
  (`lib/nrsolver.cpp:45-46`). Any use before `setLinearSolver()` reads an
  indeterminate pointer. Fix: `solver_(nullptr)` in the ctor (or default member
  initializer).

- [x] **2. Uninitialized `cxSolver_` in the AC-family cores** — `include/coreac.h:163`
  `ComplexSparseSolver* cxSolver_` has no default initializer and is not set in
  the constructors of `ACCore` (`lib/coreac.cpp:44-47`), `ACSPCore`, `ACStbCore`,
  `ACXFCore`, `NoiseCore`. `HBACCore` was already fixed (`cxSolver_(nullptr)` in
  `lib/corehbac.cpp`); apply the same fix to the other five.

- [x] **3. Stale duplicate header `include/cirdelay.h`** — new file this branch, 250 lines
  (deleted in working tree; commit the removal)
  Redefines `class DelayLines` and shares the same include guard
  `__CIRDELAY_DEFINED` as the real `include/coredelay.h`. Referenced by nothing
  (no source, no CMakeLists). The two copies have already diverged (commit
  `ca669000` edited both in parallel). Incomplete `git mv` — delete it.

- [x] **4. Unused local `options` in `ACCore::rebuild()`** — `lib/coreac.cpp:154`
  (removed in working tree; commit it)
  `auto& options = circuit.simulatorOptions().core();` is never read in
  `rebuild()` (the `options` used for `rcondcheck` is a separate local in
  `coroutine()` at line 197). Breaks `-Werror` builds. Leftover copy-paste — no
  sibling core's `rebuild()` has this line.

## Consistency issue

- [~] **5. Hardcoded solver in PSS adjoint monodromy** — `lib/corepsstran.cpp:414`
  `KluRealSparseSolver scratchASolver(scratchA);` bypasses the abstraction. The
  sibling site at `corepsstran.cpp:54` correctly does
  `RealSparseSolver::createSolver(solverId, ...)` honoring
  `params.opParams.solver` → `options.tdsolver` → `defaultTdSolverId`. A user
  selecting `opsolver="superlu"` silently gets KLU for the adjoint tsolves.
  WON'T FIX — old code, slated for removal.

## Lower confidence / cleanup

- [x] **6. `Simulator::setup()` no longer idempotent** — `lib/simulator.cpp:96`
  `registerSolver<...>()` returns `unordered_map::insert(...).second`, folded
  into `ok`. On a second `setup()` call the registry static still holds the
  entries, insert returns false, and `setup()` returns failure though nothing is
  wrong. `registerAnalysis()` deliberately returns unconditional `true` for this
  reason.
  FIXED — `setup()` always refreshes the paths, but registration runs once
  (`setupDone_`/`setupOk_` guard); later calls return the first call's result.

- [x] **7. Dead protected methods in `solklu.h`** — `structuralRank()`,
  `numericalRank()`, `singularColumn()`, `kluRgrowth()` carried over from the old
  `KluMatrixCore` public API. The `LinearSparseSolver` interface exposes none of
  them and nothing derives from `KluLinearSparseSolver`, so they are unreachable
  (and drag in the `klu_*_rgrowth` symbols).
  FIXED — all four removed; stale `rgrowth()` mention in the `rcond()` comment
  corrected.
