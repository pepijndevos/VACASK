# Bug audit — `feature/behavioral` branch (vs. `main`)

Scope: all commits on `feature/behavioral` not on `main` (25 commits, ~1700 lines changed),
covering the new nonlinear behavioral source feature (`lib/dflparser.y`, `lib/parseroutput.cpp`,
`lib/rpnexprva.cpp`, `include/rpnexpr.h`, `lib/osdiinstance.cpp`, `lib/devbase.cpp`), the
bundled `idt(a,b)` fix (`lib/coretran.cpp`, `lib/corepsstran.cpp`), and the real-number
formatting change (`lib/value.cpp`).

Each item below has been traced against the actual code at `HEAD`, not just the diff.

---

## Crashes / undefined behavior on malformed but syntactically legal netlists

- [ ] **1. A behavioral source with no `v`/`potential`/`i`/`flow` parameter crashes the simulator.**
  `lib/dflparser.y`, `behavioral` rule (~line 869-963): the value-parameter and
  expression-parameter loops set `haveExpr = true` when a `v`/`potential`/`i`/`flow`
  parameter is seen, but nothing after the loops checks that `haveExpr` ended up `true`.
  A line like
  ```
  b1 (a 0) discipline=["thermal", "Temp", "Pwr"]
  ```
  (missing `v=`/`i=`, e.g. because of a typo such as `pv=...`) parses successfully and
  builds a `PTBehavioral` with a default-constructed, **empty** `Rpn` expression.
  `ParserTables::processBehaviorals()` (`lib/parseroutput.cpp:635`) then calls
  `behav.expr().verilogA(...)` unconditionally. In `Rpn::verilogA()`
  (`lib/rpnexprva.cpp:164`), the `for(size_t idx=0; idx<expr.size(); idx++)` loop runs zero
  times for an empty expression, so `sstack` stays empty, and the line
  ```cpp
  auto& [resultExpr, resultIsId, resultIdx] = sstack.back();   // rpnexprva.cpp:458
  ```
  invokes `std::vector::back()` on an empty vector — undefined behavior, in practice a
  crash — instead of the parser reporting "expression missing" the way it already does for
  other malformed behavioral-source lines.

- [ ] **2. `$intparam()`/`$realparam()` called with the wrong arity (in particular zero
  arguments) crashes the simulator.**
  `lib/rpnexprva.cpp:408-414`, inside `Rpn::verilogA()`:
  ```cpp
  if (name==intParamId || name==realParamId) {
      auto& [argText, argIsId, argIdx] = sstack.back();     // read BEFORE validating n
      if (n!=1 || !argIsId) {
          s.set(Status::Unsupported, ...);
          ...
      }
  ```
  `sstack.back()` is read *before* the arity (`n`) is checked. `$intparam`/`$realparam` are
  not special lexer/grammar tokens — they are ordinary identifiers, so
  `$intparam()` (zero arguments) is a syntactically legal function call at parse time; the
  arity mismatch is only supposed to be caught here. If this call is the first thing
  translated in the expression (e.g. the whole behavioral source is
  `b1 (a 0) potential=$intparam()`), `sstack` is still empty at this point, and
  `sstack.back()` is undefined behavior — a crash — instead of producing the
  "`$intparam()` requires a single identifier argument" diagnostic that the very next line
  is trying to produce.

## Silent mistranslation

- [ ] **3. A failed noise-function translation is reported but not treated as failure.**
  `lib/rpnexprva.cpp:422-438`, in the generic function-call handling inside
  `Rpn::verilogA()`:
  ```cpp
  auto [ok, txt] = fit->second(sstack, argPos, n, expr, s);
  if (!ok) {
      s.extend(location(e));      // extends the status message...
  }
  sstack.resize(argPos);
  sstack.push_back({std::move(txt), false, idx});   // ...but txt/ok is used anyway
  break;
  ```
  `noiseTranslator<...>()` (used for `white_noise`/`flicker_noise`, `rpnexprva.cpp:82-101`)
  returns `(false, "")` and sets an error status when the last argument isn't a constant
  string literal, e.g.
  ```
  bwn (n1 0) i=white_noise(pwr, contribname)   // contribname is a parameter, not a literal
  ```
  The `if (!ok)` branch only *extends* the already-set `Status` message; it never
  `return false`s. Execution falls through, pushes the empty `txt` onto the stack anyway,
  and `Rpn::verilogA()` goes on to return `true` at the end. The caller
  (`ParserTables::processBehaviorals()`) treats this as success and writes out a `.va` file
  containing a malformed statement (e.g. `I(br) <+ white_noise(pwr, );`), which then either
  fails later with a confusing raw OpenVAF/Verilog-A compiler error, or in a differently
  malformed expression could silently absorb into something that compiles but computes the
  wrong value — instead of surfacing VACASK's own clear diagnostic at the point of
  translation.

## Unintended global side effect

- [ ] **4. The round-trip real-number precision fix was applied to the shared `Value`
  printer, not scoped to Verilog-A code generation, changing output everywhere.**
  `lib/value.cpp`, `operator<<(std::ostream&, const Value&)`:
  ```cpp
  std::ostream& operator<<(std::ostream& os, const Value& obj) {
      auto oldPrecision = os.precision(std::numeric_limits<double>::max_digits10);
      ...
      os.precision(oldPrecision);
      return os;
  }
  ```
  This was added (commit `a6c86a37`, "Fixed real formatting precision") so that real
  literals embedded into the synthesized Verilog-A source round-trip exactly
  (`Rpn::verilogA()` builds literal text via `Value::str()`, which itself calls this same
  `operator<<`). But `operator<<`/`Value::str()` is the generic formatter used everywhere in
  the codebase — e.g. `PTParameters`'s dump path (`lib/parseroutput.cpp:59`:
  `os << (it->name()) << "=" << it->val() << " "`), which backs `--dump`/`print
  device(...)`/`print model(...)`. Every real-valued parameter printed anywhere in the
  simulator now shows 17-significant-digit round-trip artifacts instead of the previous
  clean, shorter formatting, e.g. `rp=9.9` becomes `rp=9.9000000000000004`. There is no test
  covering `--dump`/`print device`/`print model` numeric formatting, so this regression
  would not be caught automatically. The precision bump should be scoped to the
  Verilog-A-literal code path (e.g. a helper used only from `rpnexprva.cpp`) rather than the
  shared `Value` stream operator.

## Documentation

- [ ] **5. The example in `docs/dev-builtin-behavioral.md` declares `ground 0`, violating
  the project's own documentation convention.**
  `docs/dev-builtin-behavioral.md:222` (`## Example` section):
  ```
  ground 0
  load "resistor.osdi"
  ...
  ```
  `.github/copilot-instructions.md:51` states: "In examples do not declare node `0` as
  ground (it is by default the ground node anyway)." The redundant `ground 0` line should
  be removed from the example.

---

### Not flagged (checked and found consistent)

- `idt(a,b)` fix mirrored from `lib/coretran.cpp` into `lib/corepsstran.cpp`
  (`es.icEnabled = false` added to both `PssTranCore::clearTrajectory()` and
  `PssTranCore::onTimestepAccepted()`): both `EvalSetup` blocks that evaluate/store the
  reactive residual in `corepsstran.cpp` got the fix; consistent with the single analogous
  block in `coretran.cpp`.
- `findPeerInstance`/`findControl` refactor from free template functions
  (`devctlsrc.cpp`) into `Instance` member functions (`devbase.h`/`devbase.cpp`): call
  sites correctly updated, semantics preserved (`*this` implicit instead of an explicit
  argument).
- `OsdiInstance::populateStructuresCore()`'s use of the hardcoded internal node name
  `"flow(br)"` to resolve `i(instance)` control currents matches the pre-existing
  convention already used by CCCS/CCVS (`include/devctlsrc.h`) and `vsource`
  (`lib/devvisrc.cpp`), not something invented ad hoc for this feature.
- `OsdiInstance::unbindInternalNodes()`: dead code after `return false;` was moved out of
  the `if`, which is itself a (pre-existing bug) fix bundled into this branch, not a
  regression.
