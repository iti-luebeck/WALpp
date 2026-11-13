# WAL operator parity: gap analysis and staged plan

Branch `wal-operator-parity`. Goal: let WAL++ run programs written for upstream WAL
(ics-jku/wal), so that published WAL code, including the examples in the WAL papers,
executes without translation.

Measured against **upstream wal-lang 0.9.1** (`wal/ast_defs.py` operator enum plus
`wal/libs/std/std.wal`) and **WAL++ v1.2.2** (146 registered operators, 6 special forms).

## Headline numbers

| | Count |
|---|---|
| Upstream builtins | 107 |
| WAL++ operators + special forms | 151 |
| **Upstream builtins missing from WAL++** | **66** |
| Upstream stdlib functions/macros (written in WAL itself) | ~45 |

WAL++ is not a subset: it has ~90 operators upstream lacks (the whole statistics,
spectrum, Hamming and export families). The gap is concentrated in **language
machinery** rather than analysis operators.

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

## Not in this branch

The `-e` printer fix and `-e` accumulation (shared `value_printer.h`, plus three
regression tests) are uncommitted on `github-release` and are **not** in this worktree,
which was branched from committed HEAD. Development here should use explicit
`(print ...)` for list results, or those commits should be landed first.
