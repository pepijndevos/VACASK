# Bug List

## rpneval.cpp

- [x] **1. Wrong arity reported in minArity error message** ([lib/rpneval.cpp:151](lib/rpneval.cpp#L151))
When a function is called with too few arguments the error message prints `builtinPtr->maxArity` instead of `builtinPtr->minArity`, so the user sees the maximum rather than the minimum required count.

- [x] **2. Typo in minArity error message** ([lib/rpneval.cpp:151](lib/rpneval.cpp#L151))
"at lest" should be "at least".

- [x] **3. Extra closing parenthesis in maxArity error message** ([lib/rpneval.cpp:143](lib/rpneval.cpp#L143))
The string ends with `"argument(s))."` -- there is one surplus `)` before the period.

- [x] **4. Typo "insted" in stack-depth error message** ([lib/rpneval.cpp:264](lib/rpneval.cpp#L264))
"insted" should be "instead".

- [x] **5. Redundant recomputation of branch condition** ([lib/rpneval.cpp:224-225](lib/rpneval.cpp#L224))
`auto branch = cond^negate;` is computed on line 224, but the `if` on line 225 recomputes `cond^negate` rather than testing `branch`. The result is the same, but `branch` is only used on line 231.

---

## rpnbuiltin.cpp

- [x] **6. Missing `return false` after vectorPack preprocessing error** ([lib/rpnbuiltin.cpp:281-285](lib/rpnbuiltin.cpp#L281))
In `vectorPack`, when `vectorPackPreprocess` returns false the error message is extended but there is no `return false`. The loop continues as if nothing went wrong, and then the code tries to build a vector from potentially wrong type/count information.

- [x] **7. Unconditional extra push in listMerge** *(fixed in `analysis/hbac` branch)* ([lib/rpnbuiltin.cpp:368](lib/rpnbuiltin.cpp#L368))
`vVec.push_back(std::move(*vp))` at line 368 is outside the `if/else` block and executes on every iteration regardless of which branch was taken. In the `else` branch `*vp` was already moved on line 365, so a moved-from value is pushed again. In the `if` branch an empty `ValueVec` shell is pushed after its contents were already moved item by item.

---

## rpnexpr.cpp

- [x] **8. Wrong variant type accessed in TPackList case** ([lib/rpnexpr.cpp:179](lib/rpnexpr.cpp#L179))
`auto n = e->get<PackVec>().arity;` is inside the `TPackList` handler and should read `e->get<PackList>().arity`. Accessing the wrong variant member is undefined behavior if the active alternative is different.

---

## value.cpp

- [x] **9. Dead comparison against unsigned zero** ([lib/value.cpp:194](lib/value.cpp#L194))
In `Value::getScalar`, `ndx` has type `size_t` (unsigned). The condition `ndx<0` is always false and the corresponding check is dead code; out-of-range negative indices passed as large unsigned values are not caught by this branch.

- [x] **10. ValueVec items printed with surrounding quotes** ([lib/value.cpp:262](lib/value.cpp#L262))
In `operator<<`, the `Value::Type::ValueVec` branch writes `"\""` before and after each element (`os << "\"" << *it << "\""`). `ValueVec` contains `Value` objects, not strings; the surrounding quotes are wrong and produce malformed output.

---

## devbase.cpp

- [x] **11. Assignment instead of comparison in removeAncestor** ([lib/devbase.cpp:52](lib/devbase.cpp#L52))
`if (numRemoved=0)` assigns zero to `numRemoved` and then tests the result, which is always zero (false). The early-return branch is therefore never taken and `removeAncestor` always returns `true` even when nothing was erased.

- [x] **12. Typo "Oputput" in getOutvar error message** ([lib/devbase.cpp:156](lib/devbase.cpp#L156))
"Oputput" should be "Output".

---

## parser.cpp

- [x] **13. Missing `throw` in parseExpression (lvalue overload)** ([lib/parser.cpp:116](lib/parser.cpp#L116))
`std::runtime_error(...)` is constructed but not thrown. On parse failure the error is logged but execution continues and the function silently returns a default-constructed (empty) `Rpn`. The `throw` overload on line 143 is correct.

- [x] **14. Missing `throw` in parseParameters (lvalue overload)** ([lib/parser.cpp:182](lib/parser.cpp#L182))
Same issue: `std::runtime_error(...)` is not thrown. The function silently returns a default-constructed `PTParameters` on failure.

- [x] **15. Wrong error text in parseParameters (rvalue overload)** ([lib/parser.cpp:207](lib/parser.cpp#L207))
The message says `"Failed to parse expression '"` but should say `"Failed to parse parameters '"`.

---

## hierdevice.cpp

- [x] **16. Duplicate parameter map insertions not detected** ([lib/hierdevice.cpp:85](lib/hierdevice.cpp#L85))
In `buildParameterMap`, `parameterMap.insert({id, i})` is called but the returned `inserted` boolean is never checked. Duplicate parameter names are silently ignored, unlike `buildTerminalMap` which checks and reports them.

- [x] **17. Structured binding shadows outer `ok` in recomputeBlockConditionsWorker** ([lib/hierdevice.cpp:503](lib/hierdevice.cpp#L503))
`auto [ok, subCond] = recomputeBlockConditionsWorker(...)` introduces a new local `ok` that hides the outer `bool ok = true` declared at line 496. The outer `ok` is never updated, so error results from recursive calls are not propagated; the function returns `std::make_tuple(ok, cond)` where `ok` is always `true`.

- [x] **18. Same shadowing bug in recomputeBlockConditions** ([lib/hierdevice.cpp:531](lib/hierdevice.cpp#L531))
`auto [ok, subCond] = recomputeBlockConditionsWorker(...)` shadows the outer `bool ok = true` on line 523. The function always returns `true`.

---

## answeep.cpp

- [x] **19. Self-assignment of `factor` in setupLogSweep** ([lib/answeep.cpp:123](lib/answeep.cpp#L123))
The parameter `factor` has the same name as the member variable. `factor = factor` assigns the parameter to itself; the member `ScalarSweep::factor` is never updated to the caller-supplied value.

- [x] **20. parameterIsFree called with null name** ([lib/answeep.cpp:423](lib/answeep.cpp#L423))
When no parameter name is given (the principal parameter branch was taken, `!it->parameter` was true), `instPtr->parameterIsFree(it->parameter)` is still called with the null `it->parameter`. The parameter name used for the freedom check does not correspond to the parameter that will actually be swept.

---

## osdicallback.cpp

- [x] **21. Unconditional free of a non-heap pointer** ([lib/osdicallback.cpp:50-55](lib/osdicallback.cpp#L50))
The comment inside the `LOG_FMT_ERR` branch states "Not allowed to free msg because it is a constant string", but `free(msg)` on line 55 is outside the `if/else` and executes unconditionally. In the format-error case `msg` points to a string literal; freeing it is undefined behavior.

---

## context.cpp

- [x] **22. dump() prints every context for every level** ([lib/context.cpp:357-366](lib/context.cpp#L357))
In `ContextStack::dump`, the outer loop iterates indices `i` but the inner loop iterates over the entire `stack` vector on each pass instead of only the entry at index `i`. Every context entry is printed once for each context level, giving duplicate output.

---

## coreop.cpp

- [x] **23. Bitwise OR instead of logical OR** ([lib/coreop.cpp:185](lib/coreop.cpp#L185))
`if (!node1 | !node2)` uses bitwise OR. The intent is clearly a logical OR `||`. While both yield the same boolean result for `bool`-convertible pointers, short-circuit evaluation is lost.

---

## corehb.cpp

- [x] **24. Wrong variable in convergence error check** ([lib/corehb.cpp:454](lib/corehb.cpp#L454))
`runSolver` calls `nrSolver.run()` and stores the result in a local `converged`. The very next line checks `if (!converged_ || abort)` using the member variable `converged_` instead. Because `converged_` is always `false` at this point (set to `false` in `coroutine()` before `runSolver` is called), the condition is always true and `setError(HBError::SolverError)` fires on every invocation -- including successful ones. The check should use the local `converged`.

---

## devctlsrc.cpp

- [x] **25. Reactive residual values loaded into resistive array** ([lib/devctlsrc.cpp:1089-1091](lib/devctlsrc.cpp#L1089))
The guard condition checks `loadSetup.reactiveResidual` (reactive), but the body writes to `loadSetup.resistiveResidual[d.uFlow1]` and `loadSetup.resistiveResidual[d.uFlow2]`. The reactive residual contributions are silently dropped and the resistive array receives garbage additions. Both lines should write to `loadSetup.reactiveResidual`.

---

## densematrix.cpp

- [x] **26. Loop variable initialized to itself (UB)** ([lib/densematrix.cpp:79](lib/densematrix.cpp#L79))
`for(size_t i=i; i<n; i++)` declares `i` and initializes it to itself -- an indeterminate value. Reading an uninitialized object is undefined behavior. The same mistake occurs on line 93. Both loops should start with `i=0`.

---

## libplatform.cpp

- [x] **27. `getenv` return not checked for null** ([lib/libplatform.cpp:53](lib/libplatform.cpp#L53))
`std::getenv("PATH")` returns `nullptr` when the environment variable is absent. The result is passed directly to `splitString` as a `const std::string&` argument, which constructs a `std::string` from a null pointer -- undefined behavior that typically crashes. A null check is needed before use.

---

## nrsolver.cpp

- [x] **28. Outer `residualOk` declared but never used** ([lib/nrsolver.cpp:107](lib/nrsolver.cpp#L107))
`bool residualOk;` is declared at function scope but is immediately shadowed by `bool residualOk = true;` inside the do-while loop body (line 209). The outer declaration is dead code and is uninitialized; only the inner one participates in the convergence logic.

---

## answeep.cpp

- [x] **29. Typo "suppported" in user-visible error message** ([lib/answeep.cpp:247](lib/answeep.cpp#L247))
`"vector component sweeps are not suppported yet."` has three consecutive `p`s. Should be "supported".

---

## include/densematrix.h

- [x] **30. Non-const `apply` takes unused `result` parameter** ([include/densematrix.h:416](include/densematrix.h#L416))
The non-const overload `void apply(T (*func)(T), DenseMatrixView<T>& result)` accepts a `result` matrix it never writes to; the body calls `row(i).apply(func)` which modifies the matrix in place and ignores `result`. The parameter is a silent no-op. The const version on line 408 uses `result` correctly; the non-const version appears to be a defective copy of it.

---

## include/rpnfunctor.h

- [x] **31. `FwMaxAggregate` initialises result to 1 instead of first element** ([include/rpnfunctor.h:475](include/rpnfunctor.h#L475))
`Value::ScalarType<Tin> res = 1;` seeds the running maximum with the literal `1`. For any vector whose maximum value is less than `1` (e.g. all negative numbers, or all fractions) the function returns `1` instead of the true maximum. The symmetric `FwMinAggregate` correctly seeds with `x[0]`; `FwMaxAggregate` should do the same.

- [x] **32. `FwMinAggregate::ok` reads `x[0]` before empty check** ([include/rpnfunctor.h:461](include/rpnfunctor.h#L461))
`Value::ScalarType<Tin> res = x[0];` is executed before `if (x.size()==0)`. On an empty vector this is an out-of-bounds access. The size check must come first, or the `x[0]` line should be removed (it has no effect in `ok`, which only validates the argument).

---

## include/rpnbuiltin.h

- [x] **33. Sigma converted via wrong pointer** ([include/rpnbuiltin.h:542](include/rpnbuiltin.h#L542))
In `mcGenerator`, the sigma validation calls `v2p->convertInPlace(Value::Type::Real)` (the variation pointer) instead of `v3p->convertInPlace(Value::Type::Real)`. The sigma argument is never converted; the variation argument is converted twice and sigma is read from `v3p` unconverted at line 549.

---

## include/parseroutput.h

- [x] **34. `setTitle` rvalue overload calls `add` instead of `setTitle`** ([include/parseroutput.h:768](include/parseroutput.h#L768))
`ParserTables&& setTitle(const std::string t) &&` returns `std::move(this->add(t))`. There is no `add(std::string)` overload; `add` accepts `PTLoad&&` or `PTEmbed&&`. The line should call `this->setTitle(t)` to delegate to the lvalue overload, matching the pattern of every other rvalue overload in that block.

---

## include/introspection.h

- [x] **35. `Id` member read and written as `String`** *(not a bug: Value does not distinguish Id from String)* ([include/introspection.h:178](include/introspection.h#L178))
In the `StructMember::Type::Id` branch, both the change check and the assignment use `vwrite->val<const String>()` instead of `vwrite->val<const Id>()`. Every other branch reads the value through its own type. If `Id` stores more than a plain string (e.g. an interned index), this reads and writes the wrong representation and the `changed` comparison is unreliable.

---

## CMakeLists.txt

- [x] **36. Unclosed brace in `set(ENV{...})` call** ([CMakeLists.txt:71](CMakeLists.txt#L71))
`set(ENV{LLVM_SYS_181_PREFIX "${LLVM_PREFIX}")` is missing the closing `}` on the variable name. The correct form is `set(ENV{LLVM_SYS_181_PREFIX} "${LLVM_PREFIX}")`. As written this is a CMake syntax error that breaks configuration on macOS/Homebrew builds.

---

## packaging.cmake

- [ ] **37. Typo in permissions variable name renders file permissions empty** ([packaging.cmake:18](packaging.cmake#L18))
`set(install_permisssions_file ...)` has three `s` characters. Every subsequent `install()` call that supplies `PERMISSIONS ${install_permissions_file}` (correctly spelled, two `s`) expands an undefined variable, which CMake silently treats as an empty string. The `PERMISSIONS` keyword receives no values, so all installed files (license, docs, demo, test, config, Python scripts, inc files) are installed with CMake's default permissions rather than the intended read-only set.

---

## devices/CMakeLists.txt

- [x] **38. Numeric `EQUAL` used for string comparison** ([devices/CMakeLists.txt:96](devices/CMakeLists.txt#L96))
`if (NOT "${tmpdirectory}" EQUAL "." ...)` uses the numeric comparison operator `EQUAL` to compare a directory-path string against `"."`. String comparison requires `STREQUAL`. On the very next check inside the same file (line 88) `STREQUAL` is used correctly. With `EQUAL`, CMake attempts a numeric comparison; both operands are non-numeric strings, so the result is implementation-defined and typically evaluates to false, meaning the guard never fires and the subdirectory bookkeeping is skipped for every path including top-level ones.

- [x] **39. Typo in variable name creates unused dead variable** ([devices/CMakeLists.txt:68](devices/CMakeLists.txt#L68))
`set(osdi_solurce_files "")` defines a variable that is never referenced again. The list that is actually built and consumed by `add_custom_target` is `osdi_source_files` (correctly spelled), which is never explicitly initialised to empty before the loop's `list(APPEND ...)` calls. The typo'd variable is dead code.

---

## test/CMakeLists.txt

- [x] **40. Duplicate `file(COPY ...)` to the same destination** ([test/CMakeLists.txt:39](test/CMakeLists.txt#L39))
Inside the `foreach` loop, line 38 and line 39 both copy each dependency file to `${CMAKE_CURRENT_BINARY_DIR}/manual_run`. The second copy is redundant and indicates a copy-paste error (the intended destinations were probably `${CMAKE_CURRENT_BINARY_DIR}` and `${CMAKE_CURRENT_BINARY_DIR}/manual_run`, but line 37 already covers the former).

---

## lib/CMakeLists.txt

- [x] **41. Flex `-d` debug flag active in all build types** *(intentional)* ([lib/CMakeLists.txt:3](lib/CMakeLists.txt#L3))
`COMPILE_FLAGS "-d"` is passed unconditionally to the Flex lexer. The `-d` flag enables Flex's built-in scanner debug output, which prints a trace of every token matched to `stderr` at runtime. This is present in Release builds and will produce unwanted noise in production.

---

## test/test_diode.sim

- [x] **42. Mixed tab/space indentation in embedded Python `else` block** ([test/test_diode.sim:64](test/test_diode.sim#L64))
The `if isTest():` branch (lines 61-62) uses 4-space indentation. The `else:` branch starting at line 64 uses tab characters throughout. Python 3 raises `TabError: inconsistent use of tabs and spaces in indentation` when this file is executed in a non-test context.

---

## demo/spice/mes1.sim

- [x] **43. Typo "MESFETT" in P-MESFET figure title** ([demo/spice/mes1.sim:93](demo/spice/mes1.sim#L93))
`fig2.suptitle('P-MESFETT level 1 test')` has an extra `T`. Should be `'P-MESFET level 1 test'`.

---

## demo/spice/mes1inv.sim

- [x] **44. Figure titles say "JFET" instead of "MESFET"** ([demo/spice/mes1inv.sim:49](demo/spice/mes1inv.sim#L49))
`fig1.suptitle('N-JFET level 1 inverter')` (line 49) and `fig2.suptitle('P-JFET level 1 inverter')` (line 73) both use the wrong device type. The circuit uses `sp_mes1` MESFET models; both titles should read `'N-MESFET level 1 inverter'` and `'P-MESFET level 1 inverter'` respectively.

---

## demo/spice/jfet2amp.sim

- [x] **45. Wrong frequency axis divisor in P-JFET noise plot** ([demo/spice/jfet2amp.sim:190](demo/spice/jfet2amp.sim#L190))
Lines 190-194 plot the P-JFET noise results using `plot['frequency']/163` as the x-axis, giving a nonsensical unit. The corresponding N-JFET section (lines 133-137) correctly uses `/1e6` to convert Hz to MHz. All five `loglog` calls in the P-JFET noise block should divide by `1e6`.

---

## demo/spice/bsim4v8inv.sim

- [x] **46. Title says "BSIM3" instead of "BSIM4"** ([demo/spice/bsim4v8inv.sim:1](demo/spice/bsim4v8inv.sim#L1))
The title line reads `BSIM3 4.8.2 MOSFET inverter` but the file loads `spice/bsim4v8.osdi` and includes `bsim4v82.inc`. It should read `BSIM4 4.8.2 MOSFET inverter`.

---

## demo/spice/bsim4v8amp.sim

- [x] **47. Noise traces for Rbps/Rbpd/Rbpb all read `n(m1,rbsb)`** ([demo/spice/bsim4v8amp.sim:116](demo/spice/bsim4v8amp.sim#L116))
Lines 116-118 (NMOS section) and 184-186 (PMOS section) each label three noise contributions as `"M1,Rbps"`, `"M1,Rbpd"`, and `"M1,Rbpb"`, but all three access `plot['n(m1,rbsb)']`. The distinct keys `n(m1,rbps)`, `n(m1,rbpd)`, and `n(m1,rbpb)` are never read; every "Rbps"/"Rbpd"/"Rbpb" trace is in fact a duplicate of the `rbsb` trace.

- [x] **48. Noise traces for Igs and Igd both read `n(m1,igb)`** ([demo/spice/bsim4v8amp.sim:120](demo/spice/bsim4v8amp.sim#L120))
Lines 120-121 (NMOS section) and 188-189 (PMOS section) label two traces `"M1,Igs"` and `"M1,Igd"`, but both access `plot['n(m1,igb)']`. The distinct keys `n(m1,igs)` and `n(m1,igd)` are never read; both traces are duplicates of the `igb` trace.

---

## demo/spice/vdmossh.sim

- [x] **49. Legend labels say "delta Tj"/"delta Tc" but values are absolute temperature** ([demo/spice/vdmossh.sim:87](demo/spice/vdmossh.sim#L87))
Lines 87-88 plot `tj` and `tc` with legends `"delta Tj"` and `"delta Tc"`, and line 91 plots `27+plot['m1.p']*(0.4+4)` labeled `"delta Tj target"`. The inline comment on line 40 reads `"Initial junction temperature is 27 deg C"`, and the transient initial condition `ic=["tj"; 27]` confirms `tj` is absolute temperature in degrees C. The labels should be `"Tj"`, `"Tc"`, and `"Tj target"` (not "delta").

---

## demo/bsim3-ptm/amp.sim

- [x] **50. `set_xlabel` called twice; y-axis label never set** ([demo/bsim3-ptm/amp.sim:59](demo/bsim3-ptm/amp.sim#L59))
Lines 58-59 call `set_xlabel('Vin [V]')` then immediately `set_xlabel('Vout [V]')` on the same axes. The second call overwrites the first and should be `set_ylabel('Vout [V]')`. The y-axis label is never set for either the NMOS figure (line 59) or the PMOS figure (line 66).

---

## demo/bsim3-ptm/diffpair.sim

- [x] **51. `set_xlabel` called twice; y-axis label never set** ([demo/bsim3-ptm/diffpair.sim:51](demo/bsim3-ptm/diffpair.sim#L51))
Same issue: line 50 calls `set_xlabel('Vin [V]')` and line 51 calls `set_xlabel('Vout [V]')` on the same axes. Line 51 should be `set_ylabel('Vout [V]')`. The y-axis label is never set.

---

## demo/bsim3-ptm/mirror.sim

- [x] **52. Wrong x-axis label on PMOS mirror input plot** ([demo/bsim3-ptm/mirror.sim:115](demo/bsim3-ptm/mirror.sim#L115))
`set_xlabel('Vds [V]')` is used for the `dc4` plot, but `dc4` sweeps `ibias` (the current-source `dc` parameter). The x-axis label should be `'Iin [uA]'`, matching the analogous NMOS plot for `dc2` at line 100.

---

## demo/multiplier/adders.sim

- [x] **53. `plot_signals` uses global `tm` instead of `plot` parameter** ([demo/multiplier/adders.sim:64](demo/multiplier/adders.sim#L64))
Inside `def plot_signals(axes, plot, signals, scaling):`, the function body references `tm['time']` and `tm[name]` instead of `plot['time']` and `plot[name]`. The `plot` parameter is never used; the function always plots data from whichever dataset is currently in the global `tm`.

- [x] **54. Figure title typo "Half added"** ([demo/multiplier/adders.sim:67](demo/multiplier/adders.sim#L67))
`fig1.axes[0].set_title('Half added')` should be `'Half adder'`.

---

## demo/control/uncondfatal.sim

- [x] **55. Title copy-pasted from `initfatal.sim`** ([demo/control/uncondfatal.sim:1](demo/control/uncondfatal.sim#L1))
The title reads `Test $fatal() in initialization`. The embedded Verilog-A module calls `$fatal(0, "Unconditional abort.")` unconditionally (no `if` or init-phase guard), so the title should describe an unconditional fatal, not an initialization-time one.

---

## benchmark/mul/vacask/runme.sim

- [ ] **56. `c3` connects the same node pair as `c1` — wrong circuit topology** ([benchmark/mul/vacask/runme.sim:20](benchmark/mul/vacask/runme.sim#L20))
In the Cockcroft-Walton voltage multiplier, `c1 (1 2)` and `c3 (1 2)` both connect nodes 1 and 2, so `c3` is in parallel with `c1` rather than forming the second-stage pump. Given that `d3 (10 2)` rectifies from node 10 to node 2, the second-stage pump capacitor should be `c3 (10 2)`. The same topology error exists in `benchmark/vadistiller/mul/vacask/runme.sim:20`.

- [ ] **57. Plot label `"v(1) exact"` should reference node 20** ([benchmark/mul/vacask/runme.sim:56](benchmark/mul/vacask/runme.sim#L56))
`fig1.axes[0].plot(t*1000, v20, ..., label="v(1) exact")` plots variable `v20` (node 20 voltage) but labels it `"v(1) exact"`. The label should be `"v(20)"`. The same stale label exists in `benchmark/vadistiller/mul/vacask/runme.sim:54`.

---

## test/test_acstb.sim

- [x] **58. Interactive plot accesses nonexistent key `stb1["w"]`** ([test/test_acstb.sim:102](test/test_acstb.sim#L102))
In the `else` (interactive) branch, line 102 accesses `stb1["w"]`, but the STB analysis only outputs `"wf"` (forward return ratio) and `"wr"` (reverse return ratio), as used on lines 64 and 69. The interactive plot should use `stb1["wf"]`.

---

## test/test_capacitor.sim

- [x] **59. Legend label `"I(L1)"` in an RC test with no inductor** ([test/test_capacitor.sim:71](test/test_capacitor.sim#L71))
`plot(['time']*1000, plot['2'], ..., label="I(L1)")` labels the simulated node-2 voltage trace as inductor current. The circuit is a resistor-capacitor network with no inductor; the label should be `"V(2)"`.

---

## test/test_ctlsrc.sim

- [x] **60. Diagnostic print uses `i` instead of `v` for `vcvs1.v`** ([test/test_ctlsrc.sim:96](test/test_ctlsrc.sim#L96))
Line 93 assigns `v = op1["vcvs1.v"]`; line 95 correctly passes `v` to `relDiff`; but line 96 prints `print("vcvs1.v=", i, ...)`, silently reporting the stale value of `i` (last assigned to `vccs1.i`) instead of `v`.

---

## test/test_visrc.sim

- [x] **61. Diagnostic print uses `v` instead of `i` for `i(v1)`** ([test/test_visrc.sim:72](test/test_visrc.sim#L72))
Line 69 assigns `i = op1["v1:flow(br)"]`; line 71 correctly passes `i` to `relDiff`; but line 72 prints `print("i(v1)=", v, ...)`, reporting the stale value of `v` (node-1 voltage) instead of the branch current `i`.

---

## test/test_hb2.sim

- [x] **62. Comment states wrong modulation frequency** ([test/test_hb2.sim:14](test/test_hb2.sim#L14))
Line 14 reads `// x = 4*sin(2*pi*50k*t)*(1+0.5*sin(2*pi*1k*t))` but the actual instance on line 10 has `modfreq=5k`. The subsequent comments in that block correctly use `5k` throughout; only the formula on line 14 has `1k`.

---

## python/rawfile.py

- [ ] **63. `sweepGroups` assigned as local variable instead of `self.sweepGroups`** ([python/rawfile.py:49](python/rawfile.py#L49))
In the `else` branch (no sweeps), line 49 writes `sweepGroups = 1` as a plain local. The `if sweeps>0` branch correctly writes `self.sweepGroups = self.allbegins.size` (line 45). The attribute is therefore never set on zero-sweep objects; any access to `self.sweepGroups` raises `AttributeError`.

- [ ] **64. File handle leaked — no context manager around `open`** ([python/rawfile.py:100](python/rawfile.py#L100))
`fp = open(fname, 'rb')` is never closed and has no `with` block. If any exception is raised during parsing (e.g., the `NotImplementedError` calls or assertion failures), the file handle leaks.

- [ ] **65. Bare `except:` swallows `KeyboardInterrupt` and `SystemExit`** ([python/rawfile.py:107](python/rawfile.py#L107))
`except:` catches `BaseException`. The re-raised `RuntimeError` discards the original exception with no chaining, losing the root cause.

- [ ] **66. Spurious `fp.readline()` after binary block may skip next header** ([python/rawfile.py:144](python/rawfile.py#L144))
`np.fromfile` leaves the file pointer immediately after the last byte of binary data. If there is no trailing newline in the raw format between the binary block and the next `Title:` line, `fp.readline()` consumes the title of the next plot, causing the second and later plots to be parsed incorrectly.

---

## python/ng2vc.py

- [ ] **67. Stray `.")` embedded in the help string** ([python/ng2vc.py:13](python/ng2vc.py#L13))
The first line of the triple-quoted help string reads `"""Ngspice to VACASK netlist converter.")` — the closing `.")` is part of the string content and is printed to the user verbatim.

- [ ] **68. Typo "ba" in help text** ([python/ng2vc.py:31](python/ng2vc.py#L31))
`(infinite ba default)` should be `(infinite by default)`.

- [ ] **69. "Need input file." check is dead code; missing `fromFile is None` guard** ([python/ng2vc.py:85](python/ng2vc.py#L85))
The condition `ndx+1 > len(sys.argv)` is always false when the `else` branch is reached (because `sys.argv[ndx]` was just successfully read). The "Need input file." error is unreachable. Meanwhile there is no check after the loop for `fromFile is None`, so passing only flag arguments and no filename causes `converter.convert(None, toFile)` to crash.

- [ ] **70. Hardcoded `sourcepath` overwrites all user-supplied `-sp` arguments** ([python/ng2vc.py:106](python/ng2vc.py#L106))
`cfg["sourcepath"] = [ ".", "/home/arpadb/sim/IHP-Open-PDK/..." ]` unconditionally replaces the list built from `-sp` arguments (line 44/76). Every user-supplied sourcepath is silently discarded, and the hardcoded developer-machine path is used instead.

---

## python/ng2vclib/generators.py

- [ ] **71. Mutable default argument `input_history=[]`** ([python/ng2vclib/generators.py:4](python/ng2vclib/generators.py#L4))
The `traverse` generator uses `[]` as the default for `input_history`. In Python, mutable default arguments are shared across all calls; any mutation of the list in one call persists into the next call that uses the default.

- [ ] **72. Missing space before section name in error message** ([python/ng2vclib/generators.py:76](python/ng2vclib/generators.py#L76))
`"included as section"+sec+" on line "` concatenates the section name directly to `"section"` with no space. The message prints as e.g. `"included as sectionmylib on line 5"`.

---

## python/ng2vclib/m_file.py

- [ ] **73. Bare `except:` swallows all exceptions** ([python/ng2vclib/m_file.py:141](python/ng2vclib/m_file.py#L141))
`except:` catches `BaseException` including `KeyboardInterrupt` and `SystemExit`, then raises a `ConverterError` with no exception chaining, discarding the original error. Should be `except OSError as e: raise ConverterError(...) from e`.

- [ ] **74. `s[:-1]` silently produced when `.lib` line has no whitespace at depth 0** ([python/ng2vclib/m_file.py:294](python/ng2vclib/m_file.py#L294))
When `sndx = s.rfind(" ")` returns -1 (no space found), the error is only raised when `depth > 0`. At depth 0 the code falls through to `lfname = s[:sndx].strip()` where `sndx == -1`, so Python evaluates `s[:-1]`, silently dropping the last character of the string and producing a wrong filename.

---

## python/ng2vclib/m_masters.py

- [ ] **75. `annot["orig_name"]` stores lowercased name instead of original case** ([python/ng2vclib/m_masters.py:101](python/ng2vclib/m_masters.py#L101))
Line 86 extracts `orig_name = orig_parts[0]` (original-case name), but line 101 writes `annot["orig_name"] = name` (the lowercased name). The `orig_name` annotation always holds the lowercased version, defeating its purpose for the `original_case_instance` config option.

- [ ] **76. `NameError`: undefined `ll` should be `l1`** ([python/ng2vclib/m_masters.py:138](python/ng2vclib/m_masters.py#L138))
`pat_preosdi.match(ll)` references `ll`, which is never defined in this scope. The variable extracted on lines 135-137 is `l1`. The code raises `NameError` on every pre-OSDI comment line encountered during pass 1.

- [ ] **77. Version string parsed as `int` but `family_map` uses string keys** ([python/ng2vclib/m_masters.py:213](python/ng2vclib/m_masters.py#L213))
`version = int(pval)` will raise `ValueError` for any non-integer version string (e.g. `"3.3"`), and even when it succeeds the wrong type is stored — `family_map` in `dfl.py` uses string keys for version.

---

## python/ng2vclib/m_inst_passive.py

- [ ] **78. Component value overwritten by `process_instance_params` in R/C/L handlers** ([python/ng2vclib/m_inst_passive.py:55](python/ng2vclib/m_inst_passive.py#L55))
In `process_instance_r`, the value entry `[("r", self.format_value(parts[2]))]` is carefully placed in `psplit` on lines 30 or 47. Line 55 then overwrites `psplit` entirely with the return of `process_instance_params`, discarding the value entry. The same bug exists in `process_instance_c` and `process_instance_l`. Component values specified as positional arguments (not as `r=`, `c=`, `l=`) are silently dropped from the output.

---

## python/ng2vclib/m_inst_q.py

- [ ] **79. Missing space between model name and parameters** ([python/ng2vclib/m_inst_q.py:41](python/ng2vclib/m_inst_q.py#L41))
`txt += fmted` appends formatted parameters with no leading space. Every other instance handler uses `txt += " " + fmted`. The generated BJT instance line will have no space between the model name and the first parameter.

---

## python/ng2vclib/m_inst_x.py

- [ ] **80. `original_case_subckt` condition is inverted** ([python/ng2vclib/m_inst_x.py:24](python/ng2vclib/m_inst_x.py#L24))
When `original_case_subckt` is `True`, `output_model` is set to `annot["mod_name"]` (the lowercased name). When it is `False`, `output_model` is set to `annot["orig_mod_name"]` (the original-case name). The branches are swapped: `True` should use the original-case name and `False` should use the lowercased name.

---

## python/ng2vclib/m_model.py

- [ ] **81. `len(params)>=0` is always `True`** ([python/ng2vclib/m_model.py:55](python/ng2vclib/m_model.py#L55))
List length is never negative, so the condition never suppresses the parameter block. When `params` is empty and `paren` is `False`, a spurious blank formatted line is appended. The condition should be `len(params)>0`.

---

## python/ng2vclib/m_params.py

- [ ] **82. `psplit[ii]` uses last-loop index instead of `mfact_index`** ([python/ng2vclib/m_params.py:189](python/ng2vclib/m_params.py#L189))
After the `for ii, split in enumerate(splits):` loop, `ii` holds the index of the last element. Line 189 writes `psplit[ii] = (psplit[ii][0], "("+psplit[ii][1]+")*"+m_chain)` intending to update the `$mfactor` entry, but `mfact_index` (set on line 182) is ignored. If `$mfactor` is not the last parameter, the wrong entry is multiplied.

- [ ] **83. Mutable default argument `to_remove=set()` in `remove_params`** ([python/ng2vclib/m_params.py:207](python/ng2vclib/m_params.py#L207))
The shared mutable default set is reused across all calls; any mutation would persist. Should be `to_remove=None` with an `if to_remove is None: to_remove = set()` guard.

- [ ] **84. Mutable default argument `vecnames=set()` in `merge_vectors`** ([python/ng2vclib/m_params.py:222](python/ng2vclib/m_params.py#L222))
Same mutable default argument problem as bug 83.

- [ ] **85. Missing `ndx += 1` in `merge_vectors` while loop causes infinite loop** ([python/ng2vclib/m_params.py:237](python/ng2vclib/m_params.py#L237))
Inside `while ndx < len(params):`, `ndx` is never incremented. When a vector parameter entry ends with a comma (triggering the loop), the same `params[ndx]` is read indefinitely and `merged` grows without bound.

---

## python/sg13g2tovc.py

- [ ] **86. `finditer` searches full `line` instead of post-`@model` substring `line2`** ([python/sg13g2tovc.py:125](python/sg13g2tovc.py#L125))
Lines 121-122 split `line` into `line1` (before `@model`) and `line2` (after `@model`), but line 125 runs `pat_identifier_assign.finditer(line)` over the full original `line`. Identifiers that appear before `@model` (e.g. `spectre_format=`) are incorrectly lowercased. `line1` and `line2` are dead variables.

- [ ] **87. Loop variable `file` shadowed by inner assignment** ([python/sg13g2tovc.py:291](python/sg13g2tovc.py#L291))
Inside the `for file, read_process_depth, output_depth, destpath in tech_files:` loop, lines 290-291 execute `file, _, _ = cvt.cfg["family_map"][k]`, rebinding `file` to a different value for the remainder of that outer-loop body.

---

## python/vacaskpp.py

- [ ] **88. `NameError` when input has no NaN bins** ([python/vacaskpp.py:53](python/vacaskpp.py#L53))
In `logBinMean`, `fret` and `Pret` are only assigned inside `if idx.size > 0:`. When `Pb` contains no NaN values (the common case), the `if` body is skipped and `return fret, Pret` on line 53 raises `NameError` because neither name is defined.

---

## python/xschem2vc.py

- [ ] **89. `convert(fname, None)` deletes `.orig` backup then raises** ([python/xschem2vc.py:91](python/xschem2vc.py#L91))
In the `__main__` block, `convert` is called with `cvt=None`, overriding the default `simple_patcher`. Inside `convert`, `None` is neither a `str` nor callable, so the `else` branch (lines 62-64) runs: `os.remove(origfile)` destroys the backup, then `raise Exception(...)` is raised. The caller likely intended to use the default patcher by omitting the second argument.

- [ ] **90. Existing `spectre_format=` line is never updated** ([python/xschem2vc.py:67](python/xschem2vc.py#L67))
When `format_spectre_line is not None` (the line already exists), the `pass` block discards `fstxt` — the freshly computed corrected value — and the file is still rewritten from `orig_lines` (lines 74-76) without any change. A stale or incorrect `spectre_format=` value is silently preserved on re-conversion.

---

## lib/dfllexer.l

- [x] **91. Hex integer pattern rejects `a-f` / `A-F` digits** ([lib/dfllexer.l:501](lib/dfllexer.l#L501))
The pattern `<BODY>0[xX][0-9]*` only accepts decimal digits after the `0x` prefix. `input-numbers.md:13` documents hexadecimal integers with examples `0xFF` and `0x1a3` — both contain letter digits. With the current regex, `0xFF` lexes as HEXINTEGER(`0x` → 0) followed by IDENTIFIER(`FF`). The pattern should be `0[xX][0-9a-fA-F]+` (the `+` also disallows the empty `0x`).

- [x] **92. Curly braces `{}` documented as grouping delimiters but not handled by lexer** *(fixed in `analysis/hbac` branch)* (entire file)
`input-overview.md:24` states that newlines within `()`, `[]`, and `{}` are ignored. The lexer has `inParen` and `inBracket` counters and explicit rules for `(`, `)`, `[`, `]`, but no rule for `{` or `}` and no `inBrace` counter. Any `{` or `}` falls into the default `.` rule and is rejected with "Syntax error, unexpected string".

- [x] **93. Backslash-newline silently consumed inside double-quoted strings** ([lib/dfllexer.l:246](lib/dfllexer.l#L246))
`<QUOTED>\\\n` updates the line counter and adds nothing to `sbuf`, treating `\<newline>` as a line continuation. `input-strings.md:46-48` explicitly states "Literal newlines are not allowed in double-quoted strings. To include a newline, use the `\n` escape sequence." The documented escape table (lines 23-33) has no line-continuation entry; by the documented fallback rule "any other character `x` yields `x`", `\<newline>` should yield a literal newline character, not be silently dropped.

- [x] **94. Heredoc end marker `>>>MARKER` matched mid-line** ([lib/dfllexer.l:180](lib/dfllexer.l#L180))
`<LONGQUOTED>\>\>\>[a-zA-Z0-9_]+` matches `>>>MARKER` anywhere in the heredoc body, with no leading-line anchoring and no trailing-newline requirement. `input-strings.md:53` says the marker must be on a line by itself. The opening rule (line 566) correctly requires `<<<MARKER` to be followed by `[\r\t ]*\n`; the closing rule does not. Mid-line `>>>MARKER` will prematurely terminate the embed.

- [x] **95. Typo "incldued" in user-visible error message** ([lib/dfllexer.l:466](lib/dfllexer.l#L466))
`"...You probably incldued a library file."` should read "included".

- [x] **96. Typo "consruction-" in YY_USER_INIT comment** ([lib/dfllexer.l:33](lib/dfllexer.l#L33))
`/* This is called at lexer consruction- */` — should be "construction.".

- [x] **97. Stale comment "Do not switch to BODY yet" contradicts next line** ([lib/dfllexer.l:675](lib/dfllexer.l#L675))
The `<LINESTART>global` rule's comment says `// Do not switch to BODY yet.` but the very next statement is `BEGIN(BODY);`. Parallel rules (`model`, `ground`, etc.) confirm `BEGIN(BODY)` is correct; the comment is stale.

---

## lib/dflparser.y

- [x] **98. `$$.isToplevel` checked on default-constructed value; subckt scope restriction is bypassed** ([lib/dflparser.y:330](lib/dflparser.y#L330))
Four productions — `subckt_build global` (line 328), `subckt_build ground` (340), `subckt_build load` (352), and `subckt_build embed` (362) — guard with `if (!$$.isToplevel)`. With Bison variant value types, `$$` is a freshly default-constructed `struct subckt` whose member `isToplevel` is initialized to `true` (in-class initializer on line 75). The guard is therefore always false and the error is never raised. `cir-nodes.md:20,31`, `cir-loading.md:12`, and `cir-subckt.md:121-126` document these statements as toplevel-only. The very next production (line 374, `subckt_build control_block`) correctly reads `$1.isToplevel` — the four buggy productions should match that pattern.

- [x] **99. Wrong location stored for duplicate sweep name** ([lib/dflparser.y:948](lib/dflparser.y#L948))
In `sweeps SWEEP IDENTIFIER opt_broken_parameter_list`, SWEEP is at position 2, IDENTIFIER at position 3. The insertion `$$.locations.insert({id, @1})` stores the position-1 (`sweeps` aggregate) location instead of the new SWEEP keyword's location. The first-sweep production on line 940 correctly uses `@1` because SWEEP is at position 1 there. The redefinition production should use `@2`. The "first defined here" pointer becomes wrong if a third sweep collides with the same name.

- [x] **100. BLKENDIF display string is `"@endif"` but the actual keyword is `@end`** ([lib/dflparser.y:188](lib/dflparser.y#L188))
`%token BLKENDIF "@endif"` declares the Bison display string as `"@endif"`. The lexer ([lib/dfllexer.l:666](lib/dfllexer.l#L666)) emits BLKENDIF for `@end`, and `input-reserved.md:28`, `cir-conditional.md`, `cir-binning.md:31`, and `cir-subckt.md:119` all document the terminator as `@end`. With `parse.error verbose` enabled (line 13), verbose syntax errors involving the conditional terminator will report `@endif`, which does not exist as a keyword.

- [x] **101. `OPTIONS` token declared but never used** ([lib/dflparser.y:126](lib/dflparser.y#L126))
`%token OPTIONS "options"` is declared but does not appear in any production rule and is never emitted by the lexer. The `options` keyword is handled as a plain identifier through the generic `command` rule.

- [x] **102. `RIGHTARROW` token declared and emitted but never consumed** *(intentional: reserved for future use)* ([lib/dflparser.y:175](lib/dflparser.y#L175))
`%token RIGHTARROW "->"` is declared and the lexer returns it for `->` ([lib/dfllexer.l:642](lib/dfllexer.l#L642)), but no production references it. Any source containing `->` will tokenize cleanly but yield a generic syntax error. No documented language feature uses `->`.

- [x] **103. Unused local `tailLen`** ([lib/dflparser.y:560](lib/dflparser.y#L560))
`auto tailLen = $3.size();` is declared in the `expr AND expr` action (line 560) and again in `expr OR expr` (line 580). Neither use of `tailLen` is read.

- [x] **104. Comment typo "RPM" should be "RPN"** ([lib/dflparser.y:592](lib/dflparser.y#L592))
`// Ternary operator a?b:c, translation to RPM` — the project's stack format is RPN (Reverse Polish Notation), not RPM.

- [x] **105. Comment typo "upacked"** ([lib/dflparser.y:676](lib/dflparser.y#L676))
`// List with single element upacked` — should be "unpacked" (the parallel comment on line 682 spells it correctly).

---

## lib/dfllexer.l (action code)

- [x] **106. `inParen` / `inBracket` are `size_t` and underflow on stray `)` or `]`** *(fixed in `analysis/hbac` branch)* ([include/dflscanner.h:86-87](include/dflscanner.h#L86), [lib/dfllexer.l:476](lib/dfllexer.l#L476), [lib/dfllexer.l:485](lib/dfllexer.l#L485), [lib/dfllexer.l:768](lib/dfllexer.l#L768))
Both counters are unsigned. On a stray `)` or `]` without a matching `(`/`[`, the decrement at line 476/485 wraps the counter to `SIZE_MAX`. The guard `if (inParen<=0 && inBracket<=0)` at line 768 then evaluates to false for the rest of the file, so every subsequent newline in `BODY` is silently swallowed and no `NEWLINE` token is ever produced again. The parser sees one continuous line and yields a cascade of confusing errors. Either the counters should be signed, or the decrement should clamp at zero and report an error.

- [x] **107. `<QUOTED>\n` error path leaves QUOTED on the start-condition stack** ([lib/dfllexer.l:217](lib/dfllexer.l#L217))
When an unterminated string ends at a newline, the action emits YYerror but does not call `yy_pop_state()` and does not reset `sbuf`. After the parser's error recovery, the next `yylex` call resumes in QUOTED state with stale `sbuf` content, treating subsequent input as continuation of the broken string.

- [x] **108. `<QUOTED>\\[0-9]+` and `<QUOTED>.` error paths leak `sbuf` (and one of them leaks state too)** ([lib/dfllexer.l:230](lib/dfllexer.l#L230), [lib/dfllexer.l:249](lib/dfllexer.l#L249))
The bad-escape rule at line 230 calls `yy_pop_state()` but does not clear `sbuf`, so the next string literal opened from the popped-to state begins with the previous string's tail. The generic catch-all error at line 249 calls neither `yy_pop_state()` nor clears `sbuf` — same problem as bug 107.

- [x] **109. `HEXINTEGER` value silently narrows from `unsigned long` to `int32_t`** ([lib/dfllexer.l:502](lib/dfllexer.l#L502))
`yylval->build<Int>(std::stoul(yytext, nullptr, 16))` reads an `unsigned long` and stores into `Int` (= `int32_t`, per `include/common.h:38`). Once the hex regex (bug 91) is fixed, values like `0xFFFFFFFF` become `-1` via implementation-defined narrowing, and `0xFFFFFFFFFFFFFFFF` raises `std::out_of_range` from `stoul`, which is not caught.

- [x] **110. `std::stoi` / `std::stoul` / `std::stod` overflow exceptions are not caught** ([lib/dfllexer.l:497](lib/dfllexer.l#L497), [lib/dfllexer.l:510](lib/dfllexer.l#L510), [lib/dfllexer.l:516](lib/dfllexer.l#L516), [lib/dfllexer.l:559](lib/dfllexer.l#L559), [lib/dfllexer.l:563](lib/dfllexer.l#L563))
None of the numeric conversion calls are wrapped in `try/catch`. A literal like `99999999999999999999` or `1e9999` throws `std::out_of_range`, which propagates out of `yylex` and terminates the process instead of producing a clean syntax error.

- [x] **111. `pushStream` failure leaves the lexer stuck in `INCEND` / `LIBEND`** ([lib/dfllexer.l:317](lib/dfllexer.l#L317), [lib/dfllexer.l:383](lib/dfllexer.l#L383))
The `<INCEND>\n` failure branch (file not found) at lines 317-321 and the analogous `<LIBEND>\n` failure at lines 383-387 return YYerror without `BEGIN(LINESTART)` or any state reset. After the parser's error recovery the next `yylex` resumes in `INCEND`/`LIBEND` and will fall into the catch-all `<INCEND>.` / `<LIBEND>.` error rule on the very first non-whitespace character, producing spurious follow-on errors.

- [x] **112. `std::string(yytext, 0, i)` copies the whole token buffer** ([lib/dfllexer.l:559](lib/dfllexer.l#L559))
This call resolves to `std::string(std::string(yytext), 0, i)` via the implicit `const char*` → `std::string` conversion required by the matching 3-arg constructor `basic_string(const basic_string&, size_type pos, size_type count)`. The intermediate `std::string(yytext)` copies the entire `yytext` C-string just to extract the first `i` characters. The intended efficient form is `std::string(yytext, i)` (the 2-arg `(const char*, size_type)` constructor).

---

## lib/dflparser.y (action code)

- [x] **113. `ends <name>` does not verify the trailing name matches the open `subckt`** ([lib/dflparser.y:395](lib/dflparser.y#L395))
The production `subckt : subckt_build ENDS IDENTIFIER NEWLINE` accepts a trailing identifier (the documented convention for self-documenting closes), but the action ignores `$3`. The user can write `ends WRONG_NAME` to close any subcircuit and no diagnostic is emitted. Either the identifier should be checked against the opening name stored in `$1.def` or the trailing-identifier form should be removed.

- [x] **114. Missing `std::move` on STRING semantic values** ([lib/dflparser.y:464](lib/dflparser.y#L464), [lib/dflparser.y:925](lib/dflparser.y#L925))
`value : STRING { $$ = Value($1); }` (line 464) and `load : LOAD STRING NEWLINE { $$ = std::move(PTLoad($2, @1.loc())); }` (line 925) copy the `std::string` semantic value. The neighbouring `embed` production at line 933 correctly uses `std::move($2)`, confirming the missing moves are a copy-paste regression.
