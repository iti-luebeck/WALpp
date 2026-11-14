# WAL operator parity: gap analysis and staged plan

Branch `wal-operator-parity`. Goal: let WAL++ run programs written for upstream WAL
(ics-jku/wal), so that published WAL code, including the examples in the WAL papers,
executes without translation.

Measured against **upstream wal-lang 0.9.1** (`wal/ast_defs.py` operator enum plus
`wal/libs/std/std.wal`) and **WAL++ v1.2.2** (146 registered operators, 6 special forms).

## Headline numbers

| | At branch start | Now |
|---|---|---|
| Upstream tests passing | 18 of 77 | **67 of 77** |
| Upstream operators missing from WAL++ | 66 of 106 | **16 of 106** |
| Open decisions | 6 | **3** |
| Upstream standard library | not loaded | loaded |

WAL++ is not a subset: it has ~90 operators upstream lacks (the whole statistics,
spectrum, Hamming and export families). The gap was concentrated in **language
machinery** rather than analysis operators, which is why closing it was mostly a
matter of connecting pieces that already existed.

A shareable summary of this document is published at
https://claude.ai/artifact/41NFwsPgipgr4Rxet8XKTH (visible only to people it has
been shared with).

## Verified against a real upstream install

Upstream wal-lang 0.9.1 is installed in a throwaway venv and used as the reference
implementation. Every claim below was executed, not inferred. This also gives the project
its testing strategy: see "Differential testing" at the end.

### Finding 1: the real gap is the signal access model, not the operator list

This dominates everything else and does not appear in the operator diff at all, because it
is not an operator.

**Upstream exposes signals as ordinary variables**, with an `@` offset for relative time:

```
wal -l counter.vcd -c '(do (set-index 40)
    (printf "counter=%d clk=%d clk@-1=%d\n" tb.dut.counter tb.clk tb.clk@-1))'
=> counter=15 clk=0 clk@-1=1
```

WAL++ requires an explicit accessor and a different separator:

```
wal -l counter.vcd -e '(print (begin (set-index 40) (get-signal "tb^dut^counter")))'
=> 15
```

Same value, same trace, completely different surface syntax. Until a bare symbol that
names a signal resolves to that signal's current value, **no published WAL program runs
verbatim**, however many operators get added. The evaluator needs a resolution fallback:
unknown symbol -> look it up as a signal name (dotted), with `@<int>` as a relative-index
offset. This is the highest-value single change in the whole project.

### Finding 2: `step` must report that it could not advance

Upstream returns true while it advances and `False` at the end:

```
wal -l counter.vcd -c '(do (set-index MAX-INDEX) (printf "at=%d step=%s\n" INDEX (step 1)))'
=> at=80 step=False
```

WAL++ clamps and returns `1` forever:

```
wal -l counter.vcd -e '(print (begin (set-index 79)
    (list (step 1) (get-index) (step 1) (get-index) (step 1) (get-index))))'
=> (1 80 1 80 1 80)
```

So `(while (step 1) ...)`, the standard WAL iteration idiom and line 6 of the
agentic-WAL paper's Listing 1, would spin forever on WAL++. This is a **behavior change
to an existing operator** and the one place where parity work touches current semantics.
It is also arguably a bug in its own right, in the same silent-clamping family as the
`set-index` no-op stub.

Note: reading `wal/trace/container.py` alone suggests the opposite convention, because the
container returns a list of *ended* traces. The evaluator inverts that into a boolean. The
executed behavior above is authoritative.

### Finding 3: the paper's Listing 1 is correct, and both implementations agree

Running the paper's running example unmodified on upstream, against `counter.vcd`:

```
wal -l counter.vcd -c '(do (define cnt 0)
    (while (step 1) (when (&& tb.clk (! tb.clk@-1)) (set! cnt (+ cnt 1))))
    (printf "rising edges=%d\n" cnt))'
=> rising edges=40
```

The equivalent in today's WAL++, which needs `filter` over an explicit range because it
has no `while`, no `when`, no bare-signal access and no `@`:

```
wal -l counter.vcd -e '(print (length (filter (lambda (i) (begin (set-index i)
    (&& (= 1 (get-signal "tb^clk")) (= 0 (get-signal "tb^clk" -1)))))
    (range 1 81))))'
=> 40
```

Both give 40. That makes this pair the first conformance test: same question, same trace,
same answer, and the WAL++ side should eventually be writable in the upstream form.

### Finding 4: special variables and scopes exist upstream

`INDEX` = 0, `MAX-INDEX` = 80, `SIGNALS` returns a list of dotted names, `SCOPES` returns
`("tb" "tb.dut")`. In WAL++ every one of these errors with *Undefined variable*.
`MAX-INDEX` also closes a genuine functional gap: WAL++ has **no trace-length operator at
all**, so the timestamp count currently has to be smuggled out via
`(length (hamming-weight-scope "tb"))`, which is obscure and scans every signal.

### Finding 5: parser work is needed, not just operators

`sig@-1`, the `#sig` scope sigil, and the special variables are all lexical. They cannot
be added as operators. `src/parser.cpp` is 277 lines, hand-written around a `peek()` loop,
and already handles quote, quasiquote and unquote, so this is tractable.

## The 66 missing builtins, by category

**Control flow and binding** (10) — `let` `do` `while` `case` `fn` `set` `for` `exit`
`printf` `in`

**Macros and metaprogramming** (8) — `defmacro` `macroexpand` `gensym` `quasiquote`
`unquote` `eval` `parse` `reval`

**Type predicates and conversions** (12) — `atom?` `defined?` `int?` `list?` `string?`
`symbol?` `convert/bin` `string->int` `bits->sint` `string->symbol` `symbol->string`
`int->string`

**Naming compatibility** (4) — `mod` (WAL++ has `%`), `average` (has `mean`), `second`,
`last`

**Arrays, a new data type** (5) — `array` `seta` `geta` `dela` `mapa`

**Aliases** (2) — `alias` `unalias`

**Scopes and groups** (9) — `all-scopes` `in-scope` `resolve-scope` `set-scope`
`unset-scope` `groups` `in-group` `in-groups` `resolve-group`

**Signal and trace analysis** (11) — `find` `find/g` `whenever` `fold/signal`
`sample-at` `trim-trace` `defsig` `new-trace` `dump-trace` `loaded-traces` `get`

**Module and host integration** (5) — `require` `import` `eval-file` `call` `repl`

## Staged plan

Tiers are ordered by value per unit of risk. Each tier should land with tests and leave
the tree green.

### Tier 1 — make published WAL programs run verbatim (the critical path)

Ordered by dependency. The paper's Listing 1 is the acceptance test for the whole tier.

1. **Bare-signal resolution** (Finding 1). Unknown symbol resolves to a signal value;
   accept dotted names alongside `^`. Touches the evaluator's symbol lookup.
2. **`@` offset notation** (Finding 5). `sig@-1` lowers to the existing
   `(get-signal name -1)` capability. Parser only.
3. **`step` returns falsy at the end** (Finding 2). Unblocks every `while` loop.
4. **Special variables** `INDEX`, `TS`, `MAX-INDEX`, `SIGNALS` (Finding 4), with
   `MAX-INDEX` backed by a real trace-length accessor.
5. **`while`, `do`, `printf`**, and `alias`/`unalias`.
6. **`when`, `unless`, `cond`** as native special forms. These are **not** upstream
   builtins; upstream defines them as macros in `std.wal`. WAL++ has no macro system, so
   adding them natively now is the pragmatic choice, and a deliberate divergence from
   upstream's structure that should be recorded.

After this tier, Listing 1 should run character for character and return 42 on an AXI
trace, and 40 on `counter.vcd` in the rising-edge form above.

### Tier 2 — the cheap bulk (low risk, mechanical)

Type predicates, conversions, naming-compatibility aliases, `let`, `case`, `cond`, `fn`,
`exit`, `in`, `second`, `last`. Almost all are pure functions over existing `Value`
types, testable in isolation. This is where the operator count moves most per hour.

### Tier 3 — the distinctive WAL features (medium risk, needs design)

Scopes and groups (9 operators plus `CS`/`CG`/`SCOPES`), and the signal-analysis
operators `find`, `whenever`, `fold/signal`, `sample-at`. These depend on the trace
hierarchy and on scope state in the evaluator, so they need a design pass first. They
are also what makes WAL feel like WAL rather than a Lisp with a VCD reader.

### Tier 4 — subsystems (large, independent)

The macro system (`defmacro`, `macroexpand`, `gensym`, quasiquote/unquote evaluation) and
the array type (`array`, `seta`, `geta`, `dela`, `mapa`). Each is a self-contained
project. The macro system also unlocks porting upstream's ~45-function stdlib, which is
written in WAL itself: once macros work, `std.wal` can largely be adopted rather than
reimplemented in C++.

### Tier 5 — host and module integration

`require`, `import`, `eval-file`, `call`, `repl`, `defsig`, `new-trace`, `dump-trace`.
Lowest priority for analysis work, and `new-trace`/`dump-trace` overlap with WAL++'s
existing writers.

## Honest scope note

"All operators from WAL" is 65 builtins plus a macro system, an array type, a scope
subsystem and ~45 stdlib functions. That is a multi-week project, not a single sitting.
Counting only upstream builtins, Tier 1 is 5 of the 66 and Tier 2 is 23, so the two
together are 28. They nonetheless carry most of the practical compatibility benefit,
because the remaining 38 are concentrated in subsystems (scopes, macros, arrays) that
published analysis snippets use far less often. Tier 4 is where the long tail becomes
cheap: once macros work, upstream's ~45-function stdlib can be adopted rather than
reimplemented.

## Differential testing

Upstream is installed and runnable, so parity does not have to be argued, it can be
measured. Proposal: a harness that runs the same WAL program against upstream `wal` and
WAL++ `wal` on the same trace and compares stdout.

- Fixtures: `tests/original/traces/counter.vcd` for cheap cases, the AES and RISC-V
  traces in `artifacts/traces/` for realistic ones.
- Each new operator lands with a differential case, so "implemented" means "agrees with
  upstream", not "does something plausible".
- Cases where WAL++ should deliberately differ (its own statistics, Hamming and export
  families, which upstream lacks) get marked as WAL++-only rather than quietly skipped.
- The harness should skip cleanly when upstream is not installed, so it does not become a
  hard build dependency.

This is the only way "all operators from WAL" ends in something checkable. It also pays
for itself immediately: the pair in Finding 3 is already a passing conformance case.

## Progress: Tier 1 items 1 and 2 are done

Implemented in `include/wal/signal_resolve.h` plus the evaluator and parser:

- **Bare-signal resolution.** A symbol that is not a bound variable now resolves to a
  signal. Variables always win, so no existing program changes meaning.
- **`@` offset notation.** `tb.clk@-1` desugars in the parser to the accessor WAL++
  already had, `(get-signal "tb.clk" -1)`. An integer or an identifier is accepted after
  `@`, so `sig@INDEX` will work once `INDEX` exists.
- **`get-signal`, `signal?` and `signal-width`** accept every spelling too, which closes
  the dotted-vs-caret trap: `(signal? "tb.dut.counter")` used to return `false` on a
  trace that plainly contained it.

Verified on `counter.vcd`, differentially against upstream:

| | upstream | WAL++ |
|---|---|---|
| `tb.dut.counter` at index 40 | 15 | 15 |
| `tb.clk`, `tb.clk@-1` at index 40 | 0, 1 | 0, 1 |
| rising clock edges over the trace | 40 | 40 |

All 21 pre-existing tests still pass.

### The `^` conflict, and how it is handled for now

Upstream spells a signal `[tid^]dotted.path[@offset]`: `^` separates a **trace id**, and
`.` joins the hierarchy. Confirmed both ways: `a^tb.dut.counter` works after loading
under id `a`, and `tb^dut^counter` is an error upstream.

WAL++ uses `^` to join the hierarchy, and its own `--fast-*` modes use `.`. So the three
conventions disagree, and WAL++ disagrees with itself.

The resolver accepts all of them rather than picking a winner, so nothing breaks today.
**The real decision is still open:** converging on upstream's meaning of `^` would make
the interpreter consistent with upstream *and* with WAL++'s own fast paths, at the cost
of breaking any script or Python caller using `^` as a hierarchy separator. That is a
release-gated choice, not a code change to make quietly.

## Upstream conformance: a measured baseline

`tests/original/` turned out to be upstream's test suite vendored verbatim. Two things
were wrong with it:

- `tests/original/wal/` calls itself a shim but is a **~690-line Python interpreter** of
  its own (`_eval_define`, `_eval_let`, `_eval_lambda`, ...). It `CDLL`s `libwalpp`,
  warns if that fails, and evaluates in Python regardless. Its own README reports
  "18/30 tests passing". So passing those tests said nothing about WAL++.
- They were **not registered with CTest**, so they never ran at all.

`tests/conformance/` now provides a real adapter: upstream's `from wal.core import Wal`
resolves to WAL++'s pybind11 module, with values converted to Python natives.
`run_conformance.py` runs the upstream files against the actual engine and groups the
failures into a to-do list. It is wired into CTest as a **ratchet**
(`WALPP_CONFORMANCE_MIN_PASS`, currently 18): it fails only if *fewer* tests pass than
the baseline, so parity cannot regress and the number can only be raised.

**Baseline: 18 of 77 upstream tests pass.** `test_parser.py` and `test_varresolve.py`
are excluded, since they assert on upstream's lark parse tree and its internal
resolution pass rather than on behavior.

The failures confirm the tier ordering, and add names the operator diff missed because
they are special variables rather than builtins:

```
 6x MAX-INDEX    3x set     3x do      2x alias    2x fn      2x groups
 2x LOCAL-SCOPES 2x in-scope 2x SCOPES 2x SIGNALS  2x TRACE-NAME
 1x each: when unless cond printf whenever find second in array seta geta
          atom? int? list? string? symbol? convert/bin int->string string->int
          symbol-add
```

`LOCAL-SCOPES` and `TRACE-NAME` were not in the 66-builtin diff; the special-variable
set is larger than first probed and needs its own enumeration.

### Semantic divergences, not just missing names

Four failures are genuine behavior differences and deserve decisions of their own:

- `(+ 1 "hi")` concatenates upstream; WAL++ raises *Expected number, got "hi"*.
- `(!= 3 3 3)` is n-ary upstream; WAL++ requires exactly 2 arguments.
- Three assertion mismatches (`True != False`, `False != 1`, `2 != 3`) are unexplained
  so far and should be investigated individually before any operator is added.

These matter more than the missing-name count: an absent operator fails loudly, while a
differing one returns a plausible wrong answer.

## Incidental findings

- **`include/wal/operators/trace_operators.h` is dead code.** Only
  `trace_management_operators.h` is included by the evaluator, yet both register the same
  ten trace operators. Editing the wrong one costs a build cycle to notice. It should be
  deleted or the duplication resolved.
- **The `print` operator and the `-e` echo disagree on booleans**: `print` renders
  `#t`/`#f`, the `-e` result printer renders `true`/`false`. Minor, but it is the same
  two-printers problem as the list-echo defect.

## Tier 1 and most of Tier 2 are done

Conformance went 18 -> 22 -> 37 of 77 upstream tests. All 22 CTest tests pass.

### A correctness bug found on the way: zero was true

`Value::is_truthy` returned true for everything that was not nil or a bool, so the
integer 0 was true. Signal values are integers, so this was not a nicety:

```
(if 0 THEN ELSE) -> THEN      (! 0) -> false
(&& 1 0)         -> true      (|| 0 0) -> true
```

and `(if tb.clk 'HIGH 'LOW)` answered `'HIGH` with the clock low. The damage was
silent: counting rising edges the idiomatic way, `(&& tb.clk (! tb.clk@-1))`, returned
**0** instead of 40, while the explicit `(&& (= 1 tb.clk) (= 0 tb.clk@-1))` returned 40.
Rules pinned against upstream: 0, 0.0, "", () and #f are false.

### Three more "built but not connected" cases

The pattern from the `set-index` stub and the `-e` printer kept repeating:

- **Special variables.** `Trace::get_special_signal` already implemented `INDEX`,
  `MAX-INDEX`, `TS`, `SIGNALS`, `SCOPES`, `TRACE-NAME` and `TRACE-FILE`. Nothing called
  it from the evaluator, so all of them errored. `MAX-INDEX` also closes a real gap:
  there was no way to ask how long a trace is.
- **`step`'s boundary.** `Trace::step` returned true/false correctly;
  `TraceContainer::step` called it, discarded the bool and returned `""`, so `step` was
  always truthy and `(while (step 1) ...)` could not terminate.
- **Multi-form scripts.** `Wal::eval_str` parses *one* expression, and `-f`, `--stdin`
  and `--pipe` all went through it, so a script file silently ran its first form and
  stopped. `Parser::parse_program`, which returns every form, already existed and was
  unused. Added `Wal::eval_program` and pointed the REPL at it.

### Added

- Control flow: `when`, `unless`, `while`, `cond`, `case`, `let`, plus `do` and `fn` as
  spellings of `begin` and `lambda`. A multi-form lambda body is wrapped in an implicit
  `begin`, as upstream allows.
- `printf` with full `%[flags][width][.precision]conv` specs.
- `alias` / `unalias`, consulted by both symbol resolution and `get-signal`, so an
  aliased name works with an `@` offset too.
- Predicates `int? float? string? symbol? list? bool? nil? atom? defined?`.
- Conversions `string->int`, `int->string`, `symbol->string`, `string->symbol`,
  `convert/bin`.
- `mod`, `average`, `second`, `last`, `in`, `exit`.
- `[...]` as list delimiters, which upstream uses for binding and parameter lists.
- **Inline application**: `((fn [x] (* x 2)) 21)`. The evaluator required the head of a
  form to be a symbol, so a lambda had to be named with `define` before it could be
  called. That was a pre-existing limitation, not a parity item.

### The paper's Listing 1 now runs unmodified

```lisp
(alias clk "tb.clk")
(define cnt 0)
(while (step 1)
  (when (&& clk (! clk@-1))
    (set! cnt (+ cnt 1))))
(printf "Total rising edges: %d\n" cnt)
```

`wal -l counter.vcd -f listing1.wal` prints `Total rising edges: 40`, and upstream
prints the same for the same file.

### Tests whose expectations were wrong

`test_trace_operators.cpp` and `test_trace_script_application.cpp` asserted
`(step n) == n` under a section named "Trace Stepping (Stub)". They encoded the stub
contract, so they were updated to the real one rather than worked around.

## What is left

Measured, not estimated: 65 of 77 upstream tests pass, and 16 of upstream's 106
operators are missing.

### Operators still missing (16)

| Group | Operators | Note |
|---|---|---|
| Modules | `import`, `require`, `eval-file` | Loading WAL and Python modules. `import` pulls in a Python module upstream, which has no direct C++ equivalent. |
| Runtime eval | `eval`, `parse`, `call` | Evaluate data as code. The pieces exist (`Parser::parse_program`, `SEval::eval`); only the operators are absent. |
| Trace authoring | `new-trace`, `dump-trace`, `defsig`, `trim-trace`, `sample-at` | Creating and writing traces, not just reading them. |
| Trace queries | `loaded-traces`, `find/g` | Small; `find/g` is `find` across all groups. |
| Scope | `set-scope`, `unset-scope` | The imperative form of `in-scope`, which does work. |
| Conversion | `bits->sint` | Two's-complement reading of a bit string. Wanted by `signed` in the standard library. |

### Decisions, not work

These are behavior changes that need a call rather than an implementation.

1. **The `^` separator.** WAL++'s interpreter uses `^` for hierarchy, its own
   `--fast-*` paths use `.`, and upstream reserves `^` for a trace id. The resolver
   currently accepts all spellings, which hides the question rather than answering
   it. `SIGNALS` and `SCOPES` return `^`-joined names where upstream returns dotted
   ones (2 test failures). This is the only decision that gets harder after the
   first public release.

2. **`(+ 1 "hi")`.** Upstream concatenates, giving `"1hi"`; WAL++ raises
   *Expected number* (3 failures). Adopting upstream's rule is dangerous here
   specifically because `vcd_trace.cpp` returns a BIT STRING for signals wider than
   64 bits, so `(+ wide 1)` would quietly become string concatenation of a bit
   pattern instead of an error. Related: `(= wide 5)` already answers false with no
   error, which is arguably a bug regardless.

3. ~~**Integers wider than 64 bits.**~~ **Settled: 64-bit default, `--bigint` mode.**
   64-bit is what waveform values and indices need and the arbitrary-precision path
   is slower, so it stays the default and says so when a literal overflows. The
   conformance suite runs in bigint mode, since upstream's integers are unbounded.

4. **`0 == #f`.** Upstream inherits Python's `0 == False`; WAL++ treats an int and a
   bool as different types. Visible at a trace boundary: at MAX-INDEX `reval` is out
   of range and answers false, so upstream counts `(stable clk)` as 1 where WAL++
   counts 0.

5. ~~**Standard-library names that collide with builtins.**~~ **Settled for
   `append`.** WAL++'s list concatenation is now `extend`, so `append` carries
   upstream's meaning of adding one element. Both are verified against upstream. The
   loader still skips the rest, which remain equivalent enough not to matter:
   `car`, `cdr`, `cond`, `filter`, `reverse`, `set-index`, `set!`, `sort`, `sum`,
   `symbol-add`, `unless`, `when`.

6. ~~**SV array element naming.**~~ **Settled: `data<0>`, as upstream spells it.**
   This stopped being optional when subscript syntax arrived: `[` then opened a
   subscript, so `TOP.tb.data[0].data.something` read as the symbol `TOP.tb.data`
   subscripted by 0 and a bracketed element could not be named at all. Upstream hit
   the same ambiguity and solved it the same way. The old bracketed spelling still
   resolves through `get-signal`.

### What settling 3, 5 and 6 did not touch

Nothing in the paper. The paper contains no code listing, no signal name and no
separator: its only mention of the language is that upstream WAL "introduced a
Lisp-like S-expression language". Its measurements come from the `--fast-*` paths,
which use dotted names and never touch the interpreter. Re-verified after each
change: byte-identical extraction output and unchanged per-run time.

### Not parity gaps (5 failures)

- 2 need upstream's internal `Operator` enum, which WAL++ has no equivalent of.
- 2 compare printed output, which Python cannot capture from C++ stdout.
- 1 is a typo in upstream's own suite: `test_overflow` reads
  `(count tb.overflow` with no closing parenthesis.

These five can never pass and should be excluded permanently rather than counted
against parity.

## Silent wrong answers found and fixed

Each of these returned a plausible value with no error, which is why none had been
noticed. All are covered by regression tests verified to fail against the previous
binary.

| Defect | What it did |
|---|---|
| `Value::is_truthy` returned true for everything non-nil, non-bool | integer 0 was true, so `(&& clk (! clk@-1))` counted 0 rising edges instead of 40 |
| `Value::operator==` compared only the variant tag for reference types | `(= (list 1 2) (list 1 3))` was true for ANY two lists, which also broke `in` and `!=` |
| The FST reader skipped FST-flagged aliases and kept the bit range in a name | `counter.fst` listed 4 signals where the identical `counter.vcd` listed 7, and `tb.dut.counter` would not resolve against an FST |
| `TraceContainer::step` discarded `Trace::step`'s boundary flag | `(while (step 1) ...)` could never terminate |
| `eval_str` parsed one form, and `-f` used it | a script file ran its FIRST form and stopped |
| `convert/bin` reinterpreted the digits as decimal | disagreed with upstream for every input |

## Branch state

Conformance against upstream WAL 0.9.1's own test suite:

```
 18/77   before this branch (the suite had never actually run against WAL++)
 22/77   zero is false
 37/77   control flow, predicates, printf, aliases
 48/77   arrays, set/get, trace sweeps, structural equality, the FST reader
 61/77   macros, scopes and groups, operators as values
 65/77   the standard library
 67/77   bigint mode, append/extend, data<0> names
```

All 22 CTest tests pass. The ratchet in `tests/CMakeLists.txt` is at 67, so parity
can only go up.

The acceptance test is that published WAL runs unmodified and agrees with upstream:

```lisp
(alias clk "tb.clk")
(define cnt 0)
(while (step 1)
  (when (&& clk (! clk@-1))
    (set! cnt (+ cnt 1))))
(printf "Total rising edges: %d\n" cnt)
```

`wal -l counter.vcd -f listing1.wal` prints 40, as upstream does. So does the
idiomatic one-liner that the standard library provides:

```
wal -l counter.vcd -e '(count (rising tb.clk))'   # => 40
```

## The extraction paths are untouched

The `--fast-hw-*`, `--fast-hd-*`, `--fast-values-signals` and `--fast-list` paths
live in `include/wal/fast/` and `src/fast/`, whose entire include list is
`<string> <vector> <unordered_map> <unordered_set> <cstdint>`. They never construct
a `Value` or an evaluator, so no semantic change on this branch can reach them.

Verified rather than assumed: on the same trace, every one of those paths produces
byte-identical output before and after this branch, and per-run time is unchanged.
The standard library is loaded lazily for the same reason.
