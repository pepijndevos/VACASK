# VACASK C++ source audit - bugs

Audit of `.cpp` and `.h` files in [include/](include/), [lib/](lib/), [simulator/](simulator/), [demo/](demo/). Each item is a real or strongly-suspected defect; severity varies. Style-only nitpicks omitted.

## Critical (logic inversion / incorrect control flow)

- [x] **1.** **[lib/coretran.cpp:374](lib/coretran.cpp#L374)** — Inverted error check in `TranCore::populateStructures()`. The second `createJacobianEntry` call returned `false` when `ok` was `true` (which means: succeed → bail out; fail → proceed). Fixed by changing `ok` to `!ok`, matching the sibling check at [line 370](lib/coretran.cpp#L370).

- [x] **2.** **[lib/coretran.cpp:673-677](lib/coretran.cpp#L673-L677)** — `setError(TranError::NoiseMode)` was invoked when `params.noisemode` was invalid but the coroutine did **not** `co_yield CoreState::Aborted`; execution continued with a bad noise mode. Fixed by adding the missing `co_yield CoreState::Aborted;` right after `setError`, matching the sibling Fmax/Oversample/Fmin/Rows branches.

- [ ] **3.** **[lib/coretran.cpp:1241-1262](lib/coretran.cpp#L1241-L1262)** — Trapezoidal LTE filter computes `correct` from a slope/crossing test, then unconditionally overrides it with `correct = true;` ([line 1252](lib/coretran.cpp#L1252)). The whole block computing `kdelta`/`hcross` is dead, the slope-based decision is silently disabled.

- [x] **4.** **[lib/coretran.cpp:364](lib/coretran.cpp#L364)** — `if (!node1 | !node2)` used bitwise `|` instead of logical `||`. Both operands are pointer-style booleans, so the value was correct, but short-circuit evaluation was lost and the line read as a typo. Codebase-wide scan ran four grep variants filtered against the bit-flag noise (`*Flags`, `*MASK`, `Component::*`, `static_cast`, address-of, etc.) — this was the **only** instance of `&`/`|` standing in for `&&`/`||`. Fixed to `||`.

- [x] **5.** **[lib/circuit.cpp:376-380](lib/circuit.cpp#L376-L380)** — Silent shadowing of an outer name. The `while` condition's `pos` is *not* the inner `pos` (which is out of scope past the closing brace) — it binds to the *outer* `pos` introduced by the structured binding `auto [fs, pos, line, offset] = itld.location().data();` at [line 294](lib/circuit.cpp#L294). The outer `pos` is a `FileStackFileIndex` describing the source location of the `load` directive and is constant for the whole `do/while`, so the loop's exit no longer depends on whether a `*` remains in `asName`. Two failure modes: when outer `pos != std::string::npos` (the common case for load directives sourced from a real file), the loop is infinite once the inner `find("*")` returns `npos`; when outer `pos == std::string::npos`, the loop runs exactly once. Path only reached when a user passes a `map = ["...", "...*..."]` argument on a `load`, so the test suite hadn't exercised it. Fix: hoist the find result into its own name (`starPos`) and use it in the condition. *Fixed.*

- [x] **6.** **[lib/circuit.cpp:1274](lib/circuit.cpp#L1274)** — `unknownCountExcludingGround = atUnknown-1;` — `atUnknown` is an unsigned `UnknownIndex` (`uint32_t`); `atUnknown==0` wrapped to ~4.29e9. Fixed with `atUnknown>0 ? atUnknown-1 : 0` — the degenerate "no unknowns at all" case correctly yields a count of 0 instead of wrapping.

- [x] **7.** **[lib/outrawfile.cpp:106](lib/outrawfile.cpp#L106)** — `outStream.seekp(outStream.end);` bound to the one-argument `seekp(pos_type)` overload, seeking to absolute byte 2 (the value of the `end` seekdir enumerator) rather than to end-of-stream. The bug was the call arity, not the `end` spelling. Fixed to the two-argument form `outStream.seekp(0, std::ios_base::end);`. After rewriting the point count in `epilogue()`, the put pointer is now correctly returned to end-of-file before close.

- [x] **8.** **[lib/filestack.cpp:170-178](lib/filestack.cpp#L170-L178), [224-229](lib/filestack.cpp#L224-L229)** — Off-by-one in `addRpnString`: `return -(rpnStringStack.size()+1)` returned `-2` for the first string, but `cString` decodes negative ids as `rpnStringStack[-id-1]`, so `-2` → `rpnStringStack[1]` (OOB; only `[0]` exists). Encoding intent confirmed with author: file ids start at 0 (first file = toplevel netlist), string ids start at -1. Fixed to `return -static_cast<FileStackFileIndex>(rpnStringStack.size());` so the first string → -1 → `rpnStringStack[0]`. **Note: dormant code** — `addRpnString` has no callers and `rpnStringStack` is never populated/read on any live path; the whole negative-id / RPN-string subsystem is unwired scaffolding (kept intentionally per repo policy). Fix is correctness-for-future-use only, zero runtime impact today.

- [x] **9.** **[lib/parser.cpp](lib/parser.cpp), [include/parser.h](include/parser.h)** — Each of `parseNetlistString` / `parseExpression` / `parseParameters` had a `const std::string&` overload plus a `const std::string&&` overload. The rvalue overloads' `stream.str(std::move(input))` was a fake move (`std::move` on a `const` rvalue copies), so they were no faster than the lvalue versions despite the comment; the duplicated bodies had also drifted (`parseNetlistString(const std::string&&)` was missing the `acctNew.parse++` its lvalue twin had). Note: because callers pass string *literals*, the rvalue overloads were the ones actually selected. Fixed (Option C) by collapsing each pair into a single **by-value** overload `parseX(std::string input, ...)`: `addStringFile(input)` makes the unavoidable file-stack copy, then `stream.str(std::move(input))` genuinely moves into the parse stream. Same copy count as a correct move overload, half the functions, no duplication, single home for the `parse` counter. `addStringFile` always copies into `stringsPool`, so no further move is possible there.

- [x] **10.** **[simulator/main.cpp:29](simulator/main.cpp#L29) vs [109](simulator/main.cpp#L109)** — Help text advertised `--extratomlfile` (no hyphen) but the parser only accepted `--extra-tomlfile`, so the documented switch was untriggerable. Fixed in working tree: help text now reads `--extra-tomlfile`, matching the parser.

## Resource / API misuse

- [ ] **11.** **[lib/klumatrix.cpp:119](lib/klumatrix.cpp#L119), [128](lib/klumatrix.cpp#L128), [174](lib/klumatrix.cpp#L174)** — `KluMatrixCore::rebuild()` calls `this->~KluMatrixCore()` to wipe state and then continues using `this`. Reuse-after-destruction works only because the destructor sets all pointers to `nullptr`, but the pattern is fragile (any field that the dtor relies on being default-constructed will misbehave), and the explicit destructor call is invoked again on the same object inside an error path before construction finishes.

- [ ] **12.** **[lib/klumatrix.cpp:127-131](lib/klumatrix.cpp#L127-L131), [173-177](lib/klumatrix.cpp#L173-L177)** — `if (!AP || !AI)` / `if (!Ax)` checks are dead: `operator new[]` throws on failure rather than returning null. Either use `new (std::nothrow)` or remove the checks.

- [ ] **13.** **[lib/klumatrix.cpp:154-164](lib/klumatrix.cpp#L154-L164)** — Loop fills `AP[col]` only when a transition into a new column is seen. If any column has no entries at all, the corresponding `AP[c]` is left uninitialized, and `klu_factor` reads garbage. In current use every column gets a diagonal from `buildSparsityAndStates()`, but the matrix builder must not depend on that invariant silently. After the loop, fill `AP[i]` for all `i <= AN` not yet populated.

- [ ] **14.** **[lib/klumatrix.cpp:591](lib/klumatrix.cpp#L591), [596](lib/klumatrix.cpp#L596), [598](lib/klumatrix.cpp#L598), [643](lib/klumatrix.cpp#L643), [687](lib/klumatrix.cpp#L687), [692](lib/klumatrix.cpp#L692), [712](lib/klumatrix.cpp#L712)** — `dumpSparsity`, `dump`, and `dumpVector` take an `std::ostream& os` but write to `std::cout` in several places (`std::cout << "\n";`, `std::cout.copyfmt(...)`, etc.). The `os` parameter is partially ignored; output and formatting bleed onto stdout regardless of the requested stream.

- [ ] **15.** **[lib/pool.cpp:102](lib/pool.cpp#L102)** — `CStringPool::dump(std::ostream& os, ...)` prints the string count to `std::cout` instead of `os`.

- [ ] **16.** **[lib/pool.cpp:42-49](lib/pool.cpp#L42-L49)** — In `allocate()`, if the freshly-allocated `newBlock` cannot satisfy the request (allocation inside `newBlock` fails), the function silently returns `nullptr`. Callers in this codebase don't check for null, so this becomes a hard-to-trace crash later.

- [ ] **17.** **[lib/outrawfile.cpp:33](lib/outrawfile.cpp#L33)** — `std::asctime(std::localtime(&t))` — both functions return pointers to static buffers and are not thread-safe. Even ignoring threads, `localtime` is deprecated in C++20+ in favour of `localtime_s/_r`.

## Memory and pointer safety

- [ ] **18.** **[lib/sourceloc.cpp:53](lib/sourceloc.cpp#L53)** — `fileStack->canonicalName(fileId)` is dereferenced without checking whether `fileStack` is null. `fileStack` comes from `FileStack::lookup(...)` (via `data()`); when the stack has been destructed (`badFileStackId` or unmapped id), `lookup` returns `nullptr`. The early guard `if (entry.fileStackId_!=FileStack::badFileStackId)` is much later ([line 92](lib/sourceloc.cpp#L92)) — the line-extraction at [lines 53-62](lib/sourceloc.cpp#L53-L62) runs first.

- [ ] **19.** **[lib/sourceloc.cpp:99-100](lib/sourceloc.cpp#L99-L100)** — When `fileStack->isString(fileId)` is true the function returns the bare string `"<string input>"` and discards all the formatted context already accumulated in `s`. Callers receive a 14-char message instead of the diagnostic they constructed.

- [ ] **20.** **[lib/comdata.cpp:122-134](lib/comdata.cpp#L122-L134)** — `enumerateNatures()` resizes the four `*_natureIndex` vectors to `n+1` but loops only `i<n`, leaving index `n` default-initialized. Either resize to `n` or extend the loop to `i<=n` (the rest of the code keys off `unknown_natureId.size()`).

- [ ] **21.** **[lib/an.cpp:350-380](lib/an.cpp#L350-L380)** — Inside the `switch (state)` block, only `case CoreState::Finished` sets `bool exitLoop`. `case Aborted`, `Stopped`, and `default` `co_yield` without a `break`. If a caller ever resumed the coroutine after yielding Aborted/Stopped/Default (as happens for `default`), execution would fall through to the next `case` body. Today this works only because `Analysis::run()` doesn't resume after a non-Finished yield, but adding a `break;` after each yield is the safe form.

- [ ] **22.** **[lib/an.cpp:549](lib/an.cpp#L549), [556](lib/an.cpp#L556)** — Returning a stale `lastCoroutineState` from `resume()` after `coroutine_.done()` and using it for state-driven cleanup in `finish()` is OK only when callers honour the contract. Worth a defensive check.

- [ ] **23.** **[simulator/cmd.cpp:549](simulator/cmd.cpp#L549)** — `evaluateArgs(cmd, interpreter.variableEvaluator(), args, s);` — the return value is ignored. If expression evaluation fails the function silently proceeds with whatever was populated, masking the error in `s`.

- [ ] **24.** **[lib/value.cpp:94-113](lib/value.cpp#L94-L113)** — `Value::convert()` calls `dest.~Value()` then `switch (to) { ... }`. The switch handles `String/IntVec/RealVec/StringVec/ValueVec` but not `Int/Real`. The unconditional `dest.type_ = to;` after the switch covers the type field, but the path leaves a small contract risk: any future scalar type that needs allocation must be added in both places.

- [ ] **25.** **[lib/status.cpp:51-58](lib/status.cpp#L51-L58)** — `Status::prefix(const std::string& msg)` *replaces* `message_` with an empty string when `msg.size()==0`. The intent seems to be "if msg is empty do nothing", which would be `if (msg.size()>0) { message_ = msg + "\n" + message_; }`. As written, calling `prefix("")` wipes the status message.

## Pre-existing typos / wrong literals

- [x] **26.** **[simulator/cmd.cpp:708](simulator/cmd.cpp#L708)** — `"Number of unknonws:"` is misspelled (should be `unknowns`). *Fixed in working tree.*

- [x] **27.** **[lib/coretran.cpp:949](lib/coretran.cpp#L949)** — Comment misspells `contribution` as `cointribution`. Doc-only but appears in user-facing debug paths. *Fixed in working tree.*

- [x] **28.** **[simulator/main.cpp:312-313](simulator/main.cpp#L312-L313)** — `interp.setPrintProgress(progress),` ends with `,` instead of `;`. Compiles (comma operator), but is almost certainly a typo. *Fixed in working tree.*

- [x] **29.** **[lib/an.cpp:618](lib/an.cpp#L618)** — `s.set(Status::AbortRequested, "...Coroutine exited without yielding Aborted or Finished.")` — wrong status code (this is an *internal* error, not an abort request). Caller logic that branches on `AbortRequested` will treat genuine bugs as user aborts. *Fixed in working tree: `InternalError` added to [include/status.h](include/status.h) enum and used here.*

## Header / API hazards

- [x] **30.** **[include/coretrannr.h:95](include/coretrannr.h#L95)** — Not a present-tense defect: today's single-threaded use is correct. Comment rephrased in working tree to a pre-OpenMP audit gate covering shared mutable scratch state in `TranNRSolver`, rather than a narrow "make these thread-local" instruction tied to two specific fields. To be revisited when parallel evaluation is enabled.

- [x] **31.** **[lib/devvisrc.cpp:114](lib/devvisrc.cpp#L114) (+ 5 similar in [lib/devctlsrc.cpp](lib/devctlsrc.cpp))** — `static ParameterIndex principalXxx = std::get<0>(Introspection<...>::index("..."));` paired with `principalParameterIndex()` methods that returned `std::make_tuple(principalXxx, true)`. Two real problems: (a) the file-scope static relied on `instantiateIntrospection(...)` running earlier in the same TU — an implicit ordering invariant; (b) the `bool found` flag from `index(...)` was discarded and replaced with a hard-coded `true`, so a typoed/renamed parameter name silently became index 0 (a valid index for a different parameter). Fixed by inlining the lookup as a function-local static inside each `principalParameterIndex()` and returning the resulting tuple directly — same shape as the method's return type, so no destructure/reconstruct, and the "found" bit is propagated honestly. Found a copy-paste comment `// gain` on the mutual case ([devctlsrc.cpp:842](lib/devctlsrc.cpp)) where the actual parameter is `"k"`; removed.

- [x] **32.** **[lib/filestack.cpp:184-230](lib/filestack.cpp#L184-L230)** — Encoding (per author): `id >= 0 && id < stack.size()` is a file entry, `id < 0` is the strings namespace (`rpnStringStack[-id-1]`), and `id == badFileId = numeric_limits<FileStackFileIndex>::max()` is the "no such file" sentinel. The accessors (`fileName`, `sectionName`, `canonicalName`, `parentId`, `inclusionLine`, `cString`) only guarded `id >= 0` before indexing `stack[id]`, so forwarding `badFileId` (or any stale positive past `stack.size()`) into any of them dereferenced `stack[oob]`. Fixed by introducing `FileStack::isFileEntry(id)` ([include/filestack.h](include/filestack.h)) returning `id >= 0 && static_cast<size_t>(id) < stack.size()` and replacing the `id >= 0` checks in the five Group-A accessors and the `cString` Group-B accessor; `cString` additionally `DBGCHECK`s and returns `""` for an out-of-range positive id (preserves "no nullptr" caller invariant).

## Performance / minor correctness

- [ ] **33.** **[lib/value.cpp:167-170](lib/value.cpp#L167-L170)** — `*this = std::move(IntVector(0));` — the `std::move` of an unnamed temporary is redundant (the temporary is already an rvalue) and can suppress copy-elision. Repeated for `RealVec/StringVec/ValueVec`.

- [ ] **34.** **[lib/rpnbuiltin.cpp:64](lib/rpnbuiltin.cpp#L64), [67](lib/rpnbuiltin.cpp#L67), [70](lib/rpnbuiltin.cpp#L70), [293](lib/rpnbuiltin.cpp#L293), [296](lib/rpnbuiltin.cpp#L296)** — Same redundant `std::move` of unnamed temporaries (`std::move(IntVector(n, ...))`, etc.). Drop the move.

- [ ] **35.** **[lib/rpneval.cpp:231](lib/rpneval.cpp#L231)** — `if (!(branch && keepOnBranch || !branch && keepOnNoBranch))` — relies on the unintuitive precedence `&& > ||`. Parenthesise explicitly to silence reader/compiler ambiguity warnings.

- [ ] **36.** **[lib/outrawfile.cpp:96](lib/outrawfile.cpp#L96)** — Non-binary, non-complex point output adds a trailing `"\n"` after the last value's own `"\n"`, producing an extra blank line per timepoint. Probably intentional (point separator) but worth confirming against the rawfile spec.

## Build / sanity

- [x] **37.** **[lib/circuit.cpp:376-380](lib/circuit.cpp#L376-L380)** — Misdiagnosis in the original entry. The code compiles cleanly because there is a same-name `pos` in an enclosing scope (the structured binding at [line 294](lib/circuit.cpp#L294)) that the `while` condition silently binds to — see corrected analysis in #5. Not a permissive-compiler/stale-object issue. *Cross-codebase search performed: all 10 do-while loops and 26 while loops were scanned for the inner-`auto`-shadows-while-cond-variable pattern; this is the only instance.* Resolved together with #5.

## Lexer ([lib/dfllexer.l](lib/dfllexer.l))

- [ ] **38.** **[lib/dfllexer.l:220-225](lib/dfllexer.l#L220-L225)** — The `<QUOTED>\n` rule consumes a newline (matches `\n`) but never calls `loc->lines()`. Line numbering goes stale from the first stray newline-inside-string onward. The mirror rule for *escaped* newline at [253-259](lib/dfllexer.l#L253-L259) does call `loc->lines()` — be consistent.

- [ ] **39.** **[lib/dfllexer.l:562-609](lib/dfllexer.l#L562-L609)** — SI-prefix recognition is case-sensitive in a way the regex isn't. The regex matches `meg`/`mil` at any case (since `[munpfakKMGTxX]` accepts `m`, then `[a-zA-Z_]*` swallows the rest), but the switch arm only differentiates `meg`/`mil` when the prefix character is lowercase `'m'`. Inputs like `MEG`, `Meg`, `MIL`, `Mil` therefore fall into the `'M':` case and silently become `1e6`, so `1MIL` parses as one million instead of 25.4 µm. Either lower-case the prefix string before dispatching, or add explicit cases for `'M'` with `eg`/`il` suffixes.

- [ ] **40.** **[lib/dfllexer.l:519](lib/dfllexer.l#L519), [531](lib/dfllexer.l#L531)** — Integer parsing uses `std::stoi` / `std::stoul`. If `Int` (defined in [include/value.h](include/value.h)) is wider than `int` (the codebase appears to use 64-bit ints elsewhere), then any literal that fits in `Int` but not in `int` throws `out_of_range` and is reported to the user as "out of range" — even though it actually fits. Use `std::stoll` / `std::stoull` and cast to `Int`.

- [ ] **41.** **[lib/dfllexer.l:158-166](lib/dfllexer.l#L158-L166)** — The `INITIAL`-state `.*` rule capturing the title does not strip a trailing `\r`. Other states have an explicit `<...>\r {}` rule at [line 167](lib/dfllexer.l#L167) but `INITIAL` is *not* listed, so on CRLF input the title ends with a stray `\r` character (visible in raw-file headers as garbage).

- [ ] **42.** **[lib/dfllexer.l:491-508](lib/dfllexer.l#L491-L508)** — `inParen` / `inBracket` are bumped on every `(`/`[`/`)`/`]` but never validated. Extra closing tokens drive the counters negative; the subsequent `if (inParen<=0 && inBracket<=0)` newline gate at [line 815](lib/dfllexer.l#L815) then treats balanced parens (`inParen==0`) and overshot parens (`inParen==-3`) the same, so error recovery silently treats the next newline as an end-of-statement even when the user intended a continuation.

- [ ] **43.** **[lib/dfllexer.l:794-810](lib/dfllexer.l#L794-L810)** — Single-quoted identifier rule: regex is `'[^[:space:]']*(?:''[^[:space:]']*)*'` (zero-or-more inside the quotes). The action's comment says "any nonempty alphanumeric sequence" but the pattern accepts `''`, which yields an empty `Id`. Empty `Id` values propagate into name maps and lookup tables and cause "not found" errors far from the source. Require at least one character inside the quotes.

- [ ] **44.** **[lib/dfllexer.l:774-778](lib/dfllexer.l#L774-L778)** — The `<LINESTART>endc` rule unconditionally writes `inControl = false`, even when the parser later rejects this `endc` (e.g. it appears outside a control block). The token state then desynchronises from the syntactic state, so subsequent keywords (`analysis`, `sweep`, `save`) are misclassified as identifiers until the next `control` keyword. Pair the flag flip with the parser's actual acceptance of the `endc`, or reset the flag on `YYerror` recovery.

- [ ] **45.** **[lib/dfllexer.l:415](lib/dfllexer.l#L415)** — `<SECLOOK>^[ \t]*\section[ \t]*`. Flex does not interpret `\s` as a whitespace class in its regex flavour, so `\section` lexes as the literal seven characters `s`,`e`,`c`,`t`,`i`,`o`,`n` — the backslash is dead. Functionally correct but the intent reads as if `\s` were special. Remove the backslash.

- [ ] **46.** **[lib/dfllexer.l:794-810](lib/dfllexer.l#L794-L810)** — Same rule as #43, separate concern: the pattern uses `(?:...)` (PCRE non-capturing group). Stock Flex 2.5/2.6 does *not* honour `(?:...)` and treats `(`, `?`, `:` as literal/operator characters; if the build is currently passing, it's because the surrounding regex happens to accept the misparsed form. Replace with a plain `(...)` group (Flex groups are non-capturing anyway).

- [ ] **47.** **[lib/dfllexer.l:298-335](lib/dfllexer.l#L298-L335)** — Include-end handler: on `tables.fileStack().addFile(...)` failure it does `error(...); return YYerror;` but **does not** pop the `INC`/`LINESTART` state pair from the start-condition stack first, so the scanner remains in `INCEND`. The mirror `<LIBEND>\n` handler at [400-406](lib/dfllexer.l#L400-L406) *does* pop. Adjacent inconsistency; subsequent recovery from this `YYerror` runs in the wrong state.

## Parser ([lib/dflparser.y](lib/dflparser.y))

- [ ] **48.** **[lib/dflparser.y:191-203](lib/dflparser.y#L191-L203)** — Operator precedence puts `%right NEG NOT BITNOT` *below* `%right POWER` in the file, i.e., unary minus binds *tighter* than `**`. Consequence: `-2**2` parses as `(-2)**2 = 4`, not `-(2**2) = -4`. Verilog-AMS and Python both give `**` higher precedence than unary minus. Either fix the ladder or document the deliberate deviation prominently — the doc references in [docs/expr-operators.md](docs/expr-operators.md) should agree.

- [ ] **49.** **[lib/dflparser.y:749-758](lib/dflparser.y#L749-L758), [771-781](lib/dflparser.y#L771-L781)** — In `parameter_list`, after `evaluator.evaluate(...)`, the actions execute `auto dump = std::move($1.expr);` (resp. `$2.expr`). Since `$1` / `$2` is already an action-stack temporary about to be destroyed, the named-binding-then-move is a no-op (the Rpn is destroyed either way). Drop the `auto dump = ...;` line.

- [ ] **50.** **[lib/dflparser.y:240-258](lib/dflparser.y#L240-L258)** — `output` rule: `tables.verify(status)` is called only in the `INNETLIST` branch. The `INEXPR` and `INPARAMS` branches accept whatever the parser builds without a verification step. Either run verify on all entry points or comment the asymmetry — today a malformed parameter list (e.g. duplicate keys past the local check) flows out unchecked when invoked via `parseParameters`.

- [ ] **51.** **[lib/dflparser.y:263-287](lib/dflparser.y#L263-L287), [388-402](lib/dflparser.y#L388-L402), [791-829](lib/dflparser.y#L791-L829), [832-847](lib/dflparser.y#L832-L847), [855-857](lib/dflparser.y#L855-L857), [925-936](lib/dflparser.y#L925-L936), [961-968](lib/dflparser.y#L961-L968)** — Almost every reduction does `$$ = std::move(PTSomething(...));`. The constructor result is already a prvalue; `std::move` blocks copy/move elision and is a no-op for assignment. Use `$$ = PTSomething(...);`.

- [ ] **52.** **[lib/dflparser.y:880-890](lib/dflparser.y#L880-L890)** — `savecmd` action: when `$3.size()>2` the action sets a status and `YYERROR`s, but `$3` (savestrlist) has already been built and is left intact on the semantic stack. That's fine for memory (Bison destroys it), but the error message only says "too many arguments" without naming the offending save directive or its location. Use `@1.loc()` (already in scope) in `s.extend(@1.loc())` for actionable diagnostics — `savecmd` is the only error path here without a location extension.

- [ ] **53.** **[lib/dflparser.y:430](lib/dflparser.y#L430)** — `condblock_build` `BLKELSE` action does `$$.add(std::move(Rpn()), std::move(blk), @2.loc());` — `std::move(Rpn())` is a redundant move on a default-constructed prvalue. Same comment as #51, listed separately because this is the *only* form where the empty-Rpn sentinel is intentional — easy to mis-read on review.

- [ ] **54.** **[lib/dflparser.y:46-47](lib/dflparser.y#L46-L47)** — `static Id saveCmd = Id::createStatic("save");` lives inside a `%code requires` block (and thus inside an anonymous-namespace-like region per translation unit). If `Id::createStatic` interns the string, this initialisation order is unspecified relative to other static `Id::createStatic` initialisers across TUs. Move to a function-local static or to a single TU. Same hazard appears wherever `static Id ... = Id::createStatic(...)` is used (cf. bug #31 for `devvisrc.cpp:114`).

- [ ] **55.** **[lib/dflparser.y:412](lib/dflparser.y#L412), [416](lib/dflparser.y#L416), [420](lib/dflparser.y#L420)** — `condblock_build` reductions append to `$$.back()` with no guard that `$$` (`PTBlockSequence`) is non-empty. The grammar guarantees `BLKIF expr NEWLINE` runs first, so `$$` should always have ≥1 element here — but the invariant is implicit. A future grammar tweak that allows a `condblock_build` to fold-in further reductions would break silently. Add a `DBGCHECK` or use a safer accessor.

- [ ] **56.** **[lib/dflparser.y:944-959](lib/dflparser.y#L944-L959)** — `sweeps` rule's recursive form does `$$ = std::move($1);` **before** the duplicate-name check. If the check fails (`!inserted`), the action calls `YYERROR` after that move, so `$1`'s contents are already gone — subsequent error reporting that reaches for `$1` in the bison auto-generated cleanup gets the moved-from object. In practice Bison only destructs the lvalue slot (which is now `$$`) so memory is fine, but the order is bug-prone. Swap: do the insert check first, only then `$$ = std::move($1)`.

- [ ] **57.** **[lib/dflparser.y:626-636](lib/dflparser.y#L626-L636)** — Function-call rule packs argument arity from `$3.size()` (a `size_t`) into `Rpn::FunctionCall(..., $3.size())` whose second parameter is `Rpn::Arity`. If `Rpn::Arity` is 8- or 16-bit (check [include/rpnexpr.h](include/rpnexpr.h)) a function call with more than 255/65535 arguments truncates silently. Add a range check, or widen `Rpn::Arity`.

- [ ] **58.** **[lib/dflparser.y:564-572](lib/dflparser.y#L564-L572), [583-591](lib/dflparser.y#L583-L591)** — Branch-offset arithmetic for `&&` and `||` is `$3.size()+(needsConversion?1:0)+2`. The `+2` accounts for "skip OpAnd (or OpOr) + land past it" and depends on `Branch` consuming itself. The relation is correct (traced against [lib/rpneval.cpp:199-258](lib/rpneval.cpp#L199-L258)), but it's brittle — if either `MakeBoolean` or the trailing `Op` ever becomes multi-token, the offsets break silently and the bug surfaces only as wrong values. Either move the arithmetic into a `Rpn::shortCircuitAnd(...)` helper, or assert post-construction that the branch lands on the position after the closing `Op`.

---

Total: **58** items. Mark each box when fixed; remove the entry only after the fix lands in `main`.
