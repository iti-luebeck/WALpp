---
name: walpp-waveform-analysis
description: Analyze digital simulation waveforms (VCD/FST/CSV) with the WAL++ `wal` tool - list signals, time edges and pulses, measure latencies, compute Hamming weight/distance and switching activity, and export CSV. Use when inspecting, querying or analyzing a waveform or trace file, or when writing WAL++ S-expression analysis.
---

# WAL++ Waveform Analysis

Drive the `wal` command-line tool to analyze digital simulation waveforms. WAL++ is a
C++20 waveform-analysis language with S-expression syntax and 130+ operators.

Everything below was verified against **WAL++ v1.6.3**. Where a claim could go stale,
the check that verifies it is given with it.

## Finding the binary

```bash
export WAL=$(command -v wal || echo "$PWD/build/wal")   # installed, or a local build
$WAL --version                                          # => WAL++ v1.6.3
```

If it is missing, build it from the repository root:

```bash
cmake -S . -B build && cmake --build build -j
```

Formats are auto-detected from the file: **VCD**, **FST**, **CSV**.

## Check the skill against your binary first

This file makes claims that only a specific build of `wal` satisfies. Run the
bundled check whenever something behaves unexpectedly, or after a pull:

```bash
./.claude/skills/walpp-waveform-analysis/verify.sh
```

It runs every documented behavior and reports which ones still hold. If a check
fails, trust the binary and fix this file.

## Names: `.` for hierarchy, `^` for a trace

A signal is `tb.dut.counter`. The interpreter, the `--fast-*` modes and upstream WAL
all spell it that way. `^` means something else: it prefixes a **trace id**, so
`a^tb.dut.counter` is that signal in the trace loaded as `a`.

```bash
$WAL -l t.vcd -e '(signal? (quote tb.dut.counter))'   # => true
$WAL --fast-list t.vcd -o names.txt                   # => "4 tb.dut.counter"
```

An older WAL++ joined the hierarchy with `^`, and those names still resolve, so a
script written against it keeps working. Do not write new ones that way.

**Do not type a name from memory.** Get it from the trace:

```bash
$WAL inspect t.vcd | jq -r '.data.signals[].name'   # every name, exactly as accepted
$WAL inspect t.vcd --format=names                   # or as "width name" lines
```

Note that `-e` echoes signal names as quoted symbols while `print` renders them bare,
which matters when you are generating a signal-list file:

```bash
$WAL -l t.vcd -e '(signal-names)'          # => ('tb^overflow 'tb^clk ...)
$WAL -l t.vcd -e '(print (signal-names))'  # => (tb^overflow tb^clk ...)
```

## If your binary predates the `-e` fixes

Two `-e` defects were fixed after the first public release. On an older build they fail
silently rather than erroring, which makes them expensive to diagnose. `verify.sh` tells
you which side of the fix you are on, under "The two -e defects".

| | Old behavior | Fixed behavior |
|---|---|---|
| `wal -e '(list 1 2 3)'` | prints **nothing** | prints `(1 2 3)` |
| `wal -e A -e B` | runs **only B** | runs A then B, sharing one interpreter |

The cause of the first was a result printer that handled `nil`, `bool`, `int`, `float`
and `string` and then stopped, so lists and symbols fell off the end of the chain and
vanished. The REPL had a second copy of that printer which did handle them, which is why
`(signal-names)` printed fine interactively and looked like an empty trace through `-e`.
The cause of the second was a single `std::string` that each `-e` overwrote.

On an older binary: wrap list results in `(print ...)`, and use one `-e` with `begin`
or a `-f` script instead of several `-e`.

## CLI cheat sheet

```bash
$WAL                                   # interactive REPL
$WAL -l trace.vcd                      # load a trace, then REPL
$WAL -l trace.vcd -e '(print (signal-names))'   # load + evaluate one expression
$WAL -l trace.vcd -f script.wal        # load + run a script file (many forms)
$WAL -e '(+ 1 2)'                      # evaluate without a trace
cat trace.vcd | $WAL --stdin           # stream a trace from stdin
$WAL --pipe /tmp/trace.fifo            # read from a named pipe (live simulation)
$WAL -l huge.vcd --streaming           # force streaming (automatic above 100 MB)
$WAL -l huge.vcd --cache-stats         # print signal cache statistics
```

### Fast CSV modes (bypass the interpreter)

These parse the trace directly and write CSV. Use them for large traces and for
switching-activity or power analysis.

```bash
$WAL --fast-hw-all  trace.vcd -o hw.csv            # Hamming weight, all signals, every timestamp
$WAL --fast-hd-all  trace.vcd -o hd.csv            # Hamming distance, consecutive timestamps
$WAL --fast-hw-scope "tb" trace.vcd -o s.csv       # weight, signals under a scope prefix
$WAL --fast-hd-scope "tb" trace.vcd -o s.csv       # distance, scoped
$WAL --fast-hw-signals siglist.txt trace.vcd -o s.csv   # weight, signals listed in a FILE
$WAL --fast-hd-signals siglist.txt trace.vcd -o s.csv   # distance, signal-list FILE
$WAL --fast-values-signals siglist.txt trace.vcd -o s.csv  # raw value changes (1-bit AND buses)
$WAL --fast-list trace.vcd -o names.txt            # "width name" per signal + "# timescale"
```

- `--fast-*-signals` takes a **file** (one dotted name per line), not an inline list.
- `--fast-values-signals` emits `timestamp,list-index,value` rows when a listed signal
  *changes* (plus its initial value), after a `# timescale ...` line. It gives the decoded
  value rather than a popcount, so it is the only fast mode that is meaningful for
  **buses**. Values are int64 and x/z is not represented, so vectors wider than 64 bits
  truncate **on this path**.
- Hamming weight/distance handle signals **wider than 64 bits exactly** (a separate wide
  path, since v1.2.2). Only the value export truncates.
- `--fast-*-scope` is a **prefix** match. Top-level scopes (`tb`) work; over-specific
  prefixes can match nothing - always check the `(N entries)` count the tool prints.
- HW/HD output has no header and is `timestamp,value`; HD starts at the second timestamp.

## Signal model and navigation

```bash
$WAL -l trace.vcd -e '(print (signal-names))'  # => (tb^overflow tb^clk ... tb^dut^counter)
$WAL -l trace.vcd -e '(print (get-traces))'    # => (DEFAULT)
```

| Operator | Meaning |
|---|---|
| `(signal-names)` | all signal names (list - wrap in `print`) |
| `(get-traces)` | loaded trace ids |
| `(signal? "name")` | existence check |
| `(signal-width "name")` | bit width |
| `(get-signal "name")` | value at the current index (optional offset as 2nd arg) |
| `(get-index)` / `(set-index n)` | current timestamp index / jump to `n` |
| `(step n)` | advance by `n` (negative steps back) |
| `(load "f" [id])` / `(unload id)` | load or drop a trace |

**There are no bare special variables.** `SIGNALS`, `INDEX`, `TIMESTAMP`, `MAX-INDEX`
and `SCOPES` all fail with *Undefined variable*. Use the function forms above.

Navigation works **inside a single expression**, so a whole-trace series can be built
with `map`:

```bash
$WAL -l trace.vcd -e '(print (begin (set-index 40) (get-signal "tb^dut^counter")))'
$WAL -l trace.vcd -e '(print (map (lambda (i) (begin (set-index i) (get-signal "tb^dut^counter"))) (range 0 12)))'
# => (0 0 0 0 0 0 0 0 0 0 0 1)
```

Index state also carries from one `-e` to the next, since they share one interpreter
(on a binary that predates the `-e` fix it does not, because only the last `-e` runs).
Keeping the navigation and the read in one expression works on every build.

For long series prefer the Hamming operators or the `--fast-*` CSV modes - they iterate
every timestamp in one pass instead of re-seeking per index.

## Hamming / switching activity

These auto-iterate all timestamps and return `((timestamp value) ...)`:

```lisp
(hamming-weight-all-signals-all-timestamps)
(hamming-distance-all-signals-all-timestamps)
(hamming-weight-scope "tb")
(hamming-distance-scope "tb^dut")
```

Total toggle count for a scope:

```bash
$WAL -l trace.vcd -e '(print (sum (map (lambda (p) (nth 1 p)) (hamming-distance-scope "tb"))))'
```

**Cross-check recipe.** The interpreter and the fast path should agree. On
`tests/original/traces/counter.vcd` both give **149**:

```bash
$WAL -l counter.vcd -e '(print (sum (map (lambda (p) (nth 1 p)) (hamming-distance-scope "tb"))))'
$WAL --fast-hd-all counter.vcd -o hd.csv && awk -F, '{s+=$2} END {print s}' hd.csv
```

A disagreement means a scope prefix matched the wrong set, or a signal name did not
resolve - not that one path is "more accurate".

## Analysis operators

These take an explicit list of numbers, not a signal name. Build it with `(list ...)`.

- Aggregation: `sum` `product` `mean` `min-val` `max-val`
- Statistics: `variance` `stddev` `median` `quartiles` `skewness` `kurtosis`
  `(histogram L bins)`
- Signal stats: `rms` `peak-to-peak` `crest-factor` `zero-crossings` `autocorrelation`
  `(moving-average L window)`
- Thresholded (second argument required): `(edges L threshold)` -> `((rising i) ...)`,
  `(duty-cycle L threshold)`, `(pulse-width L threshold)`
- Filtering and shape: `(gaussian-blur L sigma)` `(median-filter L window)` `derivative`
  `integral` `(rise-time L lo hi)` `(fall-time L hi lo)`
- Spectrum: `frequency-spectrum` `magnitude-spectrum` `phase-spectrum`
  `(peak-frequency L rate)` `(thd L rate)`

```bash
$WAL -e '(rms (list 1 2 3 4))'                      # => 2.73861
$WAL -e '(print (edges (list 0 1 0 1 1 0) 0.5))'    # => ((rising 1) (falling 2) (rising 3) (falling 5))
```

## Language essentials

WAL++ runs upstream WAL's language, including its standard library, so `let`,
`while`, `cond`, `case`, `when`, `unless`, `defun`, `for` and macros are all
available, as are `rising`, `falling`, `stable`, `count`, `find` and `whenever`.

```lisp
(define x 10)
(defun dbl [n] (* n 2))                ; or (define dbl (fn [n] (* n 2)))
(let ([a 1] [b 2]) (+ a b))
(when (> x 5) (print "big"))
(while (step 1) (print INDEX))
(map (fn [x] (* x 2)) (list 1 2 3))
```

Other categories: arithmetic `+ - * / ** % floor ceil round abs min max`; comparison
`= != < > <= >= between clamp in?`; logic `&& || !`; bitwise `band bor bxor bnot lshift
rshift`; math `sin cos tan asin acos atan sqrt log log10 exp`; lists `list first rest
append length reverse nth slice sort unique take drop flatten zip array-ref array-set`;
strings `concat split join substring string-length uppercase lowercase trim startswith
endswith replace string-index contains`; types `type is-int is-float is-string is-list
is-bool convert-int convert-float convert-string`; export `to-csv from-csv to-json
from-json json-array matrix-transpose`; visualization `ascii-plot gnuplot-script
html-plot export-tsv`; I/O `print`. Indexing (`nth`, `array-ref`) is 0-based.

### Upstream WAL scripts run unchanged

This was not true of older WAL++ builds, and the previous version of this skill told
you to translate. Do not. 71 of upstream's 77 tests pass and every one of its 106
operators exists, so the idioms work as written:

```lisp
(alias clk "tb.clk")
(count (rising clk))                   ; the stdlib macro, not a builtin
(whenever (rising clk) (inc cnt))
(printf "edges: %d\n" cnt)
clk@-1                                 ; the previous sample
INDEX   MAX-INDEX   TS   SIGNALS       ; special variables
```

Two defaults differ from upstream on purpose, both switchable:

| | WAL++ default | upstream | flag |
|---|---|---|---|
| types | nothing coerces: `(+ 1 "hi")` errors, `(= 0 #f)` is false | coerces | `--compat-types` |
| integers | 64-bit | unbounded | `--bigint` |

`(mod -7 3)` is `2` here and upstream; they differ only for a negative divisor.

To run an upstream script exactly: `wal --compat-types --bigint -f script.wal`

### Verbs

The extended modes have a verb spelling as well as the flags:

```bash
$WAL inspect trace.vcd --json
$WAL lint analysis.wal
$WAL eval '(count (rising tb.clk))' -l trace.vcd
$WAL extract weight trace.vcd --scope top -o activity.csv
$WAL inspect trace.vcd --format=names > signals.txt   # the --signals input
```

They are sugar over the flags, byte for byte. Use whichever reads better; the flag
forms are what the paper's scripts use and are not going away.

### Checking a script before running it

```bash
$WAL --lint analysis.wal          # syntax and unknown calls, without executing
$WAL --lint analysis.wal --json   # the same, for a program to read
```

Worth doing on anything long: at runtime a mistyped operator is only noticed when
evaluation reaches it, by which point the earlier half has already run.

## `scripts/wal-time.py` - edge and cycle timing without quoting

For the common timing jobs - listing signals, edge and pulse timing, inter-event
latency - use the bundled helper instead of hand-quoted S-expressions. It shells out to
`wal --fast-list` and `wal --fast-values-signals`, so it reads every format `wal` reads
and uses the fast path; only the timing arithmetic is Python. Signals are matched by
**substring** (ambiguous matches are listed), and all times are nanoseconds derived from
the trace timescale.

```bash
T=.claude/skills/walpp-waveform-analysis/scripts/wal-time.py
$T trace.vcd signals tx_en                  # names containing "tx_en"
$T trace.vcd edges   counter                # value-change times
$T trace.vcd pulses  eth_tx_en              # rise / fall / high-time per pulse
$T trace.vcd at      ctrl 1500              # value at t=1500 ns
$T trace.vcd find    haddr 0x40000100       # times a bus hits a value
$T trace.vcd latency haddr 0x40000100 eth_tx_en 1   # ns from A==v to the next B==v
```

It finds the binary via `$WAL`, then `wal` on `PATH`, then `build/wal` in this
repository. Values accept `12` or `0xC`; buses print as hex.

Reach for `wal -e '(...)'` instead when you want the aggregate analysis WAL++ is uniquely
good at - Hamming and switching activity over a whole scope, statistics, spectra.

## Worked examples in this repository

`artifacts/` holds the full evaluation of the WAL++ paper and is the best source of
realistic usage:

- `artifacts/scripts/walpp_tools.py` - the shared pattern: drive `wal --fast-values-signals`
  and `wal --fast-hw-scope`, then do clock-edge counting, marker detection and per-cycle
  resampling in Python/numpy on top.
- `artifacts/scripts/extract_latency.py`, `check_constant_time.py` - constant-time analysis.
- `artifacts/scripts/extract_campaign.py`, `make_figure2.py` - power/CPA/TVLA campaigns.
- `artifacts/scripts/signals/*.txt` - signal-list files in the dotted form the fast modes expect.
- `artifacts/traces/*.vcd.xz` - traces to try the commands above on.

## Python API

The `walpp` package wraps the same engine, and it avoids both CLI gotchas: one instance
keeps its state across calls, and a list comes back as a real Python list instead of
needing `print`.

```python
import walpp

wal = walpp.Wal()                        # note the capitalization: Wal, not WAL
wal.load("trace.vcd")
signals = wal.eval_str("(signal-names)")  # -> a Python list
wal.eval_str("(define n (length signals))")
print(wal.eval_str("n"))                  # state persists on the same instance
```

See the README's *Python API* section for more.
