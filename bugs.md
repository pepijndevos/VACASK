# Bug audit — feature/errorstack branch

Audited: `git diff 1e01cd58c14dead0f0e6ba32776a6b2dab6b4240..HEAD` (branch feature/errorstack vs main),
104 files / ~3.9k changed lines, migrating error reporting from `Status&`/return-codes to an
`ErrorConsumer`/exception-based error-stack.

## Bugs introduced by this branch

- [ ] 1. **AC / Noise / HBAC analyses lose node-name resolution in matrix-error messages** — `lib/coreac.cpp`, `lib/corenoise.cpp`, `lib/corehbac.cpp`
  `setResolver()` is only wired up for `OperatingPointCore::jac` (`lib/coreop.cpp:40`) and `HBCore::bsjac` (`lib/corehb.cpp:550`). `ACCore::acMatrix`, `NoiseCore::acMatrix`, and `HBACCore::jacSpec`/`acMatrix` never get a resolver registered, so `HBACUnknownNameResolver` (`corehbac.cpp:46`) is now dead code. Before the refactor, each site built a resolver at the point of formatting the error and passed it explicitly.
  Failure scenario: run `ac`/`noise`/`hbac` on a circuit whose small-signal matrix goes singular or produces NaN/Inf — the error now reports a raw row/column index instead of the node name.

- [ ] 2. **HB analyses lose detailed non-convergence diagnostics** — `lib/corehbnr.cpp` / `include/corehbnr.h`
  `NRSolver::run()` now reports non-convergence via virtual hook `pushConvergenceReport()` (default no-op). `OpNRSolver` overrides it correctly (`coreopnr.cpp:1151`); `HBNRSolver` still only implements the old `formatConvergence()` (`corehbnr.cpp:784`, worst delta/node/frequency) but was never given a `pushConvergenceReport()` override, so it silently falls back to the no-op base. `formatConvergence()` is now dead code.
  Failure scenario: an `hb`/`hbac` run fails to converge — user sees only "NR solver failed to converge." with no worst-delta/node/frequency breakdown.

- [ ] 3. **(Latent) Convergence report narrowed to only fire when `itlim` is exhausted** — `lib/nrsolver.cpp:428-430`
  Old code emitted the convergence error whenever the loop exited with `!converged`; new code gates it behind `iteration >= itlim`. Currently dead (`OpNRSolver`/`TranNRSolver`/`HBNRSolver` hooks always return `true`), but a future subclass whose hook returns `false` before `itlim` is hit would get zero explanation instead of even the generic message.

- [ ] 4. **`KluMatrixCore::formatError()`/`errorRow()`/`errorRank()`/`errorElement()` are dead and lie** — `lib/klumatrix.cpp`, `include/klumatrix.h`
  All real error state moved to `ec.push(...)`; the old `lastError`/`errorIndex`/`errorRank_`/`errorNan` fields are only ever reset, never set. These public accessors now always report "no error" / 0 / false. No current callers, so inert today, but misleading if reused.

- [ ] 5. **Leftover debug statement** — `simulator/main.cpp:49`
  `auto a = error_registry();` copies the whole error registry into an unused local. Harmless, looks like an accidental commit.

### Adjacent issue touched but not fixed by this branch (not a regression)
- [ ] 6. `lib/coreopnr.cpp:747` — `OpNRSolver::buildSystem` pushes `OpNrLoadForcesError` then has a bare `std::make_tuple(false, evalSetup_.limitingApplied);` with no `return`, falling through to `return std::make_tuple(true, ...)` — caller is told the build succeeded despite the reported failure. Confirmed present in base commit `1e01cd58` already; the analogous `HBNRSolver::buildSystem` in `corehbnr.cpp` has the `return` in both old and new code, so the two functions have diverged. This branch touched the surrounding lines but didn't fix this copy.

## Pre-existing bugs (present before this branch, found during the audit)

- [ ] 7. **`Analysis::finish()` silently discards a "restore circuit state" failure via variable shadowing** — `lib/an.cpp:613-629`
  `bool ok = true;` at the top of `finish()` is shadowed by `auto [ok, hierarchyChanged, needsCoreRebuild] = circuit.elaborateChanges(...)` inside the `preSweepValuesStored` block. On failure, `ok = false` only sets the inner shadowed variable; `return ok;` at function end returns the untouched outer `ok`.
  Failure scenario: a swept analysis completes, then restoring pre-sweep parameter/option values fails (e.g. topology changed during the sweep) — failure text is appended to `s`, but `finish()` still returns `true`; a caller branching on the boolean return treats it as a clean finish while the circuit is left in the last-swept state. Confirmed verbatim in base commit; untouched by this branch.

- [ ] 8. **`DCXFCore::coroutine()` always yields `Finished`, even after a mid-sweep solve/finiteness failure** — `lib/coredcxf.cpp`
  The per-source loop sets `error = true; break;` on failed `jacobian.solve()` or non-finite solution, but `error` is only used for a debug print — the final `co_yield CoreState::Finished;` is unconditional. Sibling coroutines (`ACCore`, `NoiseCore`, `HBACCore`, `DCIncrementalCore`) all gate the terminal yield correctly; `DCXFCore` is the outlier.
  Failure scenario: `dcxf` with 2+ sources where the solve/finiteness check fails on the first — loop breaks, later `tf[]`/`zin[]`/`yin[]` entries stay stale/zero, but the core reports success; a raw-file point is written with truncated/wrong data and the pushed error is never surfaced. Confirmed byte-identical in base commit.

- [x] 9. **`TranNRSolver::advanceNoise()` swallowed a `collectNoiseScaling()` failure** — `lib/coretrannr.cpp` (base commit ~line 155)
  Failure path built `std::make_tuple(false, changed)` but never `return`ed it, falling through to unconditional success. **Already fixed on this branch** (HEAD `coretrannr.cpp:154` has the `return`) — incidental, not a deliberate targeted fix.

- [x] 10. **Dead error-handling block in `resolveSave()`** — `lib/anacsp.cpp`, `lib/anacstb.cpp` (base commit)
  An unconditional `if (verify) return st; else return true;` made a trailing block (the only call to `smsigCore.formatError(s)`) unreachable — any error state in `smsigCore` was never surfaced. Inert, not data-corrupting. **Already removed on this branch** — incidental cleanup.

## Notes / minor items (not counted as bugs)
- `lib/core.cpp` (`AnalysisCore::addRealVarOutputSource`, both overloads): computes `node->unknownIndex()` into a local, then calls `node->unknownIndex()` again instead of reusing it. Redundant but no functional effect (pure accessor). Present in base commit too.
- `lib/coretran.cpp:1198` (`// ???` next to `havePredictor`) and the noise-row-count threshold discussion around `lib/coretran.cpp:747` looked worth a second glance but no concrete evidence of a defect was found.
- `lib/an.cpp`'s reuse of a single `ErrorStack`/`ErrorConsumer` across an entire sweep loop in `Analysis::coroutine()` is a design smell worth double-checking (relies on each core's `coroutine()`/`runSolver()` clearing state at the right point) but not confirmed as a live bug within the audited scope.
