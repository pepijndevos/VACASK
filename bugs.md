# feature/blas audit — full findings dump

Unfiltered transcription of every finding from all three review passes:
Run 1 = correctness-focused pass over the full `main...feature/blas` diff.
Run 2 = cleanup/altitude/conventions pass over the unpushed local delta
(`include/common.h`, `include/densematrix.h`, `lib/spurs.cpp`).
Run 3 = correctness-focused pass (angles A/B/C/D) over the same unpushed
delta. Overlaps between runs are left in and cross-referenced rather than
merged away.

## Run 1 — correctness pass, full branch diff vs main

1. [ ] `include/densematrix.h:336` — `vectorPlusScaledVector()`'s generic
   (non-`double`/`Complex`) fallback does `*ptr = *v0 + *ptrOther * factor;`
   but dereferences the `VectorView<T>` `v0` itself (no `operator*` exists on
   it) instead of the element pointer `ptrV0` declared and advanced two lines
   above for this purpose. Dead today because every live call site uses
   `T=double` and the double/Complex branches use BLAS instead, but fails to
   compile the moment this is instantiated for another `T` (e.g.
   `VectorView<Int>`, already used elsewhere in the file for pivot/index
   vectors).
2. [ ] `include/densematrix.h:33` — `subVector()`'s bounds check is
   `DBGCHECK((offset+n*stride)*stride_>n_*stride_, ...)`, should be
   `offset+(n-1)*stride`. Rejects valid strided sub-views whenever
   `stride>1`: e.g. `n_=5, offset=0, n=3, stride=2` accesses indices
   `{0,2,4}` (all in-bounds, max index 4<5), but the check computes
   `0+3*2=6>5` and throws in a Debug/`FORCE_DBGCHECK` build. All current
   call sites (`corepss.cpp`) pass the default `stride=1`, where the formula
   happens to be correct, so this is latent until `subVector` is used with
   `stride>1`.
3. [ ] `CMakeLists.txt:313` — the Darwin build path never links BLAS/LAPACK:
   `find_package(BLAS/LAPACK)` only runs `if (${SIM_PLATFORM} STREQUAL
   "Linux")`, and the Darwin `SIM_EXE_LIBRARIES` list (line 313) has only
   `klu amd camd colamd ccolamd btf cholmod suitesparseconfig`, no
   BLAS/LAPACK/Accelerate. Since `include/blaslapack.h`'s `extern "C"`
   routines are called from essentially every `VectorView<double>`/
   `VectorView<Complex>` operation (copy, scale, dot, norm, ...), not just
   `DenseMatrix` factor/solve, the macOS link step will fail with undefined
   symbols (`dcopy_`, `ddot_`, `dgetrf_`, ...) for the whole simulator
   binary. The accompanying comment acknowledges "macOS not wired up yet",
   so this may be a known/deferred gap rather than an oversight — but it is
   a real, total build break on a platform the CMakeLists still branches
   for.
4. [ ] `include/densematrix.h:52` (and pervasively throughout the file) —
   numerous previously-unconditional `throw std::out_of_range(...)`
   invariant checks (vector/matrix length and shape mismatches) were
   converted to `DBGCHECK`, a Release-mode no-op. E.g.
   `VectorView<T>::operator=(const VectorView<T>&)` used to always throw on
   `n_ != from.n_`; now it only throws when `SIMDEBUG`/`FORCE_DBGCHECK` is
   defined. Same pattern repeats through `swap()`, `dot()`, `addScaled()`,
   `DenseMatrixView::multiply()/factor()/luSolve()`, etc. In a Release
   build, a caller bug that previously produced an immediate, clear
   `std::out_of_range` now silently drives BLAS/LAPACK with mismatched
   lengths/strides — out-of-bounds memory access or silently wrong numeric
   results instead of a loud failure. Applied broadly across the hot
   numeric core.
5. [ ] `include/densematrix.h:541-566` — `identity()` uses `rowStride_==1`
   as a proxy for "column-major", but a **row-major** matrix with
   `nCol_==1` also has `rowStride_==1` (since `rowStride_=nCol_=1`). It gets
   misclassified as column-major and calls `dlaset_`/`zlaset_` with
   `lda=colStride_=1` while `m=nRow_` can be `>1`, violating LAPACK's
   `lda>=max(1,m)` contract. Happens to produce correct output today only
   because `n=1` in this case, so `lda` is never multiplied by a nonzero
   column index — coincidental, not by construction. No current caller hits
   it (`corepsstran.cpp`, `corehb.cpp` call sites are all square, `nCol_>1`).
   (Independently found again as A1 in Run 3.)

## Run 2 — cleanup/altitude/conventions pass, unpushed delta

6. [ ] `include/densematrix.h:473-475` — new `ptrOf()` accessors
   (const/non-const) have zero callers anywhere in the repo. Dead code
   added with no current use. (Same file/lines independently noted as C6
   in Run 3.)
7. [ ] `include/common.h:102-103` — new `CHECK(cond, msg)` macro has zero
   call sites. Duplicates `DBGCHECK`'s exact body unconditionally, but
   nothing in this diff (or the rest of the tree) uses it.
8. [ ] `include/densematrix.h:492-493` (in `operator=(const
   DenseMatrixView&)`) vs. `include/densematrix.h:514-515` (in
   `operator=(const T&)`) — `packedRowMajor`/`packedColMajor` stride-check
   logic is copy-pasted between the two `operator=` overloads, one variant
   additionally checking `other.colStride_`/`other.rowStride_`, the other
   not. Two near-identical boolean formulas to keep in sync; the existing
   diff already shows the two copies drifting (different variables
   checked). (Same duplication independently noted as D1 in Run 3.)
9. [ ] `include/densematrix.h:1120-1140` — `addRow()`'s new column-major
   reshape branch is not exercised by any caller in the repository: only 3
   call sites (all in `lib/spurs.cpp`, all on matrices constructed with the
   default `Major::Row`), and every `Major::Column` matrix in the tree is
   sized once via `resize()` and never grown via `addRow()`. A full
   reshape-and-copy code path (with its own explanatory comment) was added
   to support a currently-unused capability. (Independently found again as
   B4/C4 in Run 3.)
10. [ ] `include/densematrix.h:512-533` (`operator=(const T&)`) — the
    fast-path condition (`packedRowMajor`/`packedColMajor`, i.e. the
    *entire* `nRow_*nCol_` region must be one contiguous run) is strictly
    narrower than what LAPACK's `dlaset_`/`zlaset_` actually supports and
    narrower than what `identity()` (added in the same diff, lines
    542-560) uses for the *same* underlying routine. `identity()` only
    requires `rowStride_==1` (or `colStride_==1`) and passes the real
    `lda` (`colStride_`/`rowStride_`) to `dlaset_`, correctly fast-pathing
    genuine sub-matrix/block views (documented in
    `docs/internals/klubsmatrix.md`: "row stride = 1, column stride =
    blockColumnStride[col]", which need not equal `nRow_`).
    `operator=(const T&)`/`zero()` on that same kind of view falls through
    to the slow per-row loop instead of taking a `dlaset_` fast path, even
    though one is directly available by copying `identity()`'s `m`/`n`/
    `lda` computation. Missed optimization: fill on strided block views (a
    real usage pattern) does `nRow_` separate calls instead of one.
11. [ ] `include/densematrix.h:486-509` (`operator=(const
    DenseMatrixView&)`) — same narrowing as #10: the packed-only check
    means a copy between two same-shaped strided sub-block views
    (`rowStride_==1`, differing/larger `colStride_`/`lda` on each side)
    does not get any BLAS fast path at all, falling all the way back to
    `nRow_` row-copies (each dispatching into `VectorView::operator=`'s
    own BLAS call) — extra indirection where a direct per-column `dcopy_`
    with proper `lda` would do the same work with less overhead.
12. [ ] `include/densematrix.h:1020-1175` (`DenseMatrix<T>`) — dropping
    `DenseMatrix`'s own `at()`/`row()`/`column()` in favor of the inherited
    `DenseMatrixView` versions changes *all* accessors (not just the
    previously-`start_`-based `const` ones) to depend on `start_` being
    kept in sync with `data_.data()`. Before this diff, non-`const`
    `at()`/`row()`/`column()` indexed `data_` directly (immune to a stale
    `start_`); after, every accessor dereferences the cached `start_`
    pointer. `data()` (lines 1143-1144) still hands out a mutable
    `std::vector<T>&` reference to `data_` with no accompanying way to
    resync `start_`/strides; any external mutation through that reference
    that reallocates (`push_back`, `resize`, `insert`) leaves `start_`
    dangling, and now *every* accessor would silently read/write freed
    memory. No current caller does this, but the refactor removes a small
    amount of latent robustness for code reuse, enforced only by
    programmer discipline.
13. [ ] `include/densematrix.h:486-566` — the BLAS/LAPACK fast-path
    dispatch for `operator=`/`identity()` is implemented as three separate
    hand-rolled `if constexpr` + stride-arithmetic blocks (see #8, #10
    above) rather than going through one shared "is this view
    LAPACK-packable, and what are its effective `m`/`n`/`lda`" helper.
    `factor()`/`luSolve()`/`factorAndLuSolve()` (pre-existing, lines
    570-751) already establish this "compute LAPACK parameters if
    contiguous, else fall back" pattern; the new code reimplements a
    variant of it three more times with slightly different generality each
    time (see #10's inconsistency). A single private helper returning
    `{m, n, lda}` (or `std::nullopt`) would remove the duplication and fix
    the #10 inconsistency as a side effect. (Same theme independently
    noted as D6 in Run 3.)
14. [ ] `include/common.h:102-103` — project memory guidance about
    `DBGCHECK` not being about optimizing around its cost doesn't directly
    forbid `CHECK`, but another memory note ("usage errors should throw")
    appears to be the motivation for `CHECK` existing at all — yet the
    diff introduces the macro without converting any existing `DBGCHECK`
    call site to use it and without adding any new call site. Worth
    confirming with the author whether `CHECK` is meant to be adopted in a
    follow-up change; as it stands it's unreferenced.
15. [ ] `docs/internals/densematrix.md:44` — states `addRow()` "throws
    `std::out_of_range` for a column-major matrix, since appending a row
    there would require restriding every existing column." Now factually
    wrong: the diff's `addRow()` no longer throws for column-major
    matrices — it performs exactly the "restriding every existing column"
    the doc says is prohibited, via a full reshape-copy. Per project
    convention (update docs on noticed drift), this doc entry should have
    been updated alongside the code change.
16. [ ] `include/densematrix.h:544-546`, `489-491`, `609-611`/`717-718`
    (extensive new inline comments explaining LAPACK column-major/`lda`/
    packing conventions) — `docs/internals/densematrix.md` and
    `docs/internals/blaslapack.md` already exist for exactly this kind of
    rationale (see `densematrix.md:32`, "LAPACK dispatch condition:..."
    for the pre-existing `factor()`/`luSolve()` dispatch). The new
    inline comments restate this rationale in the header rather than in
    the designated doc file, and (per #15) the doc file wasn't updated to
    cover the new `operator=`/`identity()`/`ptrOf()`/`addRow()` additions
    at all. Not a hard rule violation, but the explanatory content now
    lives somewhere other than where the project's doc structure puts it.
17. [ ] `lib/spurs.cpp:463` and `lib/spurs.cpp:555` — `.fill(x)` → `= x`,
    explicit `VectorView<Int>(...)` call-site updates. No convention
    violation found; included for completeness. Note: `mixingStencil_` is
    `Major::Column` (set at `spurs.cpp:460`), so `mixingStencil_ =
    noJacIndex` at line 463 goes through the packed-only fast path in
    `operator=(const T&)`, which it does satisfy here since it's a
    freshly-`resize()`d, fully-packed matrix — no issue in this specific
    instance, just noting the call site that would be affected if the
    matrix were ever a sub-view instead.

## Run 3 — correctness pass, angles A/B/C/D, unpushed delta

### Angle A — line-by-line diff scan

18. [ ] **A1.** `include/densematrix.h:541-566` (`identity()`) — same bug
    as Run 1 item 5: `rowStride_==1` misclassifies a row-major `nCol_==1`
    matrix as column-major, calling `dlaset_`/`zlaset_` with an `lda`
    that violates LAPACK's `lda>=max(1,m)` contract. Failure scenario:
    `DenseMatrix<double> M(5,1,Major::Row); M.identity();` — no assertion
    trips with reference LAPACK, but a stricter/validating BLAS backend
    (or a future refactor that also changes `n`) could read/write out of
    bounds.
19. [ ] **A2.** `include/common.h:103` — `#define CHECK(cond, msg) if
    (cond) { throw std::runtime_error(msg); }` has no `do { } while(0)`
    wrapper (same style as `DBGCHECK`, but `CHECK` is unconditional and
    new). No live bug (zero call sites), but a dangling-else/
    missing-semicolon trap waiting for the first caller, e.g. `if (a)
    CHECK(b, "x"); else f();` silently binds the `else` to the macro's
    inner `if`. (Same finding as Run 2 item 14's sibling concern.)
20. [ ] **A3.** `include/densematrix.h:1032` (`using
    DenseMatrixView<T>::operator=;`) combined with `DenseMatrix`'s own
    copy-assign (1089) and move-assign (1100) creates a 4-candidate
    overload set. Traced overload resolution for the live call sites
    (`spurWeights_ = std::move(weightsSorted);`, `mixingStencil_ =
    noJacIndex;`, `static_cast<DenseMatrixView<double>&>(coeffs) =
    IAPFT;`) — all resolve to the intended overload today. Flagged as a
    fragile overload set: a future call site that assigns one
    `DenseMatrix<T>` to another through a base-typed reference/pointer
    without an explicit cast could resolve unexpectedly.
21. [ ] **A4.** `include/densematrix.h:486-509` and `512-533` — the
    `packedRowMajor`/`packedColMajor` fast-path guards correctly require
    both operands to be identically packed and same-major; verified this
    correctly falls back to the per-row loop when majors differ (e.g.
    `lib/corehbxform.cpp:113`). No bug found, but the two conditions are
    duplicated rather than centralized (see #8/D1), making them easy to
    accidentally weaken in a future edit.
22. [ ] **A5.** `include/densematrix.h:1120-1140` (`addRow()`,
    column-major branch) — relies on `std::vector<T>
    newData((nRow_+1)*nCol_)` value-initializing the new row's slots to
    zero. Correct for the currently-instantiated `T` (`int`, `double`,
    `Complex`), but implicit/easy to break if the vector were ever built
    via `reserve`+`push_back` instead of a sized constructor.
23. [ ] **A6.** `include/densematrix.h:541-566` — `identity()`'s LAPACK
    dispatch (three nested ternaries picking `m`/`n`/`lda`) was manually
    traced against the naive `at(i,j)=(i==j)?1:0` loop for square,
    non-square (2x3, 3x2), and padded cases and matches in every case
    checked — but there is no unit test in `DenseMatrix<T>::test()`
    (`lib/densematrix.cpp`) exercising `identity()` on a non-square or
    padded view. Worth adding one given how easy a sign/branch-swap typo
    would be to introduce here undetected.

### Angle B — removed-behavior auditor

24. [ ] **B1.** Old `DenseMatrix<T>::fill(T)` (removed) — only call site
    was `lib/spurs.cpp:463`, already updated by the diff to
    `mixingStencil_ = noJacIndex;`. No orphaned callers, no compile
    breakage expected.
25. [ ] **B2.** Old `DenseMatrix::at()/row()/column()` overrides
    (removed) — manually re-derived the offset/stride arithmetic for both
    `Major::Row` and `Major::Column` against the old switch-based
    formulas; byte-for-byte equivalent in all four cases. No discrepancy
    found.
26. [ ] **B3.** `start_` resync — traced every constructor, copy/move
    assignment, `resize()`, and `addRow()` path; `start_ = data_.data()`
    is issued after every mutation of `data_` in each path, no
    post-construction window with a stale `start_`. No bug found, but
    flagged since it's now an invariant relied upon by every accessor
    (see #12).
27. [ ] **B4.** Old `DBGCHECK(major_==Major::Column, "Rows cannot be
    added to column major matrices.")` and "Row major only" comment
    (removed) — confirmed no call site in the repo constructs/resizes the
    matrix passed to `addRow()` as `Major::Column`. The new column-major
    reshape branch is therefore currently dead/unexercised code — logic
    looks correct on inspection but has no test coverage. (Same finding
    as Run 2 item 9 / C4 below.)
28. [ ] **B5.** Old `DenseMatrix::zero()` (`data_.assign(data_.size(),
    T())`, removed) vs. new inherited `DenseMatrixView::zero()` (`*this =
    0`) — confirmed equivalent for the only instantiated `T`s (`int`/
    `Int`, `double`/`Real`, `Complex`), since a freshly-owned
    `DenseMatrix` is always fully packed so either path touches exactly
    `data_`'s full extent.
29. [ ] **B6.** `lib/corehbxform.cpp:113`:
    `static_cast<DenseMatrixView<double>&>(coeffs) = IAPFT;` — this cast
    is still necessary and correct post-diff: `coeffs = IAPFT;` (both
    `DenseMatrix<double>`) would resolve to `DenseMatrix`'s own
    copy-assignment (exact-match wins per A3), which copies `major_` too
    and would silently turn `coeffs` from column-major into row-major,
    breaking the "must stay column-major for LAPACK" invariant documented
    right below it. Not diff-introduced (predates the diff), but the new
    `using DenseMatrixView<T>::operator=;` makes it *look* like the cast
    might now be droppable — it is not.

### Angle C — cross-file tracer

30. [ ] **C1.** `lib/spurs.cpp:463` `mixingStencil_ = noJacIndex;` —
    `mixingStencil_` is `DenseMatrix<Int>` (`Int`=`int32_t`), so
    `operator=(const T&)`'s `constexpr` fast path is skipped entirely
    (falls through to the plain per-row loop, itself skipping
    `VectorView`'s BLAS fast path too) — functionally identical to the
    old `.fill(noJacIndex)`, just without any BLAS speedup. Not a bug,
    just noting the new BLAS fast path never fires for this call site.
31. [ ] **C2.** `lib/corepsstran.cpp:118,378` (`phiHist_.at(i).identity()`,
    `Omega.identity()`) and `lib/corehb.cpp:953` (`I.identity()`) — all
    square matrices with `nCol_==nRow_>1` in all three cases, so A1/#18's
    edge case does not currently affect any live caller.
32. [ ] **C3.** `lib/corehbxform.cpp:113` — the `operator=(const
    DenseMatrixView&)` fast path added by this diff is correctly skipped
    here (mismatched majors between `coeffs` and `IAPFT`), so this call
    site gets no speedup but also no regression.
33. [ ] **C4.** `lib/spurs.cpp:124,259,291` (`addRow()` call sites) — all
    operate on matrices that are always `Major::Row` (default). The new
    column-major reshape branch in `addRow()` is unreachable from any
    current caller. (Same finding as B4/#27 and Run 2 item 9.)
34. [ ] **C5.** `lib/spurs.cpp:266` — `VectorView(const_cast<Int*>(
    w.data()), w.size(), 1)` changed to `VectorView<Int>(...)` (explicit
    template argument added). Outside the densematrix.h/identity/addRow/
    operator= scope, but a real behavior-relevant edit: `Int` (`int32_t`)
    is the same underlying type as `smsigFreqMap`'s key type
    `VectorView<int>` on this platform, so very likely just a
    CTAD-ambiguity/compile fix — could not fully rule out that this line
    previously failed to compile or deduced a different type before the
    fix. Worth confirming with a clean rebuild.
35. [ ] **C6.** `include/densematrix.h:474-475` (`ptrOf()`) — not called
    from any file yet. Not a bug, but an unused public API surface added
    alongside the BLAS refactor suggests a follow-up call site may be
    missing (or intended for a not-yet-merged branch, consistent with the
    "hbac/pss branches pending" memory note). (Same finding as Run 2 item
    6.)

### Angle D — reuse/duplication

36. [ ] **D1.** `include/densematrix.h:486-533` — the
    `packedRowMajor`/`packedColMajor` stride-flattening check is written
    out twice (once per `operator=` overload), the second copy omitting
    the `other.*` comparisons. Could be factored into a small private
    helper (e.g. `bool isPacked() const`) shared by both overloads and by
    `identity()`'s "is this contiguous" check too. (Same as Run 2 item 8.)
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
39. [ ] **D4.** `include/common.h:97-103` — `CHECK` and `DBGCHECK` have
    textually identical bodies (`if (cond) { throw std::runtime_error(
    msg); }`), differing only in whether they're compiled out. Could
    define `DBGCHECK` in terms of `CHECK` to keep a single source of
    truth for the check expression's exact form.
40. [ ] **D5.** `include/densematrix.h:1120-1140` (`addRow()`,
    column-major branch) — reshapes the buffer with a raw pointer loop
    (`data_.data()+j*nRow_`, `std::copy`) rather than using the class's
    own `column()`/`VectorView` abstractions used everywhere else in the
    file. Minor consistency/reuse nit, not a bug.
41. [ ] **D6.** `include/densematrix.h:541-566` (`identity()`) vs.
    `factor()`/`luSolve()`/`factorAndLuSolve()` (×2) — all five methods
    independently re-derive "is this matrix LAPACK-column-major-compatible"
    (`rowStride_==1`) and `lda=colStride_`; `identity()` additionally
    handles the row-major/transpose case the LU methods don't need. A
    shared helper returning `{compatible, m, n, lda}` would remove the
    repeated boilerplate across all these methods, reducing the surface
    area for a mistake like A1/A6. (Same theme as Run 2 item 13.)
