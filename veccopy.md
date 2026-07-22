# PSS vector/matrix copy audit

Audit of `lib/corepss.cpp` / `lib/corepsstran.cpp` (shooting-Newton PSS core
and its inline sensitivity integrator) for `Vector<double>`/`DenseMatrix<double>`
values that are copied out of a function/member when a reference would
suffice. Ranked by impact; matrix copies are O(n²) and dominate.

## FIXED 1. `PhiT` copied every Newton iteration instead of only at convergence

- `lib/corepsstran.cpp:360-370` (`integrateSensitivity`)
- `lib/corepsstran.cpp:377-398` (`integrateAugmentedSensitivity`)

Both do `PhiT = phiCurrent_;`, copying the full n×n matrix into the caller's
out-param. Called once per outer Newton iteration from
`PssCore::runSensitivity` (`lib/corepss.cpp:332-353`), invoked at
`lib/corepss.cpp:501` with `phiT_` (a `PssCore` member) as the target.

`phiT_` only needs a durable copy for the *converged* result (exposed later
via `convergedMonodromy()`, `include/corepss.h:173`). On every non-final
iteration the value is only read locally to build `Jp`
(`lib/corepss.cpp:528-533`).

Fix direction: expose `phiCurrent_` via a `const DenseMatrix<double>&`
accessor on `PssTranCore` (same pattern already used for
`convergedMonodromy()`), read through that reference while building `Jp`
during the loop, and copy into `phiT_` only once, after convergence.

## FIXED 2. Double copy of Phi/Psi on every accepted timestep
- `lib/corepsstran.cpp:271` + `lib/corepsstran.cpp:332-333`
- `lib/corepsstran.cpp:314` + `lib/corepsstran.cpp:343`

`onTimestepAccepted()` runs once per accepted transient step (far more often
than once per Newton iteration):

```cpp
phiCurrent_ = rhs;                          // copy 1 (n×n)
...
DenseMatrix<double> phiSnap(phiCurrent_);    // copy 2 (n×n)
phiHist_.push_front(std::move(phiSnap));
```

Building `phiSnap` directly from `rhs` and moving `rhs` into `phiCurrent_`
turns this into one copy + one move. Same pattern applies to
`psiCurrent_`/`psiRhs`/`psiHist_`.

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
