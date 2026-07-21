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

### 1. [x] [corehbnr.cpp:132](lib/corehbnr.cpp#L132) — Out-of-bounds read in `HBNRSolver::setForces` when a stored nodeset comes from a smaller circuit
**Status: FIXED.** As of the current code, `setForces` no longer loops up to
the current circuit's `n`. It deduces `solNodes` from the stored solution's
own data (`solSpec.size()/nfSolution`), requires
`solNames.size()-1==solNodes` before trusting name-based lookup, and bounds
the loop by `solNodes` instead of `n` (`corehbnr.cpp:138,153`):
```cpp
auto solNodes = solSpec.size()/nfSolution;
...
if (solNames.size()-1==solNodes) {
    checkNames = true;
} else if (solNames.size()==0 && solNodes==n) {
    checkNames = false;
} else {
    lastHBNRError = HBNRSolverError::ForcesError;
    return false;
}
for(decltype(n) i=1; i<=solNodes; i++) {
    if (checkNames) {
        node = circuit.findNode(solNames[i]);   // i<=solNodes, always in bounds
```
`solNames` is guaranteed to have `solNodes+1` entries whenever `checkNames`
is true, so `solNames[i]` for `i` in `1..solNodes` is always in bounds. The
original out-of-bounds read is gone. See finding #1b below for a related,
still-open bug found in the same function during this re-audit.

### 1b. [x] [corehbnr.cpp:171](lib/corehbnr.cpp#L171) — Unsigned underflow / massive out-of-bounds write in `HBNRSolver::setForces` when a stored node name now resolves to the current circuit's ground node
**Status: FIXED** (uncommitted working-tree change). The current code now
does:
```cpp
auto ui = node->unknownIndex();
// Ground node, nothing to do
if (ui==0) {
    continue;
}
```
immediately after resolving `ui` and before any `ui-1` arithmetic, exactly
mirroring `OpNRSolver::setForceOnUnknown`'s guard. Verified this fully closes
the underflow: in the `checkNames` branch a stored name that now resolves to
the current circuit's ground node is skipped before `destOrigin` is
computed; in the non-`checkNames` branch (`node = circuit.reprNode(i)` for
`i` in `1..solNodes==n`) `ui` was already always non-zero, so the added check
is a harmless no-op there. No remaining issue at this line.
```cpp
node = circuit.findNode(solNames[i]);   // i>=1, i.e. a *non-ground* name in the storing circuit
...
if (!node) { continue; }
auto ui = node->unknownIndex();          // UnknownIndex = uint32_t; ground node's index is always 0
auto destOrigin = (ui-1)*blockSize;       // ui==0  ->  underflows to (UINT32_MAX)*blockSize
...
f.unknownValue_[destOrigin] = solSpec[srcOrigin].real();   // catastrophic OOB write
f.unknownForced_[destOrigin] = true;
```
There is no check that `node` isn't the ground node before computing
`ui-1`. `solNames[i]` for `i>=1` is guaranteed to be a *non-ground* name in
the **storing** circuit (`AnnotatedSolution::setNames` puts the storing
circuit's ground name only at index 0), but `names_` exists precisely "for
cross matching across slightly different circuits" (`ansolution.h:51`) —
i.e. the current circuit is allowed to differ from the one that produced the
stored solution. If the current circuit's netlist grounds a node that was
*not* ground when the solution was stored (e.g. a different `.global`/ground
declaration, or a topology tweak between runs), `circuit.findNode(solNames[i])`
resolves to the current circuit's ground node, whose `unknownIndex()` is
always `0` (`circuit.cpp:542`, `Circuit::addGround`). `ui` is `UnknownIndex`
(`uint32_t`), so `ui-1` wraps to `UINT32_MAX` instead of going negative, and
`destOrigin = (ui-1)*blockSize` becomes a huge value used directly as a
`std::vector::operator[]` index into `f.unknownValue_`/`f.unknownForced_` —
undefined behavior (heap corruption or crash), with no bounds checking since
`operator[]` never checks and `DBGCHECK` is a release-mode no-op anyway.

This exact hazard is already known and guarded against elsewhere in the
codebase: `OpNRSolver::setForceOnUnknown` (`coreopnr.cpp:374-381`) explicitly
checks `if (u==0) { return true; }` ("Is it a ground node? If yes, ignore the
force.") before using a looked-up node's `unknownIndex()` as an array index.
`HBNRSolver::setForces` is missing the equivalent guard. Note this bug
**predates this branch** — the same unguarded `(ui-1)*blockSize` pattern
already existed in `main`'s version of this function — so it is not a
regression introduced by `analysis/hbac`, but it is still live in the current
code and reachable through the same `nodeset=` mechanism this branch relies
on for HBAC. Fix: skip the unknown (`continue`) when `ui==0`, mirroring
`OpNRSolver::setForceOnUnknown`.

### 1c. [x] [corehbnr.cpp:66](lib/corehbnr.cpp#L66) — `abortOnError` parameter of `setForces` is unused
**Status: FIXED** (uncommitted working-tree change). `abortOnError` is now
read in the per-unknown "node not found" path:
```cpp
if (!node) {
    // Node not found. No forces will be applied to this unknown. 
    // If abortOnError is set, abort 
    if (abortOnError) {
        lastHBNRError = HBNRSolverError::ForcesError;
        return false;
    }
    // Otherwise continue to next force
    continue;
}
```
This is a real (not just cosmetic) behavioral improvement: previously a
missing individual node in a `nodeset=` solution was always silently
skipped, and `strictforce` in the caller (`corehb.cpp:597-603`) only ever saw
an overall `false` from the structural ("no names nor matching length")
check — so `strictforce=1` never actually caught a missing named node.
Now it does, matching the option's intended meaning. The structural-mismatch
path (`corehbnr.cpp:144-149`) is correctly left unconditional ("Abort always
regardless of `abortOnError`") since that's a harder, unrecoverable
incompatibility, not a single missing node — and the caller's own
`if (strictforce)` gate still decides whether that unconditional `false`
escalates to aborting the whole analysis or is silently treated as "no
nodeset applied," so behavior for `strictforce=0` is unchanged and safe.
Verified `lastHBNRError` is reset via `clearError()` in `initialize()`, so no
stale error state leaks between calls. No remaining issue.

### 2. [x] [corehb.cpp:631](lib/corehb.cpp#L631) — Warm-start "ordinary continue" path in `HBCore::runSolver`: size check fixed, but the branch is now reachable and WILL crash — **now fully fixed**
```cpp
if (continueState &&
    continueState->valid && continueState->coherent &&
    continueState->solution.cxValues().size()==circuit.unknownCount()*spurs_.spectrum().size() &&  // fixed: was *timepoints.size()
    continueState->solution.hbSpurs().spectrum().size()==spurs_.spectrum().size()
) {
    ...
    solution.vector() = continueState->solution.values();   // STILL WRONG
```
**Status: only the size check was fixed** (uncommitted working-tree change:
`timepoints.size()` → `spurs_.spectrum().size()`, i.e. comparing `n*nf`
against `n*nf` on both sides, consistent now). This actually makes things
**more dangerous than before**: originally `nt = 2*nf-1` (`corehbcoloc.cpp:30`)
guaranteed `nt != nf`, so the whole condition was permanently `false` and this
branch was dead code — the crash below could never trigger. Now that the size
check is internally consistent, the branch **is reachable** whenever a
coherent, matching-frequency continuation happens (a very normal case, e.g.
back-to-back HB points in a sweep with unchanged tone/harmonic settings).

`continueState->solution.values()` still calls
`std::get<std::vector<double>>(values_)`, but HB's stored `AnnotatedSolution`
only ever has `setCxValues()` called on it (`corehb.cpp:160-161,189-192`) —
`values_` holds a `std::vector<Complex>`, never a `std::vector<double>`. This
`std::get` throws `std::bad_variant_access`, i.e. **this will now crash the
next time HB hits an ordinary (coherent, same-grid) continuation** — a
regression risk introduced by fixing only half of this finding.

The correct fix must replace this line with the same real/imaginary
unpacking `HBNRSolver::postRun` uses to go the other way
(`corehbnr.cpp:326-347`, unpacking `n*nf` complex coefficients into an `n*nt`
real vector one unknown at a time — DC as a single real value, then real/imag
pairs), applied to `continueState->solution.cxValues()` to rebuild
`solution.vector()`. A plain vector assignment cannot work here since the two
sides have different lengths (`n*nf` complex vs. `n*nt` real) and different
representations.

**Status: FIXED** (uncommitted working-tree change). The working tree now
adds exactly this unpacking loop in place (`corehb.cpp:639-654`):
```cpp
auto& data = continueState->solution.cxValues();
auto& dest = solution.vector();
auto n = circuit.unknownCount();
auto nf = spurs_.spectrum().size();
auto nt = timepoints.size();
for(decltype(n) i=0; i<n; i++) {
    auto srcOrigin = i*nf;
    auto destOrigin = i*nt;
    dest[destOrigin] = data[srcOrigin].real();
    for(decltype(nf) k=1; k<nf; k++) {
        auto base = destOrigin + 1 + (k-1)*2;
        dest[base] = data[srcOrigin+k].real();
        dest[base+1] = data[srcOrigin+k].imag();
    }
}
```
Verified: this is the exact inverse of `postRun`'s TD→FD packing, `dest` is a
reference to `solution.vector()`'s storage (already sized `n*nt` by the
earlier `nrSolver.rebuild(n*nt)` call, so all writes are in-bounds), and the
last index touched (`base+1` at `k=nf-1`) equals `destOrigin+nt-1`, exactly
filling each unknown's block with no gap or overflow — matches `nt=2*nf-1`.
The old crash-triggering `solution.vector() = continueState->solution.values();`
line that originally followed this loop (and would have unconditionally
overwritten `dest` with a throwing call) has been removed. No remaining issue
at this line.

### 3. [x] [corehbnr.cpp:728](lib/corehbnr.cpp#L728) — Wrong DC value printed for every unknown but the first in `HBNRSolver::dumpSolution`
**Status: FIXED** (uncommitted working-tree change). The `k==0` branch now
reads `solution[base]` with `base=(i-1)*nt` (computed once and reused for the
`k>0` branch too), instead of the hardcoded `solution[0]`, so each unknown's
own DC value is printed. No remaining issue.

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

### 4. [x] [ansmsig.h:157](include/ansmsig.h#L157) — `Status` dropped for every small-signal analysis's output-descriptor resolution
**Status: FIXED** (uncommitted working-tree change). Both calls now pass `s`
through: `opCore.resolveOutputDescriptors(strict, s)` and
`smsigCore.resolveOutputDescriptors(strict, s)`. Confirmed every concrete
`smsigCore` type used here (`coreac`, `coreacsp`, `coreacstb`, `coreacxf`,
`coredcinc`, `coredcxf`, `corenoise`) declares the matching two-argument
`resolveOutputDescriptors(bool strict, Status& s=Status::ignore)` overload, so
error messages now correctly propagate to the caller instead of being
swallowed by the default `Status::ignore`. No remaining issue.

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

### 5. [x] [core.cpp:508](lib/core.cpp#L508) — Uninitialized member read while formatting a save-argument-count error
**Status: FIXED** (uncommitted working-tree change). The `default:` branch
now uses the actual `expectedArgumentCount` parameter instead of the member;
the unused, never-initialized `errorExpectedArgCount` member was removed from
`core.h` entirely, and no other reference to it remains in the tree. No
remaining issue.

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

### 6. [x] [core.cpp:423](lib/core.cpp#L423) — Wrong identifier interpolated into "instance not found" error
```cpp
} else if (strict) {
    s.set(Status::NotFound, "Instance '"+std::string(outvar)+"' not found.");
```
This is the `!inst` branch (instance lookup failed) in
`AnalysisCore::addOutvarOutputSource`, but the message interpolates `outvar`
instead of `instance`. Triggered by `.save p(badinstname, var)` under strict
save — the error reports the output-variable name where the (nonexistent)
instance name should appear, making the message actively misleading.

**Status: FIXED** (uncommitted working-tree change, after an intermediate
edit briefly regressed the sibling branch — now both are correct):
```cpp
} else if (strict) {  // found==false: instance exists, outvar doesn't
    s.set(Status::NotFound, "Output variable '"+std::string(outvar)+"' of instance '"+std::string(instance)+"' not found.");
    return false;
}
...
} else if (strict) {  // !inst: instance lookup failed
    s.set(Status::NotFound, "Instance '"+std::string(instance)+"' not found.");
    return false;
}
```
The `found==false` branch correctly reports the missing output variable
(using both `outvar` and `instance`), and the `!inst` branch now correctly
reports the missing instance using `instance` instead of `outvar`. No
remaining issue.

### 7. [x] [anac.cpp:31](lib/anac.cpp#L31) (and `anacxf.cpp`, `andcxf.cpp`, `andcinc.cpp`, `annoise.cpp`) — Duplicated error-location suffix in `resolveSave`
**Status: FIXED** (uncommitted working-tree change, all five files). Each
file now introduces an `addLoc` flag (defaults `true`, set `false` only on
the `resolveOpSave` path, with a comment noting `resolveOpSave()` already
adds the location) and guards the fallthrough with it:
```cpp
bool addLoc = true;
...
} else {
    std::tie(st, handled) = resolveOpSave(save, verify, s1);
    // resolveOpSave() adds location to error
    addLoc = false;
    ...
}
if (verify && !st) {
    if (addLoc) {
        s.extend(save.location());
    }
    return false;
}
```
Verified identically applied in `anac.cpp`, `anacxf.cpp`, `andcxf.cpp`,
`andcinc.cpp`, and `annoise.cpp`: the `smsigCore.addXXX` branches (unique per
file — `addAllUnknowns`/`addAllNodes`/`addNode`/`addFlow` for AC/DCInc,
`addAllTfZin`/... for ACXF/DCXF, `addAllNoiseContribInst`/... for Noise)
still get the location appended exactly once as before, and only the
`resolveOpSave` branch now skips the redundant second append. No remaining
issue.
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
