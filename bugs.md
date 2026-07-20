# Bug audit — `feature/mc` branch

Scope: everything added since the branch diverged from `main`
(merge base `8c22dfe1f7b6b6daa3af1e2c223d8343c4a7ca01`, i.e. the Monte Carlo
analysis, RPN evaluation-context threading, and control-block loop support).
79 files changed, ~2076 insertions.

Reviewed via full manual read of the RPN/MC core (`rpnevalctx.*`, `rpnfunctor.h`,
`rpnbuiltin.h`/`.cpp`, `rpneval.cpp`, `parameterized.cpp`, `cmd.cpp`/`cmd.h`
— every line of all of these was read directly, not just diff hunks) plus
targeted parallel review of the parameter/context propagation path
(`rpneval.*`, `context.cpp`, `cirparams.cpp`) and the analysis-core/device/
parser/ASA241 integration (`an.cpp`, `hierdevice.cpp`, `asa241.*`,
`dflparser.y`, `devbase.cpp`, `osdidevice.cpp`, `osdimodel.cpp`,
`test/test_mc.sim`).
The uninitialized-pointer bug and the `listMerge` bug below were both
reproduced live against the current Debug build
(`build.VACASK/Debug/simulator/vacask`).

## High severity (correctness)

- [x] **1. Uninitialized-pointer read in the `unif`/`aunif` Monte Carlo generator. — FIXED**
  `include/rpnbuiltin.h:519-562`, template `mcGenerator<F, narg>`.
  `Value* v3p;` is declared unconditionally but only *assigned* inside
  `if constexpr(narg>2)` (lines 520-523). The line
  `auto sigma = v3p->val<Real>();` (line 562) is **not** inside that
  `if constexpr` guard, so for `narg==2` — the instantiations bound to the
  `unif` and `aunif` builtins (`lib/context.cpp:115-116`,
  `mcGenerator<FwUnif,2>` / `mcGenerator<FwAunif,2>`) — it dereferences a
  never-initialized `Value*`. This is undefined behavior on every call to
  `unif(nom, rvar)` or `aunif(nom, avar)` anywhere in a netlist (e.g.
  `test/test_mc.sim` lines 11-12, 17, 35; `demo/mc/miller_ihp.sim` uses only
  `gauss`, so it doesn't trigger this, but `test_mc.sim` does).
  Manually reproduced with a minimal netlist against the current Debug
  binary (`build.VACASK/Debug/simulator/vacask`): it did not crash, most
  likely because `sigma`'s value is unused for `narg==2` and either the
  compiler elides the read or the garbage stack slot happens to hold a
  leftover valid `Value*`. That is incidental, not a guarantee — a different
  compiler, optimization level, or call ordering (e.g. `unif`/`aunif` called
  before any `gauss`/`agauss` call ever primes that stack slot) can crash.
  Fix: move the `sigma` computation inside `if constexpr(narg>2)`, alongside
  the other `v3p`-dependent checks a few lines above.

- [x] **2. `analysisNamePrefix_` was never reset after an `mc` loop ends, so later analyses silently inherited the last MC sample's file-name prefix. — FIXED**
  `simulator/cmd.cpp`, function `cmd_mc` (lines 816-1037). Inside the sample
  loop, line 954 sets
  `interpreter.setAnalysisNamePrefix(mcName+"."+std::to_string(atSam+1)+".")`
  every iteration. `CommandInterpreter::run()` unconditionally applies this
  prefix to every analysis it runs (`cmd.cpp:139`,
  `an->setFileNamePrefix(analysisNamePrefix_)`), which becomes the actual
  output-file base name (`Analysis::prefixedName_`, `lib/an.cpp:14-16`).
  `mcData` is correctly cleared on every exit path (lines 1021, 1026, 1035),
  but `analysisNamePrefix_` (`simulator/cmd.h:99,104`) is not reset on any
  of them.
  Failure scenario: a control block runs `mc mc1 samples=10 ... endmc`
  followed by a plain `analysis tran1 tran ...` outside the loop. The
  post-loop analysis silently writes to `mc1.10.tran1.raw` instead of
  `tran1.raw`, clobbering the 10th MC sample's output file instead of
  producing its own.

- [x] **3. `listMerge` (the `[a: b: c]` list-merge/flatten operator) duplicated every element it appended, corrupting the result. — FIXED**
  `lib/rpnbuiltin.cpp:533-568`, function `listMerge`. The loop body does:
  ```cpp
  if (vp->type()==Value::Type::ValueVec) {
      for(auto &it : vp->val<ValueVector>()) {
          vVec.push_back(std::move(it));   // flatten sub-list elements in
      }
  } else {
      vVec.push_back(std::move(*vp));      // move plain argument in
  }

  vVec.push_back(std::move(*vp));          // <-- unconditional, runs every iteration regardless of branch
  ```
  The last line executes unconditionally after the `if`/`else`, so every
  argument is appended twice: once correctly by the branch, and once more by
  the trailing statement, which moves an already-(partially-)moved-from `*vp`
  (harmless-looking for scalar `Int`/`Real` `Value`s, since their "moved-from"
  state is just a copy of the original bits — this project's `Value` doesn't
  clear scalar payloads on move — but not harmless for `ValueVec` arguments,
  whose *elements* were already moved out individually while the outer
  `ValueVec` container itself was not, so the second `push_back` re-inserts
  the entire original sub-list unflattened). Reproduced directly against the
  Debug binary:
  ```
  print ([1: 2: 3])          ->  [1, 1, 2, 2, 3, 3]        (expected [1, 2, 3])
  print ([[1;2]: [3;4]])     ->  [1, 2, [1, 2], 3, 4, [3, 4]]  (expected [1, 2, 3, 4])
  ```
  This is the operator documented at `docs/expr-overview.md:25` and
  `docs/expr-vectors.md:75-86` (e.g. `parameters ic = [ic_stage1: ic_stage2]`),
  so it's directly user-reachable and silently produces wrong values rather
  than erroring. **Note on scope:** the function body is unchanged by this
  branch's diff — only its signature was touched, to add the new
  `RpnEvaluationNetlistContext& ctx` parameter threaded through for MC
  support (`git diff` on `lib/rpnbuiltin.cpp` shows only signature-line
  changes). The bug predates this branch, but is included here since the
  review was extended to this file and it's a serious, easily-fixed,
  independently-confirmed defect (delete the trailing `vVec.push_back(std::move(*vp));`
  at line 554).

## Medium severity

- [x] **4. `MCData::dump()` indexed `value`/`lhPerm` with the wrong variable, so debug dumps could show the wrong bin/value for a generator. — FIXED**
  `lib/rpnevalctx.cpp:85-99`. The loop iterates
  `generatorIndexMap` (a `std::unordered_map`) and computes the correct
  per-generator index as `auto ndx = it.second;` (line 89), but then indexes
  `lhPerm[cnt][sample_]` (line 93) and `value[cnt]` (line 95) using `cnt`, a
  plain loop counter over the unordered_map's *iteration* order — which is
  not guaranteed to match insertion order (the order that determined each
  generator's real index into `value`/`lhPerm`). `ndx` is computed and then
  never used. Whenever hash iteration order diverges from insertion order
  (increasingly likely as more distinct `gauss`/`unif`/... call sites are
  registered), `mc ... debug=1` (or higher) output attributes the wrong
  uniform draw / LH bin to a generator id. Debug-only impact (only reached
  when `debug>0` is passed to `mc`, e.g. `test/test_mc.sim:41`), so it
  doesn't affect simulation results, only the diagnostic dump. Fix: index
  with `ndx` instead of `cnt`.

- [x] **5. MC loop progress bar never reached 100%. — FIXED**
  `simulator/cmd.cpp:934`: `progress.initProgress(nsamples+1, 0);`. The
  reporter computes `pos_/extent_` (`simulator/progressbar.cpp:25,80`), and
  `pos_` only ever reaches `nsamples` (`progress.setProgress(atSam+1,
  atSam+1)` at `cmd.cpp:994`, with `atSam` maxing at `nsamples-1`), while
  `extent_` is `nsamples+1`. Every other progress-tracked loop in the
  codebase passes the real point count as extent (e.g.
  `lib/answeep.cpp:295`, `lib/coreac.cpp:264`, `lib/coretran.cpp:799`), so
  this is an off-by-one: even the final forced report
  (`progress.report(true)` at `cmd.cpp:1012`) shows `nsamples/(nsamples+1)`
  (e.g. 99% for 100 samples). Fix: `progress.initProgress(nsamples, 0);`.

## Low severity

- [x] **6. `isUniqueMc` registers the loop name before argument validation succeeds. — NOT A BUG, by design.**
  `simulator/cmd.cpp:824-840`, `simulator/cmd.h:91-98`. Per the author: the
  mere appearance of an `mc name ...` loop claims that name, even if the loop
  never actually runs (e.g. it fails argument validation) — the same way an
  `analysis` name is claimed just by appearing, not by successfully
  completing. So `isUniqueMc` registering the name unconditionally, before
  `evaluateArgs()` is checked, is intentional rather than an oversight.

- [x] **7. Stale forward declaration of `evaluateExpressions` in `cmd.h`. — FIXED**
  The mismatched 4-arg declaration was removed from `simulator/cmd.h`;
  `evaluateExpressions` is now only declared/defined once, in
  `simulator/cmd.cpp:279`, with the correct 5-arg (`ctx`-included) signature
  matching every call site.

## Cleanliness / cosmetic (not functional bugs)

- [x] **8. Leftover debug `print()` in the PDK converter. — FIXED**
  `python/sg13g2tovc.py`: the stray `print(tech_file)` is gone; only the
  original `print(" ", file)` remains.

- [x] **9. `lib/asa241.cpp` re-includes `asa241.h` from inside `namespace NAMESPACE`. — FIXED**
  `#include "asa241.h"` now sits above `namespace NAMESPACE {`, matching
  every other file in the tree; no more doubly-nested `NAMESPACE::NAMESPACE`
  scope.

- [x] **10. `lib/libplatform.cpp` had unused `#include <cmath>` / `#include <limits>`. — FIXED**
  Both unused includes are gone; only `<tuple>` and `<cstdlib>` remain.

- [x] **11. `test/test_mc.sim:41` left `debug=1` enabled. — FIXED**
  Now `debug=0`, so the 1000-sample ctest no longer prints a diagnostic line
  per sample or disables the progress bar.

- [x] **12. TODO comment in `include/an.h:17`. — NOT A BUG, intentional.**
  `// TODO: name_ -> prefixedName_ in output init/finalize/delete in all analyses in other branches`.
  The migration is complete in *this* branch; the comment is deliberately
  left as a reminder for when the `hbac` and `pss` branches are merged, since
  those branches' analysis cores still need the same `name_` ->
  `prefixedName_` migration applied.

## Extended-scope pass

`simulator/cmd.cpp`, `include/rpnfunctor.h`, `lib/rpnbuiltin.cpp`,
`lib/rpnevalctx.cpp`, and `lib/parameterized.cpp` were each read in full
(not just diff hunks). All findings from these files are already folded into
the sections above (the `unif`/`aunif` bug and `listMerge` bug are in
`rpnbuiltin.h`/`lib/rpnbuiltin.cpp`; the `MCData::dump()` bug is in
`lib/rpnevalctx.cpp`; the prefix/progress-bar/`isUniqueMc`/stale-declaration
bugs are in `simulator/cmd.cpp`/`cmd.h`). No further bugs were found in
`include/rpnfunctor.h` (the `FwGauss`/`FwAgauss`/`FwUnif`/`FwAunif`/etc.
functors) or `lib/parameterized.cpp` (the `setParameters` overloads correctly
reset `ctx`'s parameter id per parameter and are order-independent with
respect to `PTParameterMap`'s hash-map iteration, since generator identity is
keyed by parameter name rather than iteration position).
