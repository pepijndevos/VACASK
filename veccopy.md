# PSS vector/matrix copy audit

Audit of `lib/corepss.cpp` / `lib/corepsstran.cpp` (shooting-Newton PSS core
and its inline sensitivity integrator) for `Vector<double>`/`DenseMatrix<double>`
values that are copied out of a function/member when a reference would
suffice. Ranked by impact; matrix copies are O(n²) and dominate.

## FIXED 1. `PhiT` copied every Newton iteration instead of only at convergence

Was: `integrateSensitivity()`/`integrateAugmentedSensitivity()` did
`PhiT = phiCurrent_;`, copying the full n×n matrix into a `PssCore`-owned
out-param (`phiT_`) once per outer Newton iteration, even though only the
converged result needed to survive.

Now: `PssTranCore` exposes `phiCurrent()`/`psiCurrent()` reference accessors
(`include/corepsstran.h`) and a `phiValid()` check
(`lib/corepsstran.cpp`). The old `integrateSensitivity`/
`integrateAugmentedSensitivity`/`PssCore::runSensitivity` wrapper is gone.
`PssCore::coroutine()` reads `PhiT`/`PsiT` through `pssTran_.phiCurrent()`/
`pssTran_.psiCurrent()` directly while building `Jp` each iteration
(`lib/corepss.cpp:472-524`), and copies into the durable member `phiT_`
exactly once, right where the shooting Newton loop converges
(`lib/corepss.cpp:589`) — before the final adjoint re-shoot that would
otherwise overwrite `pssTran_`'s live `phiCurrent_`.

Bonus cleanups that came with this:
- The unused `x_laststep` parameter of the old `integrateAugmentedSensitivity`
  (computed but never read) was removed along with the function.
- `PssCore::PsiT` (a 1-indexed, size n+1 member holding a reindexed copy of
  `psiCurrent_` with a leading zero bucket) was eliminated entirely.
  Every consumer (`computePhaseConstraint`, debug prints, the `Jp` right
  column) now reads `pssTran_.psiCurrent()` (0-indexed, size n) directly,
  with the index shift (`PsiT[i+1]` ↔ `tmpPsiT[i]`) applied inline instead
  of via a stored copy.

Regression caught and fixed during review: the removed `runSensitivity()`
wrapper used to call `setError(PssError::SensitivityFailed)` on failure.
The replacement `if (!pssTran_.phiValid())` check initially dropped that
call, so `PssCore::lastPssError` stayed `OK` and `formatError()` silently
reported no error after an aborted run. Restored at `lib/corepss.cpp:473`.

## FIXED 2. Double copy of Phi/Psi on every accepted timestep

`onTimestepAccepted()` runs once per accepted transient step (far more often
than once per Newton iteration). Was:

```cpp
phiCurrent_ = rhs;                          // copy 1 (n×n)
...
DenseMatrix<double> phiSnap(phiCurrent_);    // copy 2 (n×n)
phiHist_.push_front(std::move(phiSnap));
```

Now: `phiCurrent_ = std::move(rhs);` (`lib/corepsstran.cpp:271`) — `rhs` is
dead after this point, so the move is safe — followed by the existing
`phiSnap(phiCurrent_)` copy into history (`lib/corepsstran.cpp:332`): one
copy + one move instead of two copies. Same fix applied to the Psi side:
`psiCurrent_ = std::move(psiRhs);` (`lib/corepsstran.cpp:314`) followed by
the existing `psiHist_.push_front(psiCurrent_)` copy
(`lib/corepsstran.cpp:343`).

## 3. `integrateAdjointMonodromy`: copies where a move would do

- `lib/corepsstran.cpp:418` (`omegaHist.push_front(Omega)`)
- `lib/corepsstran.cpp:528` (`Omega = omegaHist.front();`)

`Omega` is seeded into history by copy even though it isn't read again until
overwritten at the end, and the final result is copied out of a deque
(`omegaHist`) that is local scratch destroyed right after. Both are
`std::move` candidates rather than reference-return material, but they're
free n×n copies to remove.

## 4. (minor, more invasive) Per-column copy into scratch buffers

- `lib/corepsstran.cpp:233-236` and similar sites

`phi_colbuf[i] = phi_col[i]` copies each column of `phiHist_[p]` (stored
`Major::Column`, i.e. stride-1/contiguous) into a temp `Vector<double>`
before calling `scratchC_.product()`. `VectorView` doesn't currently expose
a raw pointer, so eliminating this needs a small API extension. Lower
priority than 1-3.

## Priority

Item 1 is the one most worth fixing: it's a direct "copy where a reference
would do" case, and it scales with both n² and the outer Newton iteration
count (`pss_itl`).
