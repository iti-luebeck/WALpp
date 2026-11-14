# Working with very large traces

How WAL++ reads a trace that does not fit in memory, which mode to pick, and what
each one costs.

## Three ways to read a trace

![A scan head sweeps once across a trace file. Full load keeps every decoded
timestamp, so retained memory grows with the trace. Streaming hands each timestamp
over and drops it, so retained memory stays
flat.](../images/streaming-vs-full-load.svg)

Both make the same single pass over the file. The difference is entirely what they
keep afterwards, which is why the choice is about memory rather than speed.

| | What it keeps | Use it for |
|---|---|---|
| **Extraction** (`--fast-*`) | current state only | campaigns, one shaped question per run |
| **Streaming** (`--streaming`) | the signals you read | interpreter queries on a trace too large to load |
| **Full load** (default) | the whole trace | interactive work, repeated queries over everything |

Extraction is the fastest and the most bounded, but it answers only the four
questions it has modes for. Streaming and full load both run the whole language; they
differ in what they hold.

## Full load, the default

```bash
wal -l trace.vcd -e '(count (rising tb.clk))'
```

The reader decodes the trace into per-index values up front. Every subsequent query
is served from memory, so interactive exploration is fast after the first wait. The
cost is that the whole trace must fit.

This is the right default. Reach for something else when the trace stops fitting, not
before.

## Streaming

```bash
wal --streaming -l trace.vcd -e '(count (rising tb.clk))'
```

or, from a script:

```lisp
(load "trace.vcd" "t")
```

with `--streaming` on the command line.

A streaming trace reads its header once and then builds a signal's history **only
when that signal is first read**. An analysis that watches a clock and three bus
signals keeps four short change lists, not the trace.

The whole language works against a streamed trace. Values at an offset
(`clk@-1`), backward jumps (`set-index`), and whole-trace sweeps (`find`, `whenever`,
`count`) all give the same answers as a full load. A test asserts exactly that on
fourteen queries, because the two readers disagreeing silently is the failure mode
that matters.

### What it costs

Memory is set by what you read, not by what the trace contains:

```
4000 signals × 4000 timestamps, 198 MB trace
   1 signal read     214.2 MB peak
  50 signals read    223.5 MB peak      (~0.19 MB per signal history)
```

The baseline there is the memory-mapped file, which is clean and reclaimable. What
the process holds is the histories.

Time is the trade. Reading a signal that has not been read before costs one forward
pass over the file. A pass picks up every signal requested so far, so:

- Reading the same few signals repeatedly: one pass, then memory speed.
- Reading signals one at a time, each new: one pass each.

**Name the signals you need early.** Touching all of them once at the start collects
them in a single pass:

```lisp
(load "huge.vcd" "t")
;; one pass for all four, instead of four passes
(list tb.clk tb.addr tb.isWrite tb.valid)

;; everything after this is served from the histories
(count (rising tb.clk))
```

### Limits

Streaming is implemented for VCD. A streamed CSV or FST still falls back to the
chunk reader, which parses the header but does not answer indexed value reads.

## Which to choose

**Start with the default.** If the trace loads and queries are quick, nothing else is
worth the complexity. Note that the default is not "never stream": a trace above
100 MB streams on its own, which is what lets an oversized one work at all. Below
that it is fully loaded, because that is faster for repeated queries.

`--streaming` forces it on at any size, `--no-streaming` forces it off at any size.
All three give the same answers.

**Use `--streaming`** when a load fails or thrashes, and you have an interpreter
question to ask. Expect the first read of each signal to cost a pass.

**Use the extraction modes** when the question is one of the four they cover, or when
you are running a campaign. They are the fastest and the most predictable, and they
are what the published throughput figures measure. See
[Scaling to full-core traces](scaling-extraction.md).

**Combine them.** Extraction turns a waveform into a CSV far smaller than the
original, and the CSV is as good as the waveform for everything downstream:

```bash
wal --fast-hw-scope top.core huge.vcd -o activity.csv   # once
# then analyse activity.csv as often as you like
```

## Reducing the trace instead

Often the cheapest fix is not reading less but dumping less.

**Dump a scope.** Most simulators can restrict the dump to part of the hierarchy.
Signals you never read cost time in the simulator, space on disk and time in every
analysis.

**Dump a window.** Start the dump at the operation of interest rather than at reset.

**Trim after the fact.** When a trace is already written and only its start matters:

```lisp
(load "huge.vcd" "t")
(trim-trace "t" 100000)
```

**Sample.** When only certain indices matter, restrict the trace to them:

```lisp
(sample-at (list 0 1000 2000 3000))
```

## Diagnosing

```bash
wal -l trace.vcd -e '(length SIGNALS)' -e 'MAX-INDEX' -e 'SCOPES'
```

`SIGNALS` tells you how wide the trace is, `MAX-INDEX` how long. The product is a
good predictor of whether a full load will fit. `SCOPES` shows what you could narrow
to.

`--fast-list` answers the same question without loading anything:

```bash
wal --fast-list trace.vcd -o signals.txt
wc -l signals.txt
```

## See also

- [Scaling to full-core traces](scaling-extraction.md) for the extraction modes
- [Compatibility with upstream WAL](../reference/wal-compatibility.md) for defaults that differ
