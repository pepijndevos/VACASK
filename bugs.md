# Bug report

Audit of all changes on branch `analysis/hbac` relative to `main` (merge-base
`4932989`). Covers every file touched by the branch (115 files, ~4500/~2000
lines +/-): HBAC core (`anhbac`/`corehbac`/`spurs`), the reworked HB core
(`corehb`/`corehbnr`/`corehbxform`/`corehbcoloc`/`anhb`), the KLU/dense matrix
layer, the `name_`->`prefixedName_` output-naming migration and shared
analysis infra (`core.cpp`, `ansmsig.h`, `an*.cpp`), the other core solvers
and device code, and the parser/lexer/RPN/build/test/demo files.

Severity ordered. Line refs are to source as read during the audit.

## Confirmed (correctness-breaking)

### 1. [ ] [corehbnr.cpp:132](lib/corehbnr.cpp#L132) — Out-of-bounds read in `HBNRSolver::setForces` when a stored nodeset comes from a smaller circuit
```cpp
if (solNames.size()>0) {                 // was: solNames.size()==n+1  (main)
    checkNames = true;
...
for(decltype(n) i=1; i<=n; i++) {
    if (checkNames) {
        node = circuit.findNode(solNames[i]);   // solNames[i] with i up to n
```
`main` required `solNames.size()==n+1` before trusting name-based lookup by
index. This branch relaxed it to merely `solNames.size()>0` and dropped the
length check, while the loop still runs `i` up to the *current* circuit's
unknown count `n`. `AnnotatedSolution::names()` is sized by the *storing*
circuit's own unknown count, so using `nodeset=<name>` where the stored
solution came from a circuit with fewer unknowns than the one currently being
solved reads `solNames[i]` past the end of the vector — UB (crash, or a
garbage `Id` silently matching an unrelated node and forcing the wrong
nodeset value instead of erroring out).

### 2. [ ] [corehb.cpp:631](lib/corehb.cpp#L631) — Warm-start "ordinary continue" path in `HBCore::runSolver` is dead and, if reached, would crash
```cpp
if (continueState &&
    continueState->valid && continueState->coherent &&
    continueState->solution.cxValues().size()==circuit.unknownCount()*timepoints.size() &&
    ...
    solution.vector() = continueState->solution.values();
```
The size check compares `cxValues().size()` (`n * nf`, spectrum size) against
`unknownCount()*timepoints.size()` (`n * nt`, colocation-point count).
`nt = 2*nf-1` always (`corehbcoloc.cpp:30`) with `nf>=2` enforced, so
`nt != nf` always holds and this branch can never be taken — the "ordinary
continue mode with stored analysis state" path is permanently dead. Even if
it were reached, `continueState->solution.values()` does
`std::get<std::vector<double>>(values_)`, but HB only ever stores state via
`setCxValues()` (`std::vector<Complex>`) — that `std::get` would throw
`std::bad_variant_access`. This looks like `coreop.cpp`'s continuation logic
copy-pasted without adapting it to HB's complex-spectrum representation; net
effect is HB continuation silently always falls back to the slower
nodeset-forcing path even when a fast warm start should be possible.

### 3. [ ] [corehbnr.cpp:728](lib/corehbnr.cpp#L728) — Wrong DC value printed for every unknown but the first in `HBNRSolver::dumpSolution`
```cpp
for(decltype(n) i=1; i<=n; i++) {
    ...
    for(decltype(nf) k=0; k<nf; k++) {
        Complex x;
        if (k==0) {
            x = solution[0];                       // always index 0, ignores i
        } else {
            auto ndx = (i-1)*nt+1+(k-1)*2;
```
Every other consumer of this layout (`checkDelta`'s `baseI=(i-1)*nt`,
`postRun`'s `srcOrigin=i*nt`) correctly offsets by unknown index `i`; the
`k==0` (DC) branch here always reads `solution[0]` regardless of `i`, so the
DC value printed for unknown `i>1` is actually unknown #1's DC value. This is
reachable in normal debugging use, not just theoretical: `nrsolver.cpp`
calls `dumpSolution` whenever `nr_debug>=2`, so every NR iteration during an
HB run with elevated `nr_debug` prints an incorrect DC term for all but one
unknown — actively misleading during convergence debugging.

### 4. [ ] [ansmsig.h:157](include/ansmsig.h#L157) — `Status` dropped for every small-signal analysis's output-descriptor resolution
```cpp
if (!opCore.resolveOutputDescriptors(strict)) {      // no `s` passed
    if (strict) return false;
}
if (!smsigCore.resolveOutputDescriptors(strict)) {   // no `s` passed
    if (strict) return false;
}
```
`SmallSignal<CoreClass,DataMixin>::resolveOutputDescriptors` calls both cores
without forwarding its own `Status& s`, so they run against the default
`Status::ignore`, which discards every `s.set(...)` call. Sibling
`Tran::resolveOutputDescriptors` and `OperatingPoint::resolveOutputDescriptors`
were correctly updated in this branch to pass `s` through — this file is the
one spot the refactor missed. Since `strictsave` defaults to `1`, this fires
on the first bad `.save` target in **any** AC, ACXF, DCXF, DC-incremental, or
Noise analysis: the user sees only the generic "Failed to bind analysis
outputs." instead of the actual reason (e.g. which node/instance/outvar
wasn't found).

### 5. [ ] [core.cpp:508](lib/core.cpp#L508) — Uninitialized member read while formatting a save-argument-count error
```cpp
default:
    s.set(Status::Save, "Save directive requires "+std::to_string(errorExpectedArgCount)+" arguments.");
```
`expectedSaveArgumentsError` receives `expectedArgumentCount` as a parameter
but the `default:` branch reads the class member `errorExpectedArgCount`
instead, which is declared in `core.h` but never assigned anywhere and never
initialized in the constructor — reading it is UB. Reachable via
`AnalysisCore::addInstanceOutvar` (the only caller passing `2`), e.g. any
`.save p(inst)`-style directive with a missing outvar argument in an
OperatingPoint- or Tran-based analysis.

### 6. [ ] [core.cpp:423](lib/core.cpp#L423) — Wrong identifier interpolated into "instance not found" error
```cpp
} else if (strict) {
    s.set(Status::NotFound, "Instance '"+std::string(outvar)+"' not found.");
```
This is the `!inst` branch (instance lookup failed) in
`AnalysisCore::addOutvarOutputSource`, but the message interpolates `outvar`
instead of `instance`. Triggered by `.save p(badinstname, var)` under strict
save — the error reports the output-variable name where the (nonexistent)
instance name should appear, making the message actively misleading.

### 7. [ ] [anac.cpp:31](lib/anac.cpp#L31) (and `anacxf.cpp`, `andcxf.cpp`, `andcinc.cpp`, `annoise.cpp`) — Duplicated error-location suffix in `resolveSave`
The old code returned immediately after delegating to `resolveOpSave`. This
branch removed the early return, so on failure execution falls through to:
```cpp
if (verify && !st) {
    s.extend(save.location());   // location already appended once inside resolveOpSave()
    return false;
}
```
`resolveOpSave` (`ansmsig.h`) already appends `save.location()` itself when
`verify && !st`. Falling through appends it a second time. Reachable via any
op-save (`v()/i()/p()`) resolution failure with `verify=true` in AC, ACXF,
DCXF, DC-incremental, or Noise — functionally harmless (analysis still
aborts) but the reported error message has a duplicated location suffix, in
all five files identically.

## Plausible (needs confirmation in context)

### 8. [ ] [corehbac.cpp:299](lib/corehbac.cpp#L299) — `outspur` changes not detected by `HBACCore::requestsRebuild`, bypassing the "outspur may not change" safety check
`requestsRebuild` only compares `oldParams.maxharm` and `oldParams.maxfreq`;
`params.outspur` is never compared, even though `HBACCore::rebuild` (line
~408) explicitly guards against `outspur` changing across rebuilds ("Output
spurs are not allowed to change"), showing this case was anticipated. If an
HBAC analysis runs multiple sweep points where `outspur` legitimately varies
between points (e.g. it's an expression referencing a swept parameter) while
`maxharm`/`maxfreq` stay fixed, `rebuild()` is never invoked again for later
points — the safety check never runs, and `spurIndices`/output descriptors
silently stay bound to the *first* point's `outspur`, so later points report
results for the wrong spur with no error.

### 9. [ ] [klubsmatrix.cpp:83](lib/klubsmatrix.cpp#L83) — `storageOnly=true` rebuild leaves `AP`/`AI` unresized while inherited base methods assume they're sized `AN+1`
When `storageOnly` is true, `AP.resize(AN+1); AI.resize(nnz_);` is skipped
entirely, but `KluMatrixCore::nnz()` (`return AP[AN];`) and `errorElement()`
(loops `AP[col]`/`AP[col+1]`) are inherited unchanged and not
storageOnly-aware — an out-of-bounds `std::vector::operator[]` read on a
freshly-constructed storage-only block matrix. Currently unreachable: no
caller anywhere in the tree passes `storageOnly=true` yet, so this is latent
capability added for future use — worth guarding before something calls it.

### 10. [ ] [klumatrix.h:201](include/klumatrix.h#L201) — `isBuilt()` can return true after a failed `rebuild()`
```cpp
bool isBuilt() const { return smap!=nullptr; };
```
Both `KluMatrixCore::rebuild()` and `KluBlockSparseMatrixCore::rebuild()` set
`smap = &m;` near the top, before symbolic analysis is attempted. If
`klu_analyze`/`klu_l_analyze` subsequently fails, `smap` is still non-null,
so `isBuilt()` reports true although the matrix isn't actually factorizable.
`corehbnr.cpp:222` gates diagonal-pointer binding on `bsjac.isBuilt()` — low
confidence this is exercised today (a failed `rebuild()` likely aborts
before that check runs), but the contract as written doesn't match
"successfully built."

### 11. [ ] [core.cpp:404](lib/core.cpp#L404) — Asymmetric output-name fallback between sibling `addRealVarOutputSource`/`addComplexVarOutputSource` overloads
Three of the four overloads use `outputSources.emplace_back(asName)` in the
non-strict "not found" path; the fourth (`addComplexVarOutputSource(...,
VectorRepository<Complex>&, ...)`) uses
`outputSources.emplace_back(asName ? asName : name)`. If a caller ever passes
an empty `asName` in non-strict mode to one of the other three, the output
column comes back unnamed instead of falling back to the referenced signal's
name, unlike the fourth variant. No in-scope caller currently triggers this,
flagged for awareness given the inconsistency sits in a shared file.

## Minor / dead code / cosmetic

### 12. [ ] [corehbac.h:139](include/corehbac.h#L139) — `HBACCore::evalOp()` declared but never implemented or called
Declared with a doc comment ("Evaluate (quasi)periodic operating point based
on given nodeset") but has no definition in `corehbac.cpp` and no caller;
the actual nodeset-evaluation path used is `hbCore_.evaluateAtNodeset()`.
Looks like scaffolding left over from an earlier iteration — harmless but
should be removed or implemented.

### 13. [ ] [ansupport.h:252](include/ansupport.h#L252) — `VectorRepository::dataWithoutBucket(DepthIndexDelta, size_t)` member calls a `vector()` overload that doesn't exist
```cpp
T* dataWithoutBucket(DepthIndexDelta which, size_t bucketSize) { return dataWithoutBucket(vector(which, bucketSize)); };
```
`vector()` only has a one-argument overload (`vector(DepthIndex which=0)`);
this member template would fail to instantiate if ever called. No call site
uses it via member syntax anywhere in the tree today (all real call sites use
the free `dataWithoutBucket(Vector<T>&, size_t)`), so it silently compiles as
dead code — but it's a landmine for the next person who reaches for it.

### 14. [ ] [corehbac.cpp:218](lib/corehbac.cpp#L218) — Stale doc comment on `fillDenseBlock`'s spectrum indexing
Comment states `G`/`C` are indexed "only [over the] positive part of the
spectrum," but `jacSpec` is actually sized and populated over the full
negative+DC+positive pruned range (confirmed correct at runtime via the
regression test's negative-spur outputs). Comment is simply out of date, not
indicative of an actual bug — worth fixing so it doesn't mislead future
maintainers.

### 15. [ ] [corehbnr.cpp:492](lib/corehbnr.cpp#L492) — Stale "rows" wording in comment for a column-scaling operation
Comment says "Scale rows of Gamma/Omega Gamma..." but the code calls
`scaleColumns`/`scaleColumnsAdd`. The code itself is correct (diagonal-matrix
right-multiplication scales columns); only the comment text is wrong.

---

## Audited and found clean

No functional issues were found in: `spurs.h`/`spurs.cpp`'s spur/mixing-map
bookkeeping (`buildSmsig`, `prune`, `buildMixingMap`) — verified both by
manual trace and by running the existing `test_hbac1.sim` regression plus
ad-hoc multi-tone/asymmetric-`nharm`/`maxfreq`-pruning scripts against a
Debug build; `corehbxform.cpp`'s APFT/IAPFT/`tstart` phase-reduction math;
`corehbcoloc.cpp`; `anhb.cpp`/`anhb.h`; the `freqgrid.h/.cpp` deletion
(no dangling references anywhere); the KLU "arrays to vectors" conversion and
the block-sparse double-destructor crash fix in `klumatrix.cpp`/
`klubsmatrix.cpp`; `densematrix.h`'s new row/column/scale operations;
`nrsolver.h`/`.cpp`'s generalized bucket-size handling; all of `coreac*.cpp`,
`coredc*.cpp`, `coreop*.cpp`, `coretran*.cpp`, `corenoise.cpp`, `devbase.cpp`,
`devvisrc.cpp` (including the new FMA-based phase computation), and
`value.cpp`; the lexer's brace-counting addition, the `dflparser.y` grammar
changes (`{...}` list syntax replacing `[...]`, confirmed conflict-free via
`bison -Wcounterexamples`), the RPN `MergeList`->`listFlatten` migration, and
all touched `CMakeLists.txt`/test/demo files.
