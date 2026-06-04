# Bug report

Files reviewed (lib/ and include/): anhb.cpp, anhbac.cpp, corehb.cpp, corehbac.cpp,
corehbcoloc.cpp, corehbnr.cpp, corehbxform.cpp, klubsmatrix.cpp, klumatrix.cpp,
anhb.h, anhbac.h, corehb.h, corehbac.h, corehbnr.h, klubsmatrix.h, klumatrix.h.

Severity ordered. Line refs are to source as read.

## Critical (correctness-breaking)

### 1. [x] [anhbac.cpp:123](lib/anhbac.cpp#L123) — Infinite recursion in `HBAC::requestsRebuild`
```cpp
auto [ok2, hbacRebuild] = requestsRebuild(s);   // calls itself
```
Should be `hbacCore.requestsRebuild(s)`. Any caller of `HBAC::requestsRebuild` blows
the stack. This will completely break HBAC the moment the analysis framework asks
whether to rebuild.

### 2. [x] [anhbac.cpp:124](lib/anhbac.cpp#L124) — Wrong combiner for ok flags
```cpp
return std::make_tuple(ok1||ok2, hbRebuild||hbacRebuild);
```
The "ok" flags must be AND'd (compare with `preMapping` at
[anhbac.cpp:133](lib/anhbac.cpp#L133) which uses `ok && ok1`). With OR, a failure in
one core is masked by success in the other.

### 3. [x] [corehb.cpp:620](lib/corehb.cpp#L620) — Null pointer dereference before null check
```cpp
auto& storedSpurs = continueState->solution.hbSpurs();
if (continueState && continueState->valid && ...) {
```
`continueState` is dereferenced *outside* the null check. `runSolver` is reachable
with `continuePrevious=true` while `continueState==nullptr` (caller didn't call
`restoreState`, or it returned false), in which case the `else` branches at lines
639/655 are clearly intended to handle null — but they're unreachable because the
program already crashed on line 620. Move the `auto&` binding inside the first `if`
branch.

### 4. [x] [corehbac.cpp:806](lib/corehbac.cpp#L806) — Wrong indexing in `HBACCore::dump`
```cpp
for (i=1; i<=n; i++) for (j=0; j<nf; j++) { ... auto c = acSolution.data()[i]; ... }
```
The element index ignores `j` entirely and is also missing the `*nf` stride. Should
be `acSolution.data()[i*nf + j]`. Every spur is printed with the same value, taken
from the wrong slot.

### 5. [x] [corehbnr.cpp:524](lib/corehbnr.cpp#L524) — Missing `return` in `buildSystem`
```cpp
if (haveForces() && !loadForces(true)) {
    ...
    std::make_tuple(false, evalSetup_.limitingApplied);   // value discarded
}
return std::make_tuple(true, false);
```
The error tuple is constructed and dropped; the function falls through to the
success return. Combined with the next bug, the failure path here is currently dead
code, but the day `loadForces` learns to fail this will silently report success.

### 6. [x] [corehbnr.cpp:531-582](lib/corehbnr.cpp#L531-L582) — `loadForces` cannot fail
Body has a single `return true;` and no error paths, yet the caller checks
`!loadForces(true)`. Not a bug: the return value is part of the API for
forward-compatibility — `loadForces` may grow failure modes later, and the call
site at #5 is already wired to handle them.

## Serious

### 7. [x] [klubsmatrix.cpp:238-244](lib/klubsmatrix.cpp#L238-L244) — `dumpBlockSparsity` ignores its `os` parameter
Writes to `std::cout` everywhere instead of the supplied `std::ostream& os`. Caller
redirecting output gets nothing.

### 8. [x] [corehbcoloc.cpp:30-32](lib/corehbcoloc.cpp#L30-L32) — Validation sets status but doesn't bail
```cpp
if (params.samplefac<1.0) {
    s.set(Status::BadArguments, "samplefac must be >=1.");
}
```
Missing `return false;`. Function proceeds with the bad parameter and returns
success at the end, leaving the caller with an inconsistent (status=error,
retval=true) outcome.

### 9. [x] [corehbac.cpp:85-103](lib/corehbac.cpp#L85-L103) — Failures swallowed in output-source loop
```cpp
for (i=0; i<nStoredSpurs; i++) {
    ...
    ok = addComplexVarOutputSource(...);     // overwrites previous ok
}
```
A failure on iteration k is overwritten by success on iteration k+1; the outer
`if (!ok) break;` (line 100) sees only the last iteration. Track failures with
`ok = ok && addComplex...` and break inside the loop.

### 10. [x] [corehb.cpp:56-63](lib/corehb.cpp#L56-L63) and [corehbac.cpp:64-67](lib/corehbac.cpp#L64-L67) — Uninitialized members + wrong-order init list
In `HBCore`, the init list omits `lastHbError`, `homotopySteps`, `converged_`. In
`HBACCore`, the init list omits `lastHBACError`, `errorFreq`, `errorInst`,
`errorSpur`, `frequency`. If `formatError`/`finalizeOutputs` runs before the
analysis pass that calls `clearError`/`coroutine`, you read indeterminate values.
The init lists are also written in an order that doesn't match the declaration
order, which both produces warnings and obscures what is actually initialized
first.

### 11. [x] [corehb.cpp:60-61](lib/corehb.cpp#L60-L61) — `spurs_.spectrum()` called before `spurs_` is constructed
`nrSolver` is declared in the header before `spurs_`, so members are constructed in
that order regardless of the init list. The nrSolver init list expression evaluates
`spurs_.spectrum()` — a non-static member function call on an object whose lifetime
hasn't started ([basic.life]). Currently works because of standard layout, but it's
UB. Move `spurs_` above `nrSolver` in the class declaration (and pass the reference
normally), or pass the spurs in via `rebuild()`.

### 12. [x] [klumatrix.cpp:62-74](lib/klumatrix.cpp#L62-L74) — `KluMatrixCore` constructor: scattered uninits
Init list omits `nnz_`, `errorIndex`, `errorRank_`, `errorNan`, `isComplex_`,
`bucket_`. `nnz_` is read in `zero()` and a number of `for` loops if `rebuild()`
has not completed (or has failed mid-way). `bucket_` for `ValueType=double` is
indeterminate; any code that takes `elementPtr` for an absent entry returns a
pointer to that indeterminate scratch slot — fine for writes, but reads are UB.

### 13. [x] [klumatrix.cpp:259-265](lib/klumatrix.cpp#L259-L265) — `isFactored()` lies after rank-deficient factor
On a rank-deficient outcome (KLU returns non-null `numeric` but
`numerical_rank != AN`), `factor()` flags `Error::Factorization` but leaves
`numeric` populated. `isFactored() { return numeric; }` then reports success.
Either free `numeric` on this path or have `isFactored` consult `lastError`.

### 14. [x] [klumatrix.cpp:280-282](lib/klumatrix.cpp#L280-L282) — `refactor()` accounting wrong on fallback
`refactor()` bumps `cxrefactor`/`refactor` counters at the top, then if `!numeric`
it tail-calls `factor()` which bumps the factor counters again. Net effect: the
single operation is counted as both refactor and factor, with timing accumulated to
refactor before the actual work happens.

## Moderate

### 15. [x] [corehbnr.cpp:326](lib/corehbnr.cpp#L326) — `double[]` ↔ `Complex` reinterpret_cast
```cpp
solutionFD[destOrigin+k] = *reinterpret_cast<Complex*>(&data[srcOrigin+1+(k-1)*2]);
```
Reading a pair of `double`s through a `std::complex<double>*` violates strict
aliasing (works in practice, formally UB). The `complex→double*` direction is
blessed by [complex.numbers.general]; the reverse is not. Replaced with explicit
`Complex(data[base], data[base+1])` construction. (The bug entry originally
also pointed at lines 629-638 in `checkDelta`, but those read two doubles
directly with no cast — not UB.)

### 16. [x] [corehbxform.cpp:47](lib/corehbxform.cpp#L47), [corehbxform.cpp:52](lib/corehbxform.cpp#L52) — Variable-length arrays
```cpp
double baseFac[nBase];
double basePhaseAtTstart[nBase];
```
VLAs are a GCC extension, not portable. Use `std::vector` (or a small `std::array`
if `nBase` is bounded).

### 17. [x] [corehbcoloc.cpp:28](lib/corehbcoloc.cpp#L28) — Unguarded `spectrum()[1]`
`auto fmin = spurs_.spectrum()[1];` requires `spectrum().size() >= 2`. There's a
check earlier in `rebuild()` ([corehb.cpp:293](lib/corehb.cpp#L293)) but no guard
here. If `buildColocation` is ever called via another path, this is OOB.

### 18. [x] [corehbnr.cpp:463-470](lib/corehbnr.cpp#L463-L470) — `op_nsiter` used for HB
```cpp
auto nsiter = circuit.simulatorOptions().core().op_nsiter;
if (iteration==nsiter+1) { enableForces(0,false); enableForces(1,false); }
```
Re-using the OP setting for HB is suspicious; HB likely needs its own setting (or a
documented reason for sharing).

### 19. [x] [corehbac.cpp:502-540](lib/corehbac.cpp#L502-L540) — `co_yield CoreState::Aborted` not followed by `co_return`
Five places yield `Aborted` and then fall through to the next line. Works today
because the only caller (`run`) destroys the coroutine handle on Aborted, but the
fall-through statements (`acSolution.resize`, etc.) are reachable if anyone ever
resumes the coroutine. Add `co_return;` after each abort.

### 20. [x] [corehbac.cpp:235](lib/corehbac.cpp#L235) — Double semicolon
`auto jacIndex = &stencil.at(start, m);;` — harmless typo.

## Minor / cosmetic

### 21. [ ] [klumatrix.cpp:38-43](lib/klumatrix.cpp#L38-L43) — Unused locals in `enumerate()`
`auto e = it->first; auto u = it->second;` computed and never used.

### 22. [ ] [klumatrix.cpp:448](lib/klumatrix.cpp#L448) and [klumatrix.cpp:515](lib/klumatrix.cpp#L515) — Outer-scope `col1, col2`
Declared at outer scope, used only by inner reassignments. Move into the loop body.

### 23. [ ] [corehbnr.cpp:254](lib/corehbnr.cpp#L254) — Unused local
`auto nb = circuit.unknownCount();` unused.

### 24. [ ] [klubsmatrix.cpp:130](lib/klubsmatrix.cpp#L130) — Unused local
`auto blockCount = denseColumnBegin[n];` unused.

### 25. [ ] [klumatrix.cpp:411-437](lib/klumatrix.cpp#L411-L437) — Asymmetric inf/nan reporting
`isFinite(ValueType*, ...)` uses `nanCheck` then `else if (infCheck)` — if both are
requested and a value is both inf and nan, the report is biased to the first check.
Harmless but inconsistent with the matrix-side overload at line 365 which
OR-accumulates.

### 26. [ ] [klubsmatrix.cpp:9-11](lib/klubsmatrix.cpp#L9-L11) — Init list order reverse of declaration order
`(blockBucket_, largeBucket_)` — warning.

### 27. [ ] [klubsmatrix.h:105-118](include/klubsmatrix.h#L105-L118) — `elementPtr` not-found ignores `Component`
For `Complex` matrices, an `Imaginary` request lands on the real half of the
bucket. Bucket writes are discarded so it doesn't matter functionally, but it's
inconsistent with the `found` branch which returns `+1` for imaginary.

### 28. [ ] [corehbac.cpp:464](lib/corehbac.cpp#L464) — Redundant `std::move` on rvalue
`excitations.push_back(std::move(Excitation(inst, {}, {})));` — `std::move` on an
rvalue is redundant (and inhibits potential `emplace_back` improvements).

### 29. [ ] [corehbcoloc.cpp:75](lib/corehbcoloc.cpp#L75) — Return value ignored
`buildTransformMatrix(IAPFT)` return value ignored (the function always returns
true today, so latent).

### 30. [ ] [corehb.cpp:977](lib/corehb.cpp#L977) — Hard-to-read predicate
The `i==1 && ... || i!=1 && ...` predicate works due to precedence but is hard to
read; parenthesize.

## Two patterns worth a project-wide pass

### 31. [ ] References to references in constructor init lists
Several cores pass member references through the init list of a base/sibling that
runs first (HBCore→HBNRSolver via `spurs_.spectrum()`, the bsjac/solution chain).
These compile but are fragile — declaration order is what matters.

### 32. [ ] `clearError()` doesn't reset auxiliary error fields
(`errorIndex`, `errorRank_`, `errorNan`, `errorFreq`, `errorInst`, `errorSpur`).
Each `formatError` only reads them when its specific enum is set, so it's safe
today; but adding a new error variant that reads more fields is a bug waiting to
happen.

The two I'd fix first: #1 (`HBAC::requestsRebuild` recursion) and #3 (`runSolver`
null deref). Both are reachable and crash-class.

## Fragile reference binds (project-wide audit triggered by #11)

Scanned all 96 constructors in `lib/*.cpp` plus inline ctors in `include/*.h`.
Only #11 was true UB (method call on a not-yet-constructed member). The entries
below are the **legal-but-fragile pattern**: a constructor binds a reference to
a sibling member that is declared *later* in the same class. Standard permits
this so long as the constructor only stores the reference and never
dereferences it before the referent's lifetime begins. None of these
constructors dereference today — but the same misstep that produced #11 lives
one edit away in each of them. Worth a project-wide cleanup pass: reorder the
class members so all referents precede their consumer, the way #11 was fixed.

### 33. [ ] [anhb.cpp:12-17](lib/anhb.cpp#L12-L17) — `HB::HB`
`core(*this, params.core(), circuit, commons, jacColoc, jac, solution)` —
`jacColoc`, `jac`, `solution` are declared after `core` in
[anhb.h:70-75](include/anhb.h#L70-L75).

### 34. [ ] [anhbac.cpp:7-11](lib/anhbac.cpp#L7-L11) — `HBAC::HBAC`
`hbCore(...jacColoc, jac, solution)` and
`hbacCore(...jacSpec, hbSolution, acMatrix, acSolution)` — every matrix/vector
reference is declared after the core that captures it in
[anhbac.h:68-78](include/anhbac.h#L68-L78).

### 35. [ ] [anop.cpp:12-14](lib/anop.cpp#L12-L14) — `OperatingPoint::OperatingPoint`
`core(*this, params.core(), circuit, commons, jac, solution, states)` —
`jac`, `solution`, `states` are declared after `core` in
[anop.h:69-74](include/anop.h#L69-L74).

### 36. [ ] [antran.cpp:7-10](lib/antran.cpp#L7-L10) — `Tran::Tran`
`opCore(...jac, solution, states)` and `tranCore(...jac, solution, states)` —
`jac`, `solution`, `states` are declared after both cores in
[antran.h:64-70](include/antran.h#L64-L70).

### 37. [ ] [coretran.cpp:252-272](lib/coretran.cpp#L252-L272) — `TranCore::TranCore`
`nrSolver(...nrSettings, integCoeffs)` — `integCoeffs` is declared after
`nrSolver` in [coretran.h:177-179](include/coretran.h#L177-L179).
(`nrSettings` precedes `nrSolver` and is fine.)

### 38. [ ] HBCore residual pattern still in `nrSolver` init
Even after the #11 fix, `HBCore`'s init list still passes `solutionFD`,
`timepoints`, `APFT`, `IAPFT`, `OmegaGamma`, `GammaInvColumnMajor` to
`nrSolver` by reference. All of these are now declared *before* `nrSolver`
(post-fix), so this is no longer fragile — listed for completeness so a future
header re-shuffle doesn't reintroduce the issue.

