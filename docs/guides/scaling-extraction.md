# Scaling to full-core traces

Why the `--fast-*` modes exist, what they cost, and how to tell whether an analysis
will finish.

## The problem they solve

Waveform analysis languages are built for interactive work: load a trace, poke at
signals, write a query. That shape stops working when the question is a campaign
rather than a trace.

A side-channel evaluation of a complete processor core needs thousands of
simulation runs, each producing a waveform, each needing one number per cycle
extracted from it. At the published throughput of the Python toolchain, a single
Hamming-weight pass over one 147,000-signal netlist trace takes about 147 s. For a
500-trace campaign that is roughly 13 hours of extraction for a result whose
simulation took 2.6.

The extraction, not the simulation, becomes the thing you wait for. That is what
WAL++ was written to remove.

## Measured throughput

Against the Python WAL/SCA-WAL toolchain, on the same traces and the same machine
(Apple M4 Max, 64 GB), including waveform parsing in every figure:

| Measurement | WAL/SCA-WAL | WAL++ | Speedup |
|---|---|---|---|
| Constant-time extraction, N=16,000 | 307.8 s | 4.4 s | 70× |
| Hamming-weight micro-benchmark | 51.84 s | 0.053 s | 978× |
| Hamming-weight campaign trace | 46.52 s | 0.051 s | 912× |
| Full N=500 campaign extraction | ~12.9 h | 172 s | ~271× |

The campaign row is the one that matters: it turns an overnight job into a coffee
break, which is the difference between an analysis you run once and one you run
every time the design changes.

## Why they are fast

![The parser hands the kernel one timestamp at a time through a sink, reusing one
buffer, so nothing accumulates.](../images/kernel-pipeline.svg)

Three things, none of them clever:

**They never build an interpreter.** `--fast-hw-*`, `--fast-hd-*`,
`--fast-values-signals` and `--fast-list` go through a dedicated reader in
`include/wal/fast/`. No S-expression is parsed, no `Value` is constructed, no
environment is created. The cost is reading the file and counting bits.

**They update incrementally.** Hamming weight carries a running total and adjusts it
by only the signals that changed at each timestamp, instead of recomputing a
popcount over the whole state every time. On a 147,000-signal trace that is the
difference between O(timestamps × signals) and O(total changes).

**They stream.** The parser hands each timestamp's changes to the kernel and reuses
the buffer, so nothing accumulates.

## Memory

The parser used to build the complete delta structure before any kernel ran, so peak
memory was about 2.7 times the file. It streams now:

| trace | before | after |
|---|---|---|
| 50 MB | 137 MB (2.73×) | 54 MB (1.07×) |
| 201 MB | 542 MB (2.69×) | 205 MB (1.02×) |

What remains is the memory-mapped file, which is clean, read-only and reclaimable
under pressure. Memory the process actually holds is about 4 MB regardless of trace
size, against 341 MB before at 201 MB.

Practically: extraction is bounded by disk throughput, not by RAM, and a trace
larger than memory is no longer a problem.

## Choosing a mode

| Question | Command |
|---|---|
| How much is switching, overall? | `extract weight TRACE --scope S` |
| How much is *changing*? | `extract distance TRACE --scope S` |
| What value does this bus carry? | `extract values TRACE --signals L` |
| What is in this trace? | `inspect TRACE` |

Each has a flag spelling (`--fast-hw-scope`, `--fast-hd-scope`,
`--fast-values-signals`, `--fast-list`) which is what the paper's scripts use. The
two are the same command: see the
[command-line reference](../reference/cli.md#verbs).

Without a selector, `extract` covers the whole trace, testbench included. Prefer
`--scope` or `--signals`: the testbench switches too, and that activity is noise.

## Keeping extraction honest

**Scope before you extract.** Arbitrary hardware contains signals unrelated to the
computation. Narrowing the scope cuts both runtime and noise.

**Check the row count.** A Hamming mode emits one row per timestamp; distance emits
one fewer. A row count that does not match what you expect usually means the scope
prefix matched nothing, and the result is an empty file rather than an error.

**Time one trace before launching a campaign.** Extraction time scales with the
number of changes. Multiply by the campaign size before committing a night to it.

**Keep the output.** Extraction is deterministic, so a cached CSV is as good as the
waveform for everything downstream, and vastly smaller. The paper's artifact ships
the activity traces rather than the waveforms for exactly this reason.

## When the interpreter is the right tool

The fast modes answer one shaped question very quickly. The interpreter answers any
question at interpreter speed. Use it to work out *what* to extract, then extract at
scale:

```bash
# explore
wal -l trace.vcd -e '(length SIGNALS)' -e 'SCOPES'
wal -l trace.vcd -e '(count (rising top.core.clk))'

# then run the campaign
for f in traces/*.vcd; do
  wal extract weight "$f" --scope top.core -o "${f%.vcd}.csv"
done
```

For reading an interpreter query over a trace too large to hold, see
[Working with very large traces](large-traces.md).

## See also

- [Constant-time verification](constant-time-verification.md)
- [Power side-channel analysis](power-side-channel.md)
- [Working with very large traces](large-traces.md)
