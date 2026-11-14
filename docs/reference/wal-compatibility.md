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

```bash
python3 tests/conformance/run_conformance.py
```

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

**Turn it off:** `--compat-types` restores upstream's coercion in full.

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

**Streaming is off.** A trace is fully loaded unless `--streaming` is given. Full
load is faster for repeated queries and is the right default; streaming is for
traces that do not fit. See
[Working with very large traces](../guides/large-traces.md).

**The extraction modes do not run the interpreter.** `--fast-hw-*`, `--fast-hd-*`,
`--fast-values-signals` and `--fast-list` go through a dedicated reader. No language
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
