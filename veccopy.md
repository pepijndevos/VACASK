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

Final fix: `rhs` is gone entirely. The RHS accumulation loop and the block
solve now write directly into `phiCurrent_`
(`phiCurrent_.zero()`/`phiCurrent_.column(j)`/
`lastAlr_.solveBlock(phiCurrent_.data().data(), n)`, `lib/corepsstran.cpp:231-271`),
leaving only the one copy that's actually unavoidable —
`phiSnap(phiCurrent_)` into history (`lib/corepsstran.cpp:332`). This is
safe because `phiCurrent_`'s incoming value (Phi from the previous step) is
never read during the build; by the time this call starts it already lives
on independently as `phiHist_[0]`, pushed at the end of the *previous* call.

Two dead ends on the way here, worth keeping as context:
- First attempt made `rhs` a persistent scratch member (to avoid
  reallocating it every call) but kept constructing it fresh in
  `onTimestepAccepted()`, so the member was inert. Once the local shadowing
  declarations were actually removed, PSS broke: the RHS accumulates across
  the `p` loop (`rhs_col[i] += ...`) and used to rely on the *local*
  variable's constructor zero-filling it on every call. A persistent member
  is only resized once per shoot in `clearTrajectory()`, and resizing to an
  unchanged size doesn't clear existing contents — so every step after the
  first accumulated on top of the previous step's stale values. Fixed at
  the time by adding an explicit `rhs.zero()` at the top of the function.
- Second dead end: with `rhs` now a reused scratch member, `phiCurrent_ =
  std::move(rhs)` became actively wrong (not just non-optimal) —
  `DenseMatrix::operator=(DenseMatrix&&)` doesn't really move (see item 5
  below), but *if* that were ever fixed, moving from a buffer that must
  survive to be reused next call would leave it with a freed/dangling
  buffer while its size metadata still claimed n×n. Reverted to a plain
  copy, then eliminated `rhs` altogether as described above.

Same fix applied to the Psi side, which never had these issues since
`psiCurrent_`/`psiHist_` were already correctly structured:
`psiCurrent_ = std::move(psiRhs);` (`lib/corepsstran.cpp:314`, `psiRhs` is a
true local, dead after this point, and `Vector`/`std::vector` move-assignment
isn't buggy) followed by the existing `psiHist_.push_front(psiCurrent_)`
copy (`lib/corepsstran.cpp:343`): one copy + one move.

## FIXED 4. Per-column copy into scratch buffers

Was: `phi_colbuf[i] = phi_col[i]` copied each column of `phiHist_[p]`
(stored `Major::Column`, i.e. stride-1/contiguous) into a temp
`Vector<double>` before calling `scratchC_.product()`, because
`KluMatrixCore::product()` only took raw pointers and `VectorView` didn't
expose one.

Fixed by adding `KluMatrixCore<IndexType, ValueType>::product(VectorView<ValueType>,
VectorView<ValueType>)` and the equivalent `tproduct()` overload
(`include/klumatrix.h:311-320`, `lib/klumatrix.cpp`), which walk the CSC
columns through `VectorView::operator[]` instead of raw pointers, so they
accept a matrix column directly. The call sites in `onTimestepAccepted()`
now pass `Phi_kmi.column(j)` straight to `scratchC_.product(...)`
(`lib/corepsstran.cpp:239,253`) — `phi_colbuf` is gone.

## FIXED 5. `DenseMatrix::operator=(DenseMatrix&&)` didn't actually move

Found while chasing item 2. Was (`include/densematrix.h:743-751`):

```cpp
DenseMatrix<T>& operator=(DenseMatrix<T>&& other) {
    major_ = other.major_;
    data_ = other.data_;              // copy-assigns the vector, not move!
    start_ = std::move(data_.data()); // std::move on a raw pointer is a no-op
    ...
};
```

Identical to the copy-assignment operator apart from a pointless
`std::move()` around a raw pointer. The move *constructor* just above it
(`include/densematrix.h:688-695`) did this correctly
(`data_ = std::move(A.data_);`) — only the assignment operator was affected.
Net effect: every `someDenseMatrix = std::move(otherDenseMatrix);` in the
codebase silently did a full O(n²) copy instead of a move.

Fixed to `data_ = std::move(other.data_); start_ = data_.data();`
(`include/densematrix.h:743-751`). Checked before fixing: no other call site
in the codebase does `= std::move(...)` on a `DenseMatrix`, so nothing was
relying on the old copy-like behavior for correctness — safe to fix in
isolation. The moved-from source (e.g. a reused scratch member) is left
with an emptied `data_` but stale `nRow_`/`nCol_`/`start_`, standard
"valid but unspecified" moved-from state — do not read a `DenseMatrix`
after moving from it without reassigning/resizing it first (this is exactly
the hazard item 2's second dead end ran into).

## WON'T FIX 3. `integrateAdjointMonodromy`: copies where a move would do

- `lib/corepsstran.cpp:373` (`omegaHist.push_front(Omega)`)
- `lib/corepsstran.cpp:483` (`Omega = omegaHist.front();`)

`Omega` is seeded into history by copy even though it isn't read again until
overwritten at the end, and the final result is copied out of a deque
(`omegaHist`) that is local scratch destroyed right after. Both are
`std::move` candidates rather than reference-return material, and now that
item 5 is fixed `std::move` here would actually save the copies (both
`Omega` and `omegaHist` entries are locals that aren't reused afterward).

Deliberately not doing this: the adjoint monodromy computation
(`integrateAdjointMonodromy` and the whole `params.adjoint` path) is going
to be removed in the future, so it's not worth spending effort on.

## FIXED 6. `DenseMatrix::row(i) const` / `column(i) const` silently strip constness

Found during a follow-up sweep of `densematrix.h` for the same class of
"stupid" slip as item 5. Was (`include/densematrix.h:801-810` and `:823-832`):

```cpp
VectorView<T> row(size_t i) const {
    auto* p = const_cast<T*>(data_.data());   // strips const off data_
    ...
    return VectorView<T>(p+nCol_*i, nCol_, 1);   // returns a WRITABLE view
};
```

Both were declared `const` but returned a plain (non-const) `VectorView<T>`,
using `const_cast` to get a writable pointer out of `data_.data()`. So
`.row(i)`/`.column(i)` on a `const DenseMatrix<double>&` handed back a view
that could still be written through — defeated the const contract. Compare
to the base class `DenseMatrixView::row()/column() const`
(`include/densematrix.h:288-293`), which returns `const VectorView<T>` and
needs no cast at all, because it reads the inherited `start_` pointer
directly (a plain `T*` value, unaffected by the method's own constness).

Fixed by using `start_` (already kept in sync with `data_.data()` by every
constructor/`resize()`/assignment) instead of re-deriving from
`data_.data()`, dropping the `const_cast` and returning `const VectorView<T>`
to match the base class:

```cpp
const VectorView<T> row(size_t i) const {
    switch (major_) {
        case Major::Row:    return VectorView<T>(start_+nCol_*i, nCol_, 1);
        case Major::Column:
        default:            return VectorView<T>(start_+i, nCol_, nRow_);
    }
};
```

An in-place attempt using `auto* p = data_.data();` (no cast) doesn't
compile: `data_.data() const` returns `const T*`, which can't implicitly
convert to the `T*` the `VectorView<T>` constructor needs — `start_` sidesteps
this because its declared member type is already plain `T*`. Checked every
`.row(`/`.column(` call site in `lib/`/`include/`/`devices/` before fixing —
none chain a mutating call (`.swap()`, `=`, `.scale()`, …) directly onto the
result of a const matrix's `.row()`/`.column()`, so nothing relied on the old
leak; all safe.

## FIXED 7. Dead variable in `solveCore`

Was (`include/densematrix.h:594`): `auto pivot = pivCol[pivI];`, computed
during pivot search and never read afterward — the pivot value actually
used later comes from `pivRow[i]` post-swap. Harmless, just noise. Removed.

## Priority

Items 1, 2, 4, 5, 6, and 7 are fixed. Item 3 is deliberately left as-is
(adjoint monodromy code is slated for removal). Nothing else outstanding
in this file.
