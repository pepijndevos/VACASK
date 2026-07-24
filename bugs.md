# PSS branch bug audit

Bug audit of `analysis/pss-rebase-new` restricted to the diff introduced since
it forked from `main` (merge-base `ae9533635e26187bb57435189c09499014de59dd`).
Covers the new PSS shooting-Newton core (`corepss`/`corepsstran`/`anpss`),
its call-outs into shared infrastructure (`coretrancoef`, `klumatrix`,
`densematrix`), and the modified pre-existing analysis cores
(`coreop`/`corehb`) touched by the `AnnotatedSolution` refactor. Findings are
numbered consecutively, ranked most severe first. Check a box once fixed.

- [ ] **1. `computePsiT()` adds a spurious un-differentiated `bScaled` term (plus a wrongly `h`-scaled `bSens` term) to `d(qdot_N)/dh`, corrupting `Psi_T` for LMS methods with nonzero `b` coefficients (e.g. trapezoidal/AM2, the common default)** *(corepsstran, severe)*

  `lib/corepsstran.cpp:540-546`. The `qdot_N` reconstruction used elsewhere
  in this same diff (`IntegratorCoeffs::differentiate()`,
  `include/coretrancoef.h:122-131`) is
  `qdot_N = leading_*q_N + sum aScaled_[i]*q_hist[i] + sum bScaled_[i]*qdot_hist[i]`,
  where `bScaled_ = -b_i/b1_` carries **no** separate explicit `h_{N-1}`
  factor — confirmed both by reading `scaleDifferentiator()`
  (`lib/coretrancoef.cpp:522-540`) and by the file's own finite-difference
  self-test `checkDiffSens()` (`lib/coretrancoef.cpp:723-752`), which
  verifies `bScaledSens_ == d(bScaled_)/dh_{N-1}` exactly. Differentiating
  that formula at fixed `x_N` (the partial `computePsiT()` documents
  computing) should therefore be exactly
  `leadingSens_*q_N + sum aScaledSens_[i]*q_hist[i] + sum bScaledSens_[i]*qdot_hist[i]`
  — no extra `bScaled` term, no extra `h_{N-1}` factor.

  But `lib/corepsstran.cpp:540-541` adds `bScaled[p]*qDotHist_.at(p+1)[i+1]`
  on top of the correctly-differentiated `lastStepH_*bSens[p]*qDotHist_...`
  term at lines 543-544. For trapezoidal (AM order 2),
  `computeSensitivities()` takes the constant-coefficient shortcut
  (`aSens_=bSens_=b1Sens_=0`), so `bScaledSens_` is correctly `0` — trapezoidal's
  `b` coefficients genuinely don't depend on `h` — but `bScaled_` itself is
  nonzero (~`-1`), so the spurious term injects a wrong nonzero contribution
  into the RHS solved for `Psi_T` (`lib/corepsstran.cpp:551`).

  Impact: `Psi_T` feeds directly into the augmented Newton Jacobian's period
  column for autonomous PSS (`corepss.cpp` `Jp` block), so any
  `.pss driven=0` run using the default trapezoidal integrator (e.g.
  `test/test_pssosc1.sim` through `test_pssosc5.sim`) computes a corrupted
  Newton step and likely converges to a wrong period/solution or fails to
  converge.

  The doc section this was transcribed from (`docs/theory/pss.md`,
  "Computing Psi_T") uses a *different* normalization convention where an
  explicit `h_{N-1}` is pulled out in front of the raw `b_i` sum, which is
  why its formula legitimately has both an un-differentiated `b_i` term and
  a `h_{N-1}*b_i'` term — that convention was copied verbatim into code that
  uses the codebase's own `bScaled_`/`bScaledSens_`, which already have that
  `h` factor accounted for differently, producing the mismatch.

- [x] **2. `storeState()` lost its cheap "skip name rebuild" fast path — homotopy loops now pay full `O(n)` name-rebuild cost on every iteration** *(coreop/corehb, moderate — performance regression, fixed)*

  `lib/coreop.cpp:130` (`OperatingPointCore::storeState`) and
  `lib/corehb.cpp:183` (`HBCore::storeState`). The old signature
  `storeState(size_t ndx, bool storeDetails)` skipped `setNames(circuit)`
  (an `O(unknownCount)` loop rebuilding `circuit.reprNode(i)->name()` for
  every node, `lib/ansolution.cpp:12-27`) via a cheap `clearNames()` call
  when `storeDetails=false`. The new `storeState(size_t ndx)` always calls
  `setNames(circuit)`.

  `lib/hmtpgmin.cpp:90` and `lib/hmtpsrc.cpp:97,128` call `storeState()` on
  every gmin-stepping/source-stepping homotopy iteration and previously
  passed `storeDetails=false` since only values/states are needed
  mid-homotopy. That fast path is now gone, so circuits needing many
  homotopy iterations to converge redundantly rebuild the full name vector
  every iteration — a silent performance regression on large circuits with
  hard convergence, not caught by any test since it doesn't change output
  correctness.

  Fixed by restoring the `bool storeDetails=true` parameter on
  `storeState()` (`include/coreop.h`, `include/corehb.h`,
  `lib/coreop.cpp`, `lib/corehb.cpp`) and `clearNames()` on
  `AnnotatedSolution` (`include/ansolution.h`), and passing
  `storeDetails=false` back at the three homotopy call sites
  (`lib/hmtpgmin.cpp:90`, `lib/hmtpsrc.cpp:97,128`).

- [x] **3. Stored-solution period lookup for `ic=` skips the `typeTag()` check present at the other two lookup sites in the same file** *(corepss, minor — latent, fixed)*

  `lib/corepss.cpp:280-283` (`PssCore::runStabilisation`, ordinary/
  non-continue mode, `ic=` string lookup). `circuit.storedSolution(solutionName)`
  returns any solution regardless of producer; lines 193 and 223 in the same
  function correctly gate on `typeTag()==OperatingPointCore::solutionTag`
  before trusting the payload, but lines 280-283 read `solPtr->auxReal()`
  unguarded.

  Currently latent: only `lib/corepss.cpp` itself ever calls `setAuxReal()`
  with a meaningful nonzero value, and PSS's own stored solutions already
  share `OperatingPointCore::solutionTag`, so a mismatched-type lookup today
  just yields the default `auxReal_=0.0`, caught by the `period<=0` check at
  line 288. But it's the one lookup path in this feature that omits the
  type-tag guard the rest of the PR applies consistently, and will silently
  misinterpret data the moment any other producer starts writing a nonzero
  `auxReal_` under a tag PSS doesn't check for.

  Fixed by adding the same `typeTag()==OperatingPointCore::solutionTag`
  guard to the `ic=` lookup (`lib/corepss.cpp:281`).

- [x] **4. Off-by-one in the PSS non-convergence diagnostic** *(corepss, cosmetic — fixed)*

  `lib/corepss.cpp:465` / `lib/corepss.cpp:693-696`. The shooting loop
  `while (iterIndex <= options.pss_itl)` (iterIndex starting at 0) always
  attempted `pss_itl+1` iterations before giving up (e.g. `pss_itl=0` still
  performed one full shoot), even though `pss_itl` is documented
  (`docs/cmd-options-pss.md`) and commented (`lib/options.cpp:204`) as the
  *maximum number* of outer Newton iterations.

  Fixed by changing the loop bound to `while (iterIndex < options.pss_itl)`
  (`lib/corepss.cpp:465`), so `pss_itl` now means what it says: at most
  `pss_itl` shooting attempts. `formatError()`'s `NoConvergence` message
  (`lib/corepss.cpp:693-696`), which already printed `options.pss_itl`
  unmodified, is now correct as-is. Updated the "Newton loop outline"
  comment in `include/corepss.h` to match (`l = 0, 1, ..., pss_itl-1`).

- [x] **5. `PssTranCore::rebuild()` rebuilds `scratchC_` twice in a row** *(corepsstran, minor — fixed)*

  `lib/corepsstran.cpp:49-56`. Two back-to-back, byte-identical
  `scratchC_.rebuild(circuit.sparsityMap(), n)` calls with the same error
  message. `include/corepsstran.h` declares only two rebuildable matrix
  members (`lastAlr_`, `scratchC_`), both accounted for elsewhere, so this
  was harmless/idempotent rather than a missing rebuild — but it silently
  redid a KLU symbolic factorization pass on every `PssTranCore::rebuild()`
  call (triggered whenever the circuit topology changes) for no benefit.

  Fixed by deleting the second, redundant `scratchC_.rebuild(...)` block
  (`lib/corepsstran.cpp:49-57`).

- [x] **6. `integrateAdjointMonodromy()`'s backward integration loop allocates a fresh O(n²) matrix every step instead of hoisting it out of the loop** *(corepsstran, efficiency — moot, function is slated for removal/refactor)*

  `lib/corepsstran.cpp:425-427` (inside `for (Int k = nSteps - 1; k >= 0; k--)`
  at line 405): a brand-new `DenseMatrix<double> rhs(n, n, ...)`,
  `Vector<double> om_col(n)`, `Vector<double> rhs_col(n)` are constructed
  every iteration — `nSteps` reallocations of an `n×n` matrix per call to
  `integrateAdjointMonodromy()`, which itself runs once per PSS shooting
  Newton iteration. Additionally, around line 479,
  `DenseMatrix<double> omegaSnap(rhs); omegaHist.push_front(std::move(omegaSnap));`
  makes an unnecessary full `O(n²)` copy of `rhs` immediately before the
  move, when `rhs` is not used again that iteration and could be moved
  directly (`omegaHist.push_front(std::move(rhs))`). For circuits with many
  unknowns and many timesteps per period, this turns what could be one
  allocation into `O(nSteps)` allocations plus `O(nSteps)` extra `O(n²)`
  copies per Newton iteration.

- [x] **7. Shooting-Newton convergence check re-derives the SPICE delta-tolerance formula inline** *(corepss, minor — not a bug, reuse suggestion was invalid, see correction)*

  `lib/corepss.cpp:573-574`:
  `tol = options.pss_tolscale * max(|x0[i]|*reltol, unknown_abstol[i])`.

  **Correction:** originally flagged as "should reuse `OpNRSolver::checkDelta()`/
  `HBNRSolver::checkDelta()`," but that's not actually available to reuse.
  `checkDelta()` is a pure virtual method of the `NRSolver` base class
  (`include/nrsolver.h:127`), implemented by `OpNRSolver`/`HBNRSolver`, and
  it operates on the full stateful inner-loop NR machinery (`VectorRepository`
  solution/delta, `NRSettings`, per-iteration bookkeeping like
  `pointMaxSolution_`, debug norm computation). `PssCore`
  (`include/corepss.h:112`) is a plain `AnalysisCore`, not an `NRSolver` —
  its outer shooting-Newton loop solves a bespoke `(n+1)`-sized augmented
  system (`[Δx0, ΔT]` via `Fp`/`Jp`/`alpha`) with no correspondence to
  `NRSolver`'s per-node contract. Calling `checkDelta()` would require
  restructuring PSS's outer loop into an `NRSolver` subclass — a mismatched
  abstraction, not a simple reuse. The `lib/coretran.cpp:1523` reference was
  also wrong: that's part of a larger LTE-reference-mode dispatch
  (`relreflte`/`relref` branching), not a plain duplicate of this formula.
  At most, the one-line tolerance expression itself could be pulled into a
  small shared free function — low value, not the "five duplicated copies"
  originally claimed.

- [x] **8. `prepareStabilisation()` and `runShoot()` duplicate the same `maxacfreq`-based step-clamping logic verbatim** *(corepss, reuse — fixed)*

  `lib/corepss.cpp:154-173` and `lib/corepss.cpp:305-316` both computed
  `effMaxacfreq = max(maxacfreq, 40/period); hmax = min(maxstep, 1/(2*effMaxacfreq))`,
  differing only in which `TranParameters` struct (`stabilParams` vs
  `shootParams`) and period variable (`period` vs `T0`) they wrote into. A
  maintainer tuning the step-size policy (e.g. changing the `40.0` constant)
  had no compiler-visible link between the two copies and could easily
  update one while leaving the other stale.

  Fixed by extracting the clamp into
  `PssCore::clampStepToMaxacfreq(TranParameters& tp, double period) const`
  (declared in `include/corepss.h`, implemented in `lib/corepss.cpp`), now
  called from both `prepareStabilisation()` (with `params.stabilParams`,
  `period`) and `runShoot()` (with `params.shootParams`, `T0`). The
  `stabstep`-override branch in `prepareStabilisation()`, which was never
  actually duplicated, was left untouched.
