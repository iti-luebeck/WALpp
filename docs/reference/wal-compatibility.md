# Compatibility with upstream WAL

What WAL++ shares with [ics-jku/wal](https://github.com/ics-jku/wal), where it
differs on purpose, and how to turn the differences off.

WAL++ is a C++20 reimplementation of WAL and SCA-WAL. The point of it is throughput;
the point of this page is that nothing else changed without a reason.

## How this is measured

Upstream's own test suite, run against the WAL++ engine. The test files in
`tests/original/` are vendored verbatim from `ics-jku/wal`, and
`tests/conformance/run_conformance.py` points them at WAL++ through its Python
bindings.

Verbatim is meant literally, and is worth checking rather than trusting: a single
assertion once went missing from `test_eval.py`, and it was the one that catches a
`case` whose `default` is written before a matching clause. `diff -r` against an
upstream checkout is the check.

```bash
cmake -S . -B build -DBUILD_PYTHON_MODULE=ON    # the runner needs the extension
cmake --build build -j
PYTHONPATH=build/lib python3 tests/conformance/run_conformance.py
```

Without the extension built, the runner prints `SKIP: No module named 'wal_cpp'`
and exits 0, so a green run means nothing until you see the count.

**71 of 77 upstream tests pass**, and **0 of upstream's 106 operators are missing**.
A ratchet in `tests/CMakeLists.txt` keeps the number from going down.

The six that do not pass are not behaviors a user meets:

- `test_quote`, `test_first_second_last` assert on upstream's internal `Operator`
  enum. A quoted `'(+ 1 2)` gives a list of symbols here, not enum members.
- `test_print`, `test_printf` compare captured stdout, which Python cannot see from
  a C++ process. The behavior is covered by the executable tests, which read the
  real output.
- `test_overflow` is a missing parenthesis in upstream's own suite.
- `test_seta` needs `'+` rejected as an array key, which requires it to be an enum
  member rather than an ordinary symbol.

The suite runs with `--bigint` and `--compat-types` enabled, which states what is
being measured: whether WAL++ **can** agree with upstream, not whether it agrees by
default. Both are explained below.

## What is the same

The language. Every operator in upstream's enum exists, the reader accepts the same
syntax including `~scope-relative` and `#group-relative` names, `@` offsets, `[...]`
subscripts and quasiquote, and the standard library is upstream's own
`lib/std/std.wal`, loaded unchanged. So `rising`, `falling`, `stable`, `count`,
`defun` and `for` are the same definitions, not reimplementations.

Signals are addressed the same way: `.` joins a hierarchy, `^` prefixes a trace id.

```lisp
(alias clk "tb.clk")
(define cnt 0)
(while (step 1)
  (when (&& clk (! clk@-1))
    (set! cnt (+ cnt 1))))
(printf "Total rising edges: %d\n" cnt)
```

That runs unmodified on both and prints the same number.

## What differs by default

Five things. Each has a reason specific to waveform analysis, and three can be
switched back.

### Types do not coerce

```lisp
(+ 1 "hi")    ; error here, "1hi" upstream
(= 0 #f)      ; false here, true upstream
(= 1 #t)      ; false here, true upstream
(= 1 1.0)     ; false here, true upstream
```

Upstream is written in Python and inherits Python's coercions. WAL++ keeps an int, a
float, a bool and a string apart unless a conversion is written out.

The reason is specific: the VCD reader represents a signal **wider than 64 bits as a
bit string**, because no `int64_t` holds it. A `+` that coerced strings would turn
`(+ wide 1)` into concatenation of a bit pattern and return a plausible wrong
answer, and a loose `=` would let a comparison against that bit string quietly
succeed. On a design where the wide buses are the interesting ones, that is the worst
possible failure mode.

Ordering still mixes numeric types, so `(< 1 1.5)` is true. The strictness is about
identity, not about arithmetic.

What is strict is a string *mixed with* a number. When every operand is a string, `+`
concatenates as upstream does, because nothing there can be mistaken for arithmetic:

```lisp
(+ "t0" "^INDEX")   ; "t0^INDEX", as upstream
(+ 1 "hi")          ; error here, "1hi" upstream
```

That distinction is not cosmetic. Upstream's own `lib/std/std.wal` builds signal names
with `+`, so `ALL-INDICES`, `trace-index` and `timeframe` are written in terms of it
and cannot run without it.

**Turn it off:** `--compat-types` restores upstream's coercion in full, including the
mixed case.

### What the strictness does change in the standard library

Upstream's `stable` and `unstable` are `(= expr (reval expr 1))` and its negation. At
the last index `reval` reads past the end and answers `#f` on both engines, and
upstream's `(= 0 #f)` is true where WAL++'s is false. So the two disagree by exactly
one index:

| on `counter.vcd` | upstream | WAL++ | WAL++ `--compat-types` |
|---|---|---|---|
| `(count (stable tb.clk))` | 1 | 0 | 1 |
| `(count (unstable tb.clk))` | 80 | 81 | 80 |
| `(count (rising tb.clk))` | 40 | 40 | 40 |

`rising` and `falling` compare against `1`, so they are unaffected. If you are
reproducing an upstream number from `stable` or `unstable`, use `--compat-types`.

### Integers are 64-bit

```lisp
(int? 583547542375894327581)
; Error: Integer literal ... does not fit in 64 bits.
;        WAL++ uses 64-bit integers by default; run with --bigint for
;        arbitrary precision.
```

Python's integers are unbounded. WAL++ uses `int64_t`, which is what waveform values
and indices need, and keeps arithmetic on the native path.

**Turn it off:** `--bigint` gives exact arbitrary-precision arithmetic for `+`, `-`,
`*` and all comparisons. It is slower, which is why it is not the default.

### `mod` is Euclidean

```lisp
(mod -7 3)    ; 2 here, 2 upstream
(mod 7 -3)    ; 1 here, -2 upstream
```

The result is the least non-negative residue, `0 <= r < |divisor|`, which is what
`a mod n` means in arithmetic. C++'s `%` truncates, so its sign follows the dividend
and `(mod -7 3)` would be `-1`, which represents no congruence class mod 3 and breaks
the ordinary job of wrapping an index that has gone negative.

**For every positive divisor this agrees with upstream**, which floors. They differ
only for a negative divisor, which is where upstream's answer is the unusual one.

There is no switch for this. It is the correct answer and it matches upstream
everywhere it realistically matters.

### `append` and `extend`

```lisp
(append (list 1 2) 3)            ; (1 2 3)      as upstream
(extend (list 1 2) (list 3 4))   ; (1 2 3 4)    WAL++'s own
```

WAL++ once called its list concatenation `append`, which collided with upstream's
`append`, where it adds one element. The same source meant two different things
depending on which engine ran it. The concatenation is now `extend`, and `append` is
upstream's.

### Array elements are `data<0>`

A VCD hierarchy spells an array element `data[0]`. Both engines rename it to
`data<0>`, because `[` opens a subscript in the language and
`TOP.tb.data[0].data.something` would otherwise read as `TOP.tb.data` subscripted by
zero. The old bracketed spelling still resolves through `get-signal`.

## Spellings the manual documents

These are the forms the [WAL Programmer Manual](https://wal-lang.org/documentation/core)
defines, each checked against upstream 0.9.1. They are listed because every one of
them once disagreed, and `test_wal_manual_forms` in `tests/test_wal_executable.cpp`
now holds them to it.

- **`case`'s `default` is a fallback, not a wildcard.** It is taken only once every
  explicit label has failed, wherever it is written in the clause list.
- **`let` binds in order**, so `(let ([x 10] [y x]) (+ x y))` is 20.
- **`slice` over a number is a bit field**, inclusive and upper-first:
  `(slice 12 3 2)` is 3, `(slice 12 2)` is bit 2. Over a list or a string it indexes
  and slices as Python does.
- **`in` takes an array**, by key, which is what `geta/default` is built on.
- **A trace id may be a symbol**, as the manual types all of them:
  `(load "t.vcd" 'tr)`, `(unload 'tr)`, `(step 'tr 1)`.
- **Unnamed traces are `t0`, `t1`, ...**, including the one `-l` loads, so
  `t0^tb.clk` resolves.
- **`step` takes the count last**, `(step 'tr 5)`, as the manual writes it. The old
  `(step 5 'tr)` also works, since a count is an integer and an id never is.
- **`eval-file "module"` resolves `module.wal`**, and an explicit `.wal` works too.
- **`expr@off` is `reval`**, so an offset outside the trace reads as `#f` at either
  end rather than failing.
- **`true` and `false` are bound globals**, as they are upstream. `#t` and `#f` are
  still how a boolean is written, and still how one prints.
- **A script prints only what it prints.** The value of the last top-level form is
  not echoed, so stdout stays usable for a report or a CSV. An interactive prompt
  still echoes, because there the value is the answer.
- **`print` renders what upstream renders.** Arguments run together with no
  separator, a top-level string contributes its characters bare while a nested one
  is quoted, symbols are bare, arrays are `{("x" 10)}`, booleans are `true` and
  `false`, and a float carries the shortest decimal that reads back as the same
  value, with `.0` kept when it is whole. Three renderers in the binary used to
  disagree with each other: `print` said `#t` and truncated `(/ 10 3)` at ten
  digits, `-e` said `true` and truncated at six.
- **`(** 2 10)` is 1024**, not 1024.0. Integer operands and a non-negative exponent
  give an integer, as upstream does.

The empty value still prints as `nil` rather than upstream's `None`, which is the one
output difference left standing. It is a name for the same thing.

## Where WAL++ accepts more

These are not divergences so much as places upstream raises and WAL++ answers:

- `(max 1 5 3)` works variadically as well as `(max (list 1 5 3))`.
- `begin` is a spelling of `do`.
- `(! "")` is true rather than an error.
- `(groups 'clk)` works; upstream raises `unhashable type: Symbol`.

The one genuine difference in the other direction is `type`, which returns a symbol
here (`'int`) and a Python class object upstream (`<class 'int'>`). That is upstream
exposing its implementation, and matching it would mean exposing WAL++'s.

## Where it cannot follow

`import` and `call` are upstream's bridge into the CPython interpreter it runs
inside. A C++ program has none. They are registered and fail saying exactly that,
rather than looking like missing operators:

```
import: not available in WAL++. Upstream runs inside CPython, so this operator
calls into the host Python interpreter; WAL++ is a C++ program and has none. Use
the Python bindings (import wal_cpp) to drive WAL++ from Python instead of the
other way round.
```

## Default behaviors worth knowing

These are not upstream differences, but they are the defaults that surprise people.

**Streaming depends on size.** Below 100 MB a trace is fully loaded, which is faster
for repeated queries; above it, streaming engages on its own, which is what lets an
oversized trace work at all. `--streaming` and `--no-streaming` force either way at
any size, and all three give the same answers. See
[Working with very large traces](../guides/large-traces.md).

**The extraction modes do not run the interpreter.** `extract weight|distance|values`
and `inspect --format=names`, and their `--fast-*` flag spellings, go through a
dedicated reader. No language
semantics apply to them, so none of the differences on this page affect their output.

**Signals wider than 64 bits read as bit strings.** `--fast-hw-*` and `--fast-hd-*`
count them exactly. `--fast-values-signals` truncates them to the low 64 bits and
warns. In the interpreter, use `bits->sint` to read one as a signed number.

## Running an upstream script unchanged

```bash
wal --compat-types --bigint -f upstream_script.wal
```

That is the configuration the conformance suite uses. If a script still behaves
differently under it, that is a bug worth reporting.

## See also

- `WAL_PARITY.md` in the repository root for the full gap analysis
- [Working with very large traces](../guides/large-traces.md)
- [Scaling to full-core traces](../guides/scaling-extraction.md)
