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

- [ ] **2. `storeState()` lost its cheap "skip name rebuild" fast path — homotopy loops now pay full `O(n)` name-rebuild cost on every iteration** *(coreop/corehb, moderate — performance regression)*

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

- [ ] **3. Stored-solution period lookup for `ic=` skips the `typeTag()` check present at the other two lookup sites in the same file** *(corepss, minor — latent)*

  `lib/corepss.cpp:272` (`PssCore::determineInitialPeriod`, ordinary/
  non-continue mode). `circuit.storedSolution(solutionName)` returns any
  solution regardless of producer; lines 185 and 215 in the same file
  correctly gate on `typeTag()==OperatingPointCore::solutionTag` before
  trusting the payload, but line 272-275 reads `solPtr->auxReal()`
  unguarded.

  Currently latent: only `lib/corepss.cpp` itself ever calls `setAuxReal()`
  with a meaningful nonzero value, and PSS's own stored solutions already
  share `OperatingPointCore::solutionTag`, so a mismatched-type lookup today
  just yields the default `auxReal_=0.0`, caught by the `period<=0` check at
  line 280. But it's the one lookup path in this feature that omits the
  type-tag guard the rest of the PR applies consistently, and will silently
  misinterpret data the moment any other producer starts writing a nonzero
  `auxReal_` under a tag PSS doesn't check for.

- [ ] **4. Off-by-one in the PSS non-convergence diagnostic** *(corepss, cosmetic)*

  `lib/corepss.cpp:465` / `lib/corepss.cpp:693-696`. The shooting loop
  `while (iterIndex <= options.pss_itl)` (iterIndex starting at 0) always
  attempts `pss_itl+1` iterations before giving up (e.g. `pss_itl=0` still
  performs one full shoot). On non-convergence, `formatError()`'s
  `NoConvergence` case prints
  `"PSS failed to converge in " + std::to_string(options.pss_itl) + " iterations."`
  — the raw option value, not the actual attempt count — understating the
  true number of attempts by exactly one in every case.

- [ ] **5. `PssTranCore::rebuild()` rebuilds `scratchC_` twice in a row** *(corepsstran, minor — copy-paste)*

  `lib/corepsstran.cpp:49-56`. Two back-to-back, byte-identical
  `scratchC_.rebuild(circuit.sparsityMap(), n)` calls with the same error
  message. `include/corepsstran.h` declares only two rebuildable matrix
  members (`lastAlr_`, `scratchC_`), both accounted for elsewhere, so this
  is harmless/idempotent rather than a missing rebuild — but it silently
  redoes a KLU symbolic factorization pass on every `PssTranCore::rebuild()`
  call (triggered whenever the circuit topology changes) for no benefit,
  and the duplicated identical error message is a clear signal one of the
  two calls was meant to target a different matrix that never got added.

- [ ] **6. `integrateAdjointMonodromy()`'s backward integration loop allocates a fresh O(n²) matrix every step instead of hoisting it out of the loop** *(corepsstran, efficiency)*

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

- [ ] **7. Shooting-Newton convergence check re-derives the SPICE delta-tolerance formula inline instead of reusing existing `checkDelta()` helpers** *(corepss, reuse)*

  `lib/corepss.cpp:573-574`:
  `tol = options.pss_tolscale * max(|x0[i]|*reltol, unknown_abstol[i])`
  duplicates the same formula already centralized in
  `OpNRSolver::checkDelta()` (`lib/coreopnr.cpp`) and
  `HBNRSolver::checkDelta()` (`lib/corehbnr.cpp`), plus an inline copy in
  `lib/coretran.cpp` — making this the newest of (at least) five
  near-identical hand-rolled copies of the delta-tolerance policy. If the
  tolerance formula is ever fixed or extended (e.g. a `vntol` special case)
  in one copy, the other four — including this new PSS one — silently keep
  the stale behavior since there's no single place to change it.

- [ ] **8. `prepareStabilisation()` and `runShoot()` duplicate the same `maxacfreq`-based step-clamping logic verbatim** *(corepss, reuse)*

  `lib/corepss.cpp:154-173` and `lib/corepss.cpp:305-316` both compute
  `effMaxacfreq = max(maxacfreq, 40/period); hmax = min(maxstep, 1/(2*effMaxacfreq))`
  plus the surrounding stop/step/start setup, differing only in which
  `TranParameters` struct (`stabilParams` vs `shootParams`) and period
  variable (`period` vs `T0`) they write into. A maintainer tuning the
  step-size policy (e.g. changing the `40.0` constant) has no
  compiler-visible link between the two copies and can easily update one
  while leaving the other stale, causing stabilisation and shooting to
  silently use inconsistent step-size policies.
