# Bug audit — feature/pwl

Full-file audit of the files touched on `feature/pwl` (whole files, not just the diff).
Ordered roughly by severity. Each item is unchecked; tick when fixed/verified.

## lib/devvisrc.cpp — PWL core

- [x] **1. Bisection terminates after a single step (wrong PWL value on large index jumps).**
  [lib/devvisrc.cpp:522](lib/devvisrc.cpp#L522) — the loop maintains `i1 < i2`, but the
  exit test is `if (i1-i2<=1)`. Since `i1-i2` is always negative it is always `<=1`, so the
  `while(true)` bisection runs exactly one iteration and returns an index that can be far from
  the target. It only produces the correct point when the bracket is already adjacent (the
  common case when `lastPointIndex` tracks the marching time step, which is why the test still
  passes). Whenever `target` is more than one segment away from `lastPointIndex` (e.g. first
  evaluation, non-uniform/large time steps, negative `delay`) the returned index is wrong and
  the interpolated value is computed with the wrong segment slope. Condition should be
  `i2-i1<=1`.

- [x] **2. Periodic breakpoint period-offset (`i1Per`) is never updated — dead store + off-by-one-period break times.**
  Fixed: both branches now assign `i1Per = i1Per + (i1.value() < index ? 1 : 0);`.
  [lib/devvisrc.cpp:643](lib/devvisrc.cpp#L643) and [lib/devvisrc.cpp:664](lib/devvisrc.cpp#L664).
  In the periodic "all others (had index lookup)" branches:
  ```
  i1 = d.nextBreakIndex[index];
  i2Per = i1Per + (i1.value() < index ? 1 : 0);   // assigns i2Per, immediately overwritten
  i2 = d.nextBreakIndex[i1.value()];
  i2Per = i1Per + (i2.value() < i1.value() ? 1 : 0);
  ```
  The first assignment is meant to update **`i1Per`** (whether `i1` wrapped into the next
  period), not `i2Per`. As written `i1Per` stays `0`, so both `i1Per` and the final `i2Per`
  (which is computed from the stale `i1Per`) are wrong when the next breakpoint wraps past the
  end of the period. Result: `t1`/`t2` at [lib/devvisrc.cpp:676-677](lib/devvisrc.cpp#L676)
  miss one period and a breakpoint can be scheduled in the past. Masked in the test because
  `break="all"` makes `nextBreakIndex[index] = index+1 > index` except exactly at the wrap point.

- [x] **3. `pwlIndexLookup` is not self-guarding against `target == t[n-1]`.**
  Fixed: early-out now uses `target>=wave[2*(n-1)]`.
  [lib/devvisrc.cpp:452](lib/devvisrc.cpp#L452) — the early-out only handles `target > t[n-1]`.
  If `target == t[n-1]` the forward exponential expansion clamps `tryIndex` to `n-1`, where
  `tryTime == target` so `tryTime>target` is never true → **infinite loop**. Today every caller
  passes `target < t[n-1]` strictly, so it doesn't fire, but the function's own contract
  ("Assume target>=t[0]") doesn't forbid it; a future caller will hang. Add `target>=t[n-1]`
  to the early return.

- [x] **4. `DBGCHECK(dx<=0, ...)` is a release no-op guarding a division.**
  Fixed: replaced with a hard `BadArguments` check that returns before the division.
  [lib/devvisrc.cpp:313](lib/devvisrc.cpp#L313) — if `dx` ever reaches `0` the next line divides
  by zero in release builds (silent inf slope). The surrounding period validation makes `dx>0`
  in principle, but the only real guard is compiled out. Prefer a hard check that returns
  `BadArguments`.

## lib/rpnbuiltin.cpp

- [x] **5. `separate()` size_t underflow → out-of-bounds read when offset >= vector length.**
  Fixed: guard `offs>=srcSize` (yields empty), and use ceil division so the last element of
  each component (offset>0) is no longer dropped.
  [lib/rpnbuiltin.cpp:325](lib/rpnbuiltin.cpp#L325) — `nDest = (srcSize-offs)/n`. `srcSize` is
  `size_t` and `offs` is a (validated only as `0<=offs<n`) `Int`. If `offs >= srcSize`
  (e.g. `separate([1,2], 5, 3)`), `srcSize-offs` wraps to a huge unsigned value, `nDest` becomes
  enormous and the copy loop reads far past the end of the source vector. Add a guard for
  `offs >= srcSize` (yield an empty result).

- [x] **6. Duplicate `return true;` (dead code).**
  Fixed: only a single `return true;` remains.
  [lib/rpnbuiltin.cpp:281-283](lib/rpnbuiltin.cpp#L281) — `vectorInterleave` ends with two
  consecutive `return true;` statements; the second is unreachable.

- [x] **7. `interleave` length variable is `int`, compared against `size_t`.**
  Fixed: `n` is now `size_t`.
  [lib/rpnbuiltin.cpp:225](lib/rpnbuiltin.cpp#L225) — `auto n = 0;` makes `n` an `int`; it is
  then assigned `args[i]->size()` (`size_t`) and compared with `!=` against `size()`. Signed/
  unsigned comparison plus `n*argc` is `int` arithmetic that overflows for large vectors. Use
  `size_t`.

- [x] **8. Unused local `t` in `vectorRandUnif`.**
  Fixed: the dead `auto t = Value::Type::Real;` has been removed.
  [lib/rpnbuiltin.cpp:186](lib/rpnbuiltin.cpp#L186) — `auto t = Value::Type::Real;` is never
  used (dead store).

## lib/devvisrc.cpp — minor / cleanup

- [x] **9. Dead locals in the PWL `sourceCompute` case.**
  Fixed: dead locals removed/commented out; no live unused locals remain (leftover stale
  comment lines 893-895 are cosmetic).
  [lib/devvisrc.cpp:889-892](lib/devvisrc.cpp#L889) — `y1, y2, x1, x2, dx, breakTime,
  enforceBreakpoint, nextBreakIndex` are declared and never used.

- [x] **10. `timeTol` computed twice / first copy unused.**
  Fixed: removed the unused `timeTol`; only `tol` (computed where used) remains.
  [lib/devvisrc.cpp:895](lib/devvisrc.cpp#L895) computes `timeTol` (unused); the identical value
  is recomputed as `tol` at [lib/devvisrc.cpp:904](lib/devvisrc.cpp#L904).

- [x] **11. `using ssize_t` shadows the POSIX global and is declared after a use.**
  Fixed: renamed to `signed_size_t` (no POSIX collision) and moved to the top of the namespace,
  above all uses.
  `ssize_t` is first used inside `sourceSetup` at [lib/devvisrc.cpp:402](lib/devvisrc.cpp#L402)
  (resolving to the global POSIX type), but the namespace-local
  `using ssize_t = std::make_signed<size_t>::type;` is only introduced later at
  [lib/devvisrc.cpp:445](lib/devvisrc.cpp#L445). Two different `ssize_t` meanings depending on
  position; compiles but fragile. Move the alias above first use (or drop it and use the global
  consistently).

## include/rpnfunctor.h

- [x] **12. `FwMinAggregate` dereferences `x[0]` / `begin()+1` without an empty-vector guard.**
  Not a bug — empty vector is already an error. `min(vec)`/`max(vec)` dispatch through
  `mathAggregateNumFunc1`, which calls `functor.ok(v, s)` ([rpnbuiltin.h:194](include/rpnbuiltin.h#L194))
  before `functor(v)`. `FwMinAggregate::ok` / `FwMaxAggregate::ok`
  ([rpnfunctor.h:458](include/rpnfunctor.h#L458)) return `BadArguments` for a zero-length vector,
  so `operator()` never runs on an empty vector. No change needed.

## test/*.sim and demo/*.sim — reference models / netlists

No test-breaking bugs. The `test_vipwl.sim` reference model is functionally correct for the
current test data but "accidentally correct" — a couple of typo'd conditions only stay harmless
because of the specific waveform and sweep set.

- [x] **S1. `pwl_refval` last-point-elimination never triggers (typo).**
  Fixed: condition is now `if x[0]==0 and x[-1]==period:`. Verified correct under the phase
  rotation — dropping the closing point is safe because `y[0]==y[-1]` makes original `x=0` and
  `x=10` the same physical point, and the fix also removes the duplicate that made `xref`
  non-strictly-monotonic (see note below — now moot for the dropped-point case).

- [x] **S2. `pwl_refval` phase-start condition likely wrong.**
  Fixed: changed to `if (x[0]!=0):`. After rotation the period start sits at `x==0`, so the
  "already have a point at the start" check must compare against 0, not `tph`. Prevents a
  duplicate (non-monotonic) start point when a swept phase lands exactly on a sample.

- [x] **S3. `breaks` parameter accepted but ignored.**
  Not a bug — `breaks` is intentionally retained for forward-compat. We don't test
  `break="auto"/"none"`, so the model always treats every point as a breakpoint. Added a comment
  noting `breaks` will be needed when those modes are tested.

- Note: with S1 and S2 fixed, `xref` into `np.interp` ([line 138](test/test_vipwl.sim#L138)) is
  strictly monotonic in all cases (no duplicate boundary/start points).

- `demo/eye/eye.sim` — verified `interleave` operand lengths match exactly: `range(0, n/fs, 1/fs)`
  uses `round((v1-v0)/dv)` = exactly 5000 points, so `t` and `x` are both 10001. No bug.
- `test_viwfm.sim` — only `sinephase`→`tdphase` renames; consistent. No bug.

## Notes / verified-OK

- `vectorInterleave`/`vectorSeparate` stack juggling (`swap` into the deepest arg, then
  `pop(argc-1)` / `pop(2)`) is correct.
- Fixed RNG seed `std::mt19937_64 rng(0)` in `vectorRandUnif` is assumed intentional
  (reproducible runs), not a bug.
- `sinephase` → `tdphase` rename is applied consistently across sine/am/fm and the `.sim` tests.
