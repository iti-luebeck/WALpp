# Cookbook

"I want X, what do I type." Every command here was run against a real trace before
it was written down.

![Four questions and the command each leads to: what is in this trace leads to
inspect, one question about it leads to eval, a number per timestamp leads to
extract, will this script run leads to lint.](../images/choosing-a-command.svg)

If you are not sure which of the four you want, you almost certainly want the first
one. Start there.

## Looking at a trace

**What is even in this file?**

```bash
wal inspect trace.vcd
```

JSON: how long the trace runs, its scopes, and every signal with its width and the
exact spelling the engine accepts. Pipe it to `jq` if you want one part:

```bash
wal inspect trace.vcd | jq -r '.data.signals[].name'
wal inspect trace.vcd | jq '.data.index_count'
```

**I need a plain list to feed `--signals`.**

```bash
wal inspect trace.vcd --format=names 2>/dev/null > names.txt
```

Each line is `width name`. The `2>/dev/null` drops the progress line, which goes to
stderr; without it the first line of your file is "Parsing VCD...".

To get just the names, as a `--signals` file wants:

```bash
wal inspect trace.vcd --format=names 2>/dev/null | grep -v '^#' | awk '{print $2}' > signals.txt
```

**How long is it, how wide is that bus?**

```bash
wal eval 'MAX-INDEX' -l trace.vcd                        # 80   (so 81 indices)
wal eval '(length SIGNALS)' -l trace.vcd                 # 7
wal eval '(signal-width (quote tb.dut.counter))' -l trace.vcd   # 4
wal eval '(signal? (quote tb.dut.counter))' -l trace.vcd        # true
```

## Asking one question

All of these are `wal eval '<expression>' -l trace.vcd`.

| I want | expression |
|---|---|
| count clock edges | `(count (rising tb.clk))` |
| count falling edges | `(count (falling tb.clk))` |
| the value right now | `tb.dut.counter` |
| the value at index 40 | `(begin (set-index 40) tb.dut.counter)` |
| the value one index earlier | `(begin (set-index 40) tb.clk@-1)` |
| every index where a value held | `(find (= tb.dut.counter 3))` |
| the first time it happened | `(first (find (= tb.dut.counter 3)))` |
| how many cycles a signal was high | `(length (find (= tb.clk 1)))` |

```bash
$ wal eval '(count (rising tb.clk))' -l counter.vcd
40
$ wal eval '(find (= tb.dut.counter 3))' -l counter.vcd
(15 16 47 48 79 80)
```

**Cycles between two events**, which is the shape most latency questions take:

```bash
wal eval '(let ([m (find (= tb.dut.counter 3))])
            (- (second m) (first m)))' -l trace.vcd
```

### The one that catches everyone

`find`, `count` and `whenever` sweep from the **current index**, not from zero:

```bash
wal eval '(count (rising tb.clk))' -l t.vcd                        # 40
wal eval '(begin (set-index 40) (count (rising tb.clk)))' -l t.vcd # 20
```

That is deliberate and useful for windowing an analysis. If you want the whole
trace, do not move the cursor first, or put it back with `(set-index 0)`.

## Producing a series

When you want a number per timestamp, for one trace or five hundred, use `extract`.
It does not run the interpreter at all, which is where the speed comes from.

**Switching activity, as a power proxy:**

```bash
wal extract weight   trace.vcd --scope top.core -o activity.csv   # bits set
wal extract distance trace.vcd --scope top.core -o activity.csv   # bits changed
```

Output is `timestamp,value`. Distance has exactly one row fewer than weight, because
the first timestamp has nothing before it to compare against.

**Scope it.** Without `--scope` you measure the testbench too, and the testbench
switches. It is also the cheapest speedup available.

**Raw bus values**, for latency and marker analysis where a bit count is not what
you want:

```bash
wal extract values trace.vcd --signals markers.txt -o changes.csv
```

Output is `timestamp,signal_index,value`, one row per change, where `signal_index` is
the line number in `markers.txt` (0-based).

**A whole campaign:**

```bash
for f in traces/*.vcd; do
  wal extract weight "$f" --scope top.core -o "${f%.vcd}.csv"
done
```

## Scripts

**Check before running:**

```bash
wal lint analysis.wal
```

Catches source that does not parse, with a line and column, and calls to operators
that do not exist. Exits non-zero if the script will not run, so it drops into a
Makefile or a pre-commit hook without a wrapper.

Worth doing on anything long: at runtime a mistyped operator is only noticed when
evaluation reaches it, by which point the first half has already run.

**Then run it:**

```bash
wal run analysis.wal -l trace.vcd
```

A script is ordinary WAL, and upstream WAL scripts run unchanged:

```lisp
(alias clk "tb.clk")
(define cnt 0)
(while (step 1)
  (when (&& clk (! clk@-1))
    (set! cnt (+ cnt 1))))
(printf "Total rising edges: %d\n" cnt)
```

## Driving it from a program

**JSON out**, in a fixed envelope with an `ok` flag:

```bash
wal eval '(count (rising tb.clk))' -l trace.vcd --json
```

A failure uses the same envelope, so you never have to tell "it failed" from "it
returned nothing" by reading prose. See the
[command-line reference](../reference/cli.md#the-envelope).

## Very large traces

```bash
wal -l huge.vcd --streaming -e '(count (rising tb.clk))'
```

Below 100 MB a trace is fully loaded, which is faster for repeated queries. Above it,
streaming engages on its own. `--streaming` and `--no-streaming` force either way.
All three give the same answers.

If the question is one `extract` can answer, prefer that: it reads the trace in a
single pass with memory that does not grow with the file.

See [Working with very large traces](large-traces.md).

## Exploring interactively

```bash
wal -l trace.vcd       # or: wal repl -l trace.vcd
```

Type expressions, `(quit)` to leave. Useful while you work out what to ask; once the
query is right, move it into `eval` or a script.

## Where to go next

![A first session: see what is there, get the query right, then scale it
up.](../images/first-session.svg)

- [Getting started](getting-started.md) for the longer walkthrough
- [Language reference](../reference/language.md) for what you can write in `eval`
- [Command-line reference](../reference/cli.md) for every flag
- [Constant-time verification](constant-time-verification.md) and
  [Power side-channel analysis](power-side-channel.md) for the two worked analyses
