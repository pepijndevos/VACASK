# feature/blas audit — full findings dump

Unfiltered transcription of every finding from all three review passes:
Run 1 = correctness-focused pass over the full `main...feature/blas` diff.
Run 2 = cleanup/altitude/conventions pass over the unpushed local delta
(`include/common.h`, `include/densematrix.h`, `lib/spurs.cpp`).
Run 3 = correctness-focused pass (angles A/B/C/D) over the same unpushed
delta. Overlaps between runs are left in and cross-referenced rather than
merged away.

## Run 1 — correctness pass, full branch diff vs main

1. [x] **FALSE POSITIVE** — `include/densematrix.h:336` — claimed
   `vectorPlusScaledVector()`'s generic (non-`double`/`Complex`) fallback
   dereferences the `VectorView<T>` `v0` itself instead of the element
   pointer `ptrV0`. Verified against current code: line 336 is actually
   `*ptr = *ptrV0 + *ptrOther * factor;` — it already uses `ptrV0`
   correctly. Reviewing agent misread the code; no bug here.
2. [x] **FIXED & VERIFIED** — `include/densematrix.h:34` — `subVector()`'s
   bounds check originally rejected valid strided sub-views (`stride>1`) due
   to using `offset+n*stride` instead of `offset+(n-1)*stride`. After two
   intermediate attempts that introduced their own bugs (an `&&`-grouping
   error that disabled the check entirely, then an uneven `stride_` scaling
   that missed real out-of-bounds cases when `offset!=0`), the check now
   reads `(n>0) && ((offset+(n-1)*stride)*stride_>=n_*stride_)`. Verified
   against all known edge cases: the original over-restrictive case, the
   boundary-index case, the `offset!=0`/`stride_>1` false-negative case, and
   `n==0` (short-circuits before `n-1` underflow). All correct.
3. [x] **FIXED (untested — no macOS available here)** — `CMakeLists.txt:313`
   — the Darwin build path never linked BLAS/LAPACK: `find_package(BLAS/
   LAPACK)` only ran `if (${SIM_PLATFORM} STREQUAL "Linux")`, and the Darwin
   `SIM_EXE_LIBRARIES` list had no BLAS/LAPACK/Accelerate entry. Since
   `include/blaslapack.h`'s `extern "C"` routines are called from
   essentially every `VectorView<double>`/`VectorView<Complex>` operation,
   this was a total link failure on macOS. Fixed by extending the
   `find_package(BLAS/LAPACK REQUIRED)` guard to also cover Darwin, and
   adding `${LAPACK_LIBRARIES} ${BLAS_LIBRARIES}` to Darwin's
   `SIM_EXE_LIBRARIES`, mirroring the Linux branch. Caveat: unverified on
   actual hardware; CMake's `FindBLAS`/`FindLAPACK` may resolve to Apple's
   Accelerate framework rather than Homebrew's BLAS/LAPACK depending on
   `CMAKE_PREFIX_PATH` — pin `BLA_VENDOR` if a specific provider is
   required.
4. [x] **NOT A BUG (by design)** — `include/densematrix.h:52` (and
   pervasively throughout the file) — numerous previously-unconditional
   `throw std::out_of_range(...)` invariant checks (vector/matrix length
   and shape mismatches) were converted to `DBGCHECK`, a Release-mode
   no-op. Author confirmed this is intentional: these are per-call
   invariant checks on the hottest path in the codebase (every
   `VectorView`/`DenseMatrix` op), and DBGCHECK exists precisely to avoid
   that overhead in Release. The "usage errors should throw" convention is
   aimed at API-boundary misuse, not internal hot-loop invariant checks
   called orders of magnitude more often.
5. [x] **FIXED** — `include/densematrix.h:541-566` — `identity()` uses
   `rowStride_==1` as a proxy for "column-major", but a **row-major** matrix
   with `nCol_==1` also has `rowStride_==1` (since `rowStride_=nCol_=1`). It
   gets misclassified as column-major and calls `dlaset_`/`zlaset_` with
   `lda=colStride_=1` while `m=nRow_` can be `>1`, violating LAPACK's
   `lda>=max(1,m)` contract. In practice this was always harmless for
   reference LAPACK specifically (not just "coincidentally today"): since
   `n=1` in the degenerate branch, `dlaset_`'s column loop never advances
   past column 0, so `lda` is provably never used to compute an address —
   the exposure was only to a stricter/validating BLAS backend that checks
   `lda>=max(1,m)` before doing any work. Fixed by clamping
   `lda = std::max(lda, m)` after the branch selection, which is a no-op in
   every non-degenerate case (a valid view can never naturally have
   `colStride_ < nRow_` or `rowStride_ < nCol_`) and only raises `lda` in
   the degenerate case, keeping the call contract-legal unconditionally.
   No current caller hits the degenerate case (`corepsstran.cpp`,
   `corehb.cpp` call sites are all square, `nCol_>1`). (Independently found
   again as A1 in Run 3.)

## Run 2 — cleanup/altitude/conventions pass, unpushed delta

6. [x] **INTENTIONAL, VERIFIED BUG-FREE** — `include/densematrix.h:474-476`
   — new `ptrOf()` accessors (const/non-const) have zero callers anywhere
   in the repo, but author confirms this is deliberate (added ahead of a
   future call site). Verified: `ptrOf(row,col)` returns
   `start_ + row*rowStride_ + col*colStride_`, identical offset arithmetic
   to `at(row,col)` (line 471-472), just pointer instead of reference —
   consistent with `row()`/`column()`'s existing pattern of building
   `VectorView`s from a raw pointer + stride. Const/non-const overloads
   match; no bounds-check asymmetry with `at()`. Keeping. (Same file/lines
   independently noted as C6 in Run 3.)
7. [x] **REMOVED** — `include/common.h:102-103` — new `CHECK(cond, msg)`
   macro had zero call sites, duplicating `DBGCHECK`'s exact body
   unconditionally with nothing in the tree using it. Deleted.
8. [x] **FIXED** — `include/densematrix.h:492-493` (in `operator=(const
   DenseMatrixView&)`) vs. `include/densematrix.h:514-515` (in
   `operator=(const T&)`) — `packedRowMajor`/`packedColMajor` stride-check
   logic was copy-pasted between the two `operator=` overloads, one variant
   additionally checking `other.colStride_`/`other.rowStride_`, the other
   not. Fixed by factoring the per-view predicate into two private helpers,
   `isPackedRowMajor()`/`isPackedColMajor()` (next to `luSolveCore` in the
   private section), each defined once. The two overloads still compose
   them differently on purpose: `operator=(const DenseMatrixView&)` needs
   `(isPackedRowMajor() && other.isPackedRowMajor()) ||
   (isPackedColMajor() && other.isPackedColMajor())` — same-major on both
   sides, not just independently packed — while `operator=(const T&)` only
   has `this` to check. A single merged `isPacked()` predicate would have
   let a row-major/column-major mismatch slip through the `dcopy_` fast
   path and scramble the result, so that composition wasn't collapsed
   further. (Same duplication independently noted as D1 in Run 3 — see
   item 36.)
9. [x] **INTENTIONAL, VERIFIED BUG-FREE** — `include/densematrix.h:1127-1147`
   — `addRow()`'s column-major reshape branch is not exercised by any
   caller in the repository, but author confirms it's deliberate (added
   ahead of a future call site). Traced through concretely (2×3 → 3×3
   column-major growth): the copy loop's `j*(nRow_+1)` destination stride
   uses the pre-increment `nRow_` (correctly equal to the post-increment
   value), the new row's slots land exactly on the value-initialized
   (zero) tail of each column block, `setStride()`'s post-increment
   `colStride_=nRow_` matches the layout actually built, and the returned
   `row(nRow_-1)` view addresses the new row correctly across all columns.
   No bug. Keeping. (Independently found again as
   B4/C4 in Run 3.)
10. [x] **FIXED** — `include/densematrix.h:512-533` (`operator=(const T&)`)
    — the fast-path condition (`packedRowMajor`/`packedColMajor`, i.e. the
    *entire* `nRow_*nCol_` region must be one contiguous run) was strictly
    narrower than what LAPACK's `dlaset_`/`zlaset_` actually supports and
    narrower than what `identity()` uses for the *same* underlying routine.
    Confirmed the strided-block case is real, not hypothetical:
    `docs/internals/klubsmatrix.md:33` documents `column(i)`-style block
    views with `row stride = 1, column stride = blockColumnStride[col]`,
    generally `> nRow_`. Fixed by relaxing the condition to
    `rowStride_==1 || colStride_==1` and computing the real `m`/`n`/`lda`
    (mirroring `identity()`'s dispatch, including its `lda=std::max(lda,m)`
    clamp), dropping the old `m=1,n=nRow_*nCol_` flattened form entirely —
    the 2D form is correct for both the packed case (where it reduces to
    the same result) and genuine strided sub-block views, which now get one
    `dlaset_`/`zlaset_` call instead of `nRow_` separate row-fills.
11. [x] **FIXED** — `include/densematrix.h:486-509` (`operator=(const
    DenseMatrixView&)`) — same narrowing as #10: the packed-only check
    meant a copy between two same-shaped strided sub-block views got no
    BLAS fast path at all, falling back to `nRow_` row-copies. `dcopy_`
    itself can't fix this (single stride per side, no way to express two
    different leading dimensions), so added `dlacpy_`/`zlacpy_` prototypes
    to `blaslapack.h` (the 2D copy counterpart of `dlaset_`, same
    no-XERBLA auxiliary-routine family) and relaxed the condition to
    `rowStride_==1`/`colStride_==1` (dropped `isPackedRowMajor()`/
    `isPackedColMajor()` — now unused, removed). Unlike `laset()`, this is
    *not* transpose-symmetric: a positional copy needs `this` and `other`
    to share the same native orientation (`nativeColMajor`/
    `nativeRowMajor`, both requiring `this` and `other` to match, not
    independently packed), so a `this`-row-major/`other`-column-major
    mismatch still correctly falls through to the per-row loop (verified
    against the degenerate `nRow_==1`/`nCol_==1` ambiguous-orientation
    case too — physically coincides either way, so no mismatch risk).
12. [x] **MITIGATED (comment only, by design)** — `include/densematrix.h:
    1020-1175` (`DenseMatrix<T>`) — dropping `DenseMatrix`'s own `at()`/
    `row()`/`column()` in favor of the inherited `DenseMatrixView` versions
    means every accessor now depends on `start_` staying in sync with
    `data_.data()`, and `data()` hands out a mutable `std::vector<T>&`
    with no resync path if a caller resizes/reallocates through it.
    Checked every `.data()` call site in `lib/`/`simulator/`/`include/`
    for a chained size-mutating call or a stored `auto&`/`std::vector<T>&`
    binding — found none; every current caller either copies by value
    (`auto`, not `auto&`) or immediately chains `.data()` again for a raw
    `T*`. Confirmed latent, not live. A structural fix (wrapping
    `std::vector` to intercept resizes, or dropping the mutable overload)
    would be real engineering effort against a risk with zero current
    triggers, and the mutable overload is genuinely needed by in-place
    value-writing callers — so fixed by documenting the invariant on
    `data()` instead of restructuring it.
13. [x] **FIXED (partially — see scope note)** — `include/densematrix.h:
    486-566` — the BLAS/LAPACK fast-path dispatch for `operator=(const T&)`/
    `identity()` was two separate hand-rolled `if constexpr` +
    stride-arithmetic blocks. Fixed by factoring both into a shared private
    helper, `laset(const T& alpha, const T& beta)` (next to
    `isPackedRowMajor`/`isPackedColMajor`), which computes `m`/`n`/`lda`
    once, applies the `lda=std::max(lda,m)` clamp once, and returns
    `false` if `T` isn't `double`/`Complex` or the view isn't
    stride-1-in-one-direction so the caller can fall back to a loop.
    `operator=(const T&)` calls `laset(val, val)` (uniform fill);
    `identity()` calls `laset(T(0), T(1))`. Scope note: this does *not*
    unify with `operator=(const DenseMatrixView&)`'s `dcopy_`/`zcopy_`
    dispatch (that's `dcopy_`/`zcopy_`, a different routine with a
    same-major-on-both-sides constraint — see item 11) or with
    `factor()`/`luSolve()`/`factorAndLuSolve()` (`dgetrf_`/`dgetrs_`,
    still separately duplicated — see item 41/D6 below, left open).
    (Same theme independently noted as D6 in Run 3.)
14. [x] **RESOLVED — see item 7.** `include/common.h:102-103` — author
    confirmed `CHECK` wasn't meant to be adopted here; removed rather than
    wired up.
15. [x] **FIXED** — `docs/internals/densematrix.md:44` — statement that
    `addRow()` "throws `std::out_of_range` for a column-major matrix" was
    factually wrong (the diff's `addRow()` performs a reshape-copy for
    column-major instead). Updated to describe both current paths:
    cheap tail-resize for row-major, full reshape-copy (with the new row
    zero-initialized) for column-major.
16. [x] **DEFERRED** — `include/densematrix.h` new inline comments
    explaining LAPACK column-major/`lda`/packing conventions restate
    rationale that belongs in `docs/internals/densematrix.md`/
    `blaslapack.md`, which don't yet cover the new `operator=`/
    `identity()`/`laset()`/`dlacpy_`/`ptrOf()`/`addRow()` additions.
    No code fix needed — purely a docs-placement item. Author will fold
    this into a wholesale docs update pass at the end of the branch.
17. [x] **NOT APPLICABLE, VERIFIED** — `lib/spurs.cpp:463` and
    `lib/spurs.cpp:555` — `.fill(x)` → `= x`, explicit `VectorView<Int>(...)`
    call-site updates. Original finding hedged that `mixingStencil_ =
    noJacIndex` at line 463 goes through the packed-only fast path in
    `operator=(const T&)`, and would be "affected if the matrix were ever
    a sub-view instead." Doubly moot: (1) `mixingStencil_` is
    `DenseMatrix<Int>` ([spurs.h:170](include/spurs.h#L170)) — `T=Int`
    is excluded entirely by `laset()`'s `if constexpr(is_same<T,double>||
    is_same<T,Complex>)` guard, so this call site never takes the BLAS
    fast path regardless of packing/view-ness; (2) it's declared as an
    owning `DenseMatrix<Int>`, never a view, so it can't structurally be a
    sub-view anyway.

## Run 3 — correctness pass, angles A/B/C/D, unpushed delta

### Angle A — line-by-line diff scan

18. [x] **A1 — FIXED, duplicate of item 5.** `include/densematrix.h:541-566`
    (`identity()`) — same bug as Run 1 item 5: `rowStride_==1` misclassifies
    a row-major `nCol_==1` matrix as column-major, calling `dlaset_`/
    `zlaset_` with an `lda` that violates LAPACK's `lda>=max(1,m)` contract.
    See item 5 for the fix (`lda = std::max(lda, m)`) and the corrected
    analysis of actual exposure (harmless for reference LAPACK since `lda`
    is unused when `n==1`; real risk was only a stricter/validating BLAS
    backend).
19. [x] **A2 — RESOLVED, see item 7.** `include/common.h:103` — macro
    removed entirely rather than hardened.
20. [x] **FIXED** — **A3.** `include/densematrix.h` (`using
    DenseMatrixView<T>::operator=;`) combined with `DenseMatrix`'s own
    copy-assign and move-assign created a 4-candidate overload set. Root
    cause more precisely: the blanket `using` was only needed to bring in
    `operator=(const T&)` (the fill overload, otherwise hidden by name),
    but it also dragged in `operator=(const DenseMatrixView<T>&)` as an
    unwanted side effect. Confirmed via grep that no current call site
    assigns a `DenseMatrixView<T>` directly into a `DenseMatrix<T>`
    lvalue (all `.block()` results use structured bindings, not
    assignment), so removing that exposure is safe. Fixed by dropping the
    blanket `using` and adding an explicit forwarding `operator=(const
    T&)` next to the copy/move assignments instead — `DenseMatrix<T>` now
    has exactly 3 operator= overloads (copy, move, fill), and the base's
    view-assign is reachable only via an explicit `DenseMatrixView<T>&`
    cast, matching the pattern `lib/corehbxform.cpp:113` already requires
    (verified unaffected by this change). Traced the live call sites
    (`spurWeights_ = std::move(...)`, `mixingStencil_ = noJacIndex`,
    `corehbxform.cpp`'s cast) — all still resolve correctly.
21. [x] **A4 — superseded, see items 8/10/11.** `include/densematrix.h:
    486-509` and `512-533` — the original `packedRowMajor`/
    `packedColMajor` guards this finding examined no longer exist (both
    methods were rewritten). Its "correctly falls back when majors differ"
    observation still holds under the new `nativeColMajor`/`nativeRowMajor`
    check in item 11's fix, and its duplication concern was independently
    resolved by items 8 and 13.
22. [x] **DOCUMENTED, VERIFIED NOT RELIED UPON** — `include/densematrix.h`
    (`addRow()`, column-major branch) — relies on `std::vector<T>
    newData((nRow_+1)*nCol_)` value-initializing the new row's slots to
    zero. Checked all 3 `addRow()` call sites in the repo (all in
    `lib/spurs.cpp:124,259,291`): every one unconditionally overwrites
    every element of the new row before any read, so nothing depends on
    the zero-init today. Added a doc comment on `addRow()` stating the new
    row's contents are unspecified/not guaranteed zero (an implementation
    detail, not an API contract), so a future caller doesn't assume
    otherwise and a future refactor (e.g. `reserve`+`push_back`) doesn't
    silently change observable behavior.
23. [x] **FIXED** — **A6.** `include/densematrix.h` — `identity()`'s
    LAPACK dispatch (three nested ternaries picking `m`/`n`/`lda`) had no
    unit test exercising a non-square or padded view. Added coverage in
    `DenseMatrix<T>::test()` (`lib/densematrix.cpp`): identity on 2×3 and
    3×2 row-major matrices, plus padded sub-block views (`lda≠m`, built
    via a manually-constructed `DenseMatrixView` over a sentinel-filled
    larger buffer) for *both* dispatch branches — native column-major
    (`rowStride_==1`) and the row-major "read as transpose" branch
    (`colStride_==1`) — checking both the diagonal/off-diagonal pattern
    inside the block and that the padding outside it is left untouched.

### Angle B — removed-behavior auditor

24. [x] **B1 — no bug, verified, still accurate.** Old
    `DenseMatrix<T>::fill(T)` (removed) — only call site was
    `lib/spurs.cpp:463`, already updated by the diff to `mixingStencil_ =
    noJacIndex;`, now going through item 20's new explicit
    `operator=(const T&)` forwarder. No orphaned callers.
25. [x] **B2 — no bug, verified, still accurate.** Old
    `DenseMatrix::at()/row()/column()` overrides (removed) — byte-for-byte
    equivalent to the inherited `DenseMatrixView` versions in all four
    cases (`Major::Row`/`Major::Column`). Unaffected by any subsequent fix
    in this pass (only `operator=`/`identity()`/`addRow()`/`data()` were
    touched).
26. [x] **B3 — no bug, verified, still accurate.** `start_` resync —
    traced every constructor, copy/move assignment, `resize()`, and
    `addRow()` path; `start_ = data_.data()` is issued after every
    mutation of `data_` in each path, no post-construction window with a
    stale `start_`. Re-confirmed unaffected by subsequent fixes (none
    touched these paths). The invariant this relies on is already
    mitigated at item 12 (`data()`'s doc comment).
27. [x] **B4 — duplicate of item 9, intentional & verified bug-free.** See
    item 9 for the concrete trace confirming the column-major reshape
    logic is correct. (Same finding as Run 2 item 9 / C4 below.)
28. [x] **B5 — no bug, verified, still accurate.** Old
    `DenseMatrix::zero()` (`data_.assign(data_.size(), T())`, removed) vs.
    new inherited `DenseMatrixView::zero()` (`*this = 0`) — confirmed
    equivalent for the only instantiated `T`s, since a freshly-owned
    `DenseMatrix` is always fully packed. Re-checked against item 10's
    later relaxation of `operator=(const T&)` to the broader `laset()`
    dispatch — for a fully-packed matrix that reduces to the same
    `m=nRow_, lda=colStride_=nRow_` computation as before, so the
    equivalence still holds.
29. [x] **B6 — CORRECTED, then fixed.** `lib/corehbxform.cpp:113` (was:
    `static_cast<DenseMatrixView<double>&>(coeffs) = IAPFT;`). Original
    finding claimed the cast prevents `coeffs` from being silently flipped
    row-major, on the assumption `IAPFT` is row-major — **that premise was
    wrong**, never verified against the code. Traced it: `buildAPFT()`
    calls `buildTransformMatrix(IAPFT, s)`, which does `XF.resize(n, ncoef,
    Major::Column)` on its by-reference parameter — `IAPFT` is
    column-major too, matching `coeffs`. So `coeffs = IAPFT` would not
    actually flip anything; the real (much narrower) reason the cast
    existed was to avoid `DenseMatrix`'s own copy-assign doing a full
    `std::vector` reassignment instead of an in-place element copy — and
    even that turned out to be a non-difference, since `std::vector::
    operator=` doesn't reallocate when the destination already has
    sufficient capacity (true here, both are pre-sized `ncoef×ncoef`).
    Along the way, tried replacing the cast with `DenseMatrixView(coeffs)
    = IAPFT;` (CTAD) — confirmed by isolated compile test that this is a
    **most-vexing-parse compile error**: `coeffs` is already declared in
    the same scope, so `DenseMatrixView(coeffs)` parses as a conflicting
    *declaration*, not a temporary-construction expression. Landed on a
    proper fix instead: added `DenseMatrix<T>::view()` (returns a
    `DenseMatrixView<T>` over the same data), so the call site is now
    `coeffs.view() = IAPFT;` — unambiguous, no cast, no CTAD footgun.

### Angle C — cross-file tracer

30. [x] **C1 — by design, confirmed.** `lib/spurs.cpp:463`
    `mixingStencil_ = noJacIndex;` — `mixingStencil_` is `DenseMatrix<Int>`,
    so `operator=(const T&)`'s `if constexpr` fast path is skipped
    entirely (BLAS/LAPACK have no integer routines). Author confirms this
    is deliberate: no BLAS for int arrays, full stop — not something to
    work around. Same duplicate call site as item 17.
31. [ ] **C2.** `lib/corepsstran.cpp:118,378` (`phiHist_.at(i).identity()`,
    `Omega.identity()`) and `lib/corehb.cpp:953` (`I.identity()`) — all
    square matrices with `nCol_==nRow_>1` in all three cases, so A1/#18's
    edge case does not currently affect any live caller.
32. [ ] **C3.** `lib/corehbxform.cpp:113` — the `operator=(const
    DenseMatrixView&)` fast path added by this diff is correctly skipped
    here (mismatched majors between `coeffs` and `IAPFT`), so this call
    site gets no speedup but also no regression.
33. [x] **C4 — duplicate of item 9, intentional & verified bug-free.**
    (Same finding as B4/#27 and Run 2 item 9.)
34. [ ] **C5.** `lib/spurs.cpp:266` — `VectorView(const_cast<Int*>(
    w.data()), w.size(), 1)` changed to `VectorView<Int>(...)` (explicit
    template argument added). Outside the densematrix.h/identity/addRow/
    operator= scope, but a real behavior-relevant edit: `Int` (`int32_t`)
    is the same underlying type as `smsigFreqMap`'s key type
    `VectorView<int>` on this platform, so very likely just a
    CTAD-ambiguity/compile fix — could not fully rule out that this line
    previously failed to compile or deduced a different type before the
    fix. Worth confirming with a clean rebuild.
35. [x] **C6 — duplicate of item 6, intentional & verified bug-free.**
    `include/densematrix.h:474-476` (`ptrOf()`) — not called from any file
    yet. Author confirmed deliberate (added ahead of a future call site);
    see item 6 for the offset-arithmetic verification. (Same finding as Run
    2 item 6.)

### Angle D — reuse/duplication

36. [x] **D1 — FIXED, duplicate of item 8.** `include/densematrix.h:486-533`
    — see item 8 for the fix (`isPackedRowMajor()`/`isPackedColMajor()`
    helpers). Not shared with `identity()`'s dispatch as this finding
    suggested — `identity()`'s condition is a genuinely different, broader
    check (single-stride, not full-pack); unifying them is the separate,
    still-open item 13/41 (D6).
37. [ ] **D2.** `include/densematrix.h:512-533` (`operator=(const T&)`
    fast path) — reimplements, verbatim, the same `dlaset_`/`zlaset_`
    call that `VectorView<T>::operator=(const T&)` (lines 76-95) already
    performs (`m=1, n=length, lda=stride`). Could instead construct a
    flattened `VectorView<T>(start_, nRow_*nCol_, 1)` and delegate,
    avoiding duplicated BLAS dispatch logic. (Same as Run 2 item 10's
    duplication half.)
38. [ ] **D3.** `include/densematrix.h:486-509` (`operator=(const
    DenseMatrixView&)` fast path) — similarly reimplements `dcopy_`/
    `zcopy_` dispatch that already exists verbatim in
    `VectorView<T>::operator=(const VectorView<T>&)` (lines 51-73); could
    delegate to a flattened `VectorView` pair instead. (Same as Run 2
    item 11's duplication half.)
39. [x] **D4 — RESOLVED, see item 7.** `include/common.h:97-103` — moot,
    `CHECK` removed.
40. [ ] **D5.** `include/densematrix.h:1120-1140` (`addRow()`,
    column-major branch) — reshapes the buffer with a raw pointer loop
    (`data_.data()+j*nRow_`, `std::copy`) rather than using the class's
    own `column()`/`VectorView` abstractions used everywhere else in the
    file. Minor consistency/reuse nit, not a bug.
41. [ ] **D6 — partially addressed, factor/luSolve piece still open.**
    `include/densematrix.h:541-566` (`identity()`) vs. `factor()`/
    `luSolve()`/`factorAndLuSolve()` (×2) — all five methods independently
    re-derive "is this matrix LAPACK-column-major-compatible"
    (`rowStride_==1`) and `lda=colStride_`. Item 13's fix unified
    `identity()` with `operator=(const T&)` via the new `laset()` helper
    (a different pairing than this finding proposed, since `laset()` is
    specifically the `dlaset_`/`zlaset_`-alpha/beta-fill dispatch), so
    `identity()` itself no longer duplicates that logic standalone. Still
    open: `factor()`/`luSolve()`/`factorAndLuSolve()` (×2) use a different
    routine (`dgetrf_`/`dgetrs_`, no alpha/beta fill) and still each
    independently re-derive their own contiguity/`m`/`n`/`lda` — a
    `{compatible, m, n, lda}`-returning helper for *that* family remains
    unfactored. (Same theme as Run 2 item 13.)

## Post-audit: discovered while running the self-test

42. [x] **FIXED — CRITICAL.** `include/blaslapack.h`/`include/densematrix.h`
    — `zdotc_`'s prototype assumed the f2c/g77-era "DOUBLE COMPLEX result
    returned via a hidden pointer first argument" gfortran calling
    convention (`void zdotc_(Complex* result, ...)`), and `VectorView<T>::
    dot()`'s `Complex` branch called it that way. This is silently wrong on
    this system: confirmed via an isolated standalone test (bypassing
    DenseMatrix entirely) that the installed reference BLAS
    (`/usr/lib/x86_64-linux-gnu/blas/libblas.so.3`, Debian's default
    `update-alternatives` target — not OpenBLAS, not a custom build) never
    writes through that hidden pointer at all; a sentinel value placed
    there before the call comes back untouched. Declaring `zdotc_` as
    returning `NAMESPACE::Complex` **by value** instead (matching x86-64
    SysV's native register-pair return for a 16-byte all-SSE-class
    aggregate) gives the correct result. Real-world impact: every
    `Complex`-typed `dot()` call silently returned `(0,0)` instead of the
    actual conjugated dot product — this cascades into `multiply()`
    (matrix-vector and matrix-matrix) for `Complex`, which is used
    throughout AC analysis, harmonic balance, and noise analysis. Caught
    by running `DenseMatrix<T>::test()` (both `double` and `Complex`
    instantiations) in an isolated standalone harness (`densematrix.h`/
    `densematrix.cpp` compiled directly with g++, linked against the
    system's actual BLAS/LAPACK, since the project's own build wasn't
    invoked) — `double` passed throughout, `Complex` failed every test
    involving `multiply()` with the result matrix coming back exactly
    all-zero (not a precision issue). Fixed in both `blaslapack.h` (new
    prototype + corrected header comment) and `densematrix.h::dot()` (call
    site updated to use the return value directly). Re-ran the same
    standalone harness after the fix: both `double` and `Complex`
    self-tests now pass with zero failures. Caveat: this is a
    build-environment-dependent ABI question — the fix is verified correct
    for this system's reference BLAS; if the project is ever linked
    against a BLAS built with an older gfortran/f2c-style COMPLEX-return
    convention, this would need revisiting (worth keeping the `dot()`
    result in mind if `Complex` numerical results ever look suspiciously
    zero again on a different platform).

## Post-audit 2: full review of everything changed since the last commit

Scope: `git diff` across `CMakeLists.txt`, `include/blaslapack.h`,
`include/common.h`, `include/corepss.h`, `include/densematrix.h`,
`lib/corehb.cpp`, `lib/corehbnr.cpp`, `lib/corehbxform.cpp`,
`lib/corepss.cpp`, `lib/densematrix.cpp` — i.e. every change made across
this whole session, re-reviewed fresh in one pass rather than relying on
per-change verification alone. Re-checked aliasing/self-assignment,
empty-matrix (`nRow_==0`/`nCol_==0`) construction paths, the
`addScaledRows`/`addScaledColumns` naming consistency, and doc staleness
beyond what item 16 already covers — no new issues found there. One real
finding:

43. [x] **FIXED** — `include/densematrix.h:508-509,869,930` — `laset()`, `operator=
    (const DenseMatrixView&)` (`dlacpy_`), and `maxAbs()` (`dlange_`/
    `zlange_`) all clamp with `lda = std::max(lda, m)` (and `ldb =
    std::max(ldb, m)` in the `operator=` case). This is insufficient to
    satisfy LAPACK's actual contract, `lda>=max(1,m)`: when `m==0`, the
    clamp computes `max(lda,0)`, which can still leave `lda=0` — violating
    the contract's implicit `>=1` floor. Confirmed reachable: a
    column-major `DenseMatrix(0, nCol, Major::Column)` gets `rowStride_=1,
    colStride_=nRow_=0` from `setStride()`, so `laset()`/`maxAbs()`'s
    `rowStride_==1` fast path is taken with `m=0` — this isn't just
    theoretical, `addRow()` exists specifically to grow a matrix from an
    empty starting state, so a transient 0-row matrix is a plausible
    intermediate state in general use. Verified empirically (isolated
    standalone test, `M(0,5,Column)` through `operator=(T)`, `identity()`,
    `maxAbs()`, and `view()=`/`dlacpy_`) that this system's reference LAPACK
    does not crash or misbehave — Fortran `DO I=1,M` loops with `M=0`
    execute zero times, so `lda`'s value is never actually used to compute
    an address regardless of column count `N`, matching the same
    "unused-when-the-inner-loop-doesn't-run" reasoning already documented
    for the `n==1` case in items 5/10/11/23. Same latent-only-for-a-
    stricter-LAPACK-backend severity as those. Fixed by clamping to
    `std::max({lda, m, 1})` (`ldb` too, in the `operator=` case) at all
    four call sites instead of the two-way `std::max(lda, m)`. Re-verified
    with the same standalone `m=0` test (still no crash, now
    contract-legal) and the full `DenseMatrix<T>::test()` suite (`double`
    and `Complex`, zero failures).
