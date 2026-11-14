# Writing your own extraction tool

How `--fast-hw-scope` and friends are built, and how to build another one.

Reach for this when you have a question the existing modes do not answer, and a
campaign large enough that interpreter speed is the thing you are waiting for.

## Why there are two engines

WAL++ contains an S-expression interpreter and, separately, a set of extraction
kernels. They share the file formats and nothing else.

The interpreter answers **any** question. It parses your expression, builds `Value`
objects, resolves names, and walks the trace through a cursor. That generality costs
roughly two orders of magnitude.

An extraction kernel answers **one shaped** question. It reads the file once, carries
a small running state, and writes a number per timestamp. No expression is parsed, no
`Value` is constructed, no environment exists. That is where the published
throughput figures come from, and it is a pattern you can reuse.

```
                            interpreter            extraction kernel
  answers                   any query              one shaped query
  per-sample cost           Value + lookup         a popcount
  memory                    the signals you read   current state only
  full campaign (N=500)     hours                  172 s
```

## The shape of a kernel

![The parser hands the kernel one timestamp\'s changes at a time through a sink,
reusing the same buffer. The kernel keeps a running state and emits one row per
timestamp, so nothing accumulates.](../images/kernel-pipeline.svg)

The blue box travelling left to right is one timestamp's changes. The point of the
figure is the box in the middle: there is only ever **one** of them. The parser fills
it, hands it over, and fills it again. Your kernel may read it, but must not keep a
pointer to it.

Every one of them is the same three parts:

1. **A tracked set.** Which signal ids this kernel follows, resolved once from the
   header.
2. **A running state.** One slot per tracked signal, seeded from the values at time
   zero.
3. **A fold.** For each timestamp, apply that timestamp's changes to the state,
   update a running answer, emit one row.

Nothing in that needs the whole trace in memory, which is why the parser can hand
over one timestamp at a time and forget it.

## A worked example

A tool that counts total bit toggles below a scope. This is the complete program,
compiled and run as shown:

```cpp
// toggles.cpp - one-pass extraction, built the way --fast-hd-* is built.
#include "wal/fast/vcd_parser.h"
#include <cstdint>
#include <iostream>
#include <unordered_map>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: toggles <trace.vcd> <scope-prefix>\n";
        return 1;
    }

    fast_vcd::VCDParser parser;

    uint64_t timestamps = 0, toggles = 0;
    std::unordered_map<uint32_t, int64_t> state;
    bool ready = false;

    // The sink runs once per timestamp, during the parse. Nothing accumulates, so
    // memory is set by the number of signals followed, not by the trace length.
    parser.set_delta_sink([&](const fast_vcd::DeltaTimestamp& delta) {
        if (!ready) {
            // The header is read before the first delta, so the signal table is
            // complete by the time this runs. Resolving here rather than in main()
            // is why the kernel can be installed before the parse starts.
            for (uint32_t id : parser.get_signals_in_scope(argv[2])) {
                state[id] = 0;
            }
            for (const auto& [id, value] : parser.get_initial_values()) {
                if (state.count(id)) state[id] = value;
            }
            ready = true;
        }

        ++timestamps;
        for (const auto& [id, value] : delta.changes) {
            auto it = state.find(id);
            if (it != state.end()) {
                toggles += __builtin_popcountll(
                    static_cast<uint64_t>(it->second ^ value));
                it->second = value;
            }
        }
    });

    parser.parse(argv[1]);

    std::cout << "signals in scope: " << state.size() << "\n"
              << "timestamps:       " << timestamps << "\n"
              << "bit toggles:      " << toggles << "\n";
    return 0;
}
```

```cmake
find_package(walpp REQUIRED)
add_executable(toggles toggles.cpp)
target_link_libraries(toggles PRIVATE walpp::walpp)
```

```
$ ./toggles counter.vcd tb
signals in scope: 7
timestamps:       81
bit toggles:      150
```

Cross-check it against the built-in mode:

```
$ wal --fast-hd-all counter.vcd -o hd.csv
$ awk -F, '{s+=$2} END {print NR, s}' hd.csv
80 149
```

80 rows summing to 149, against 81 timestamps and 150 toggles. The difference is the
first timestamp: Hamming distance is a comparison between consecutive states, so it
has nothing to report at the first one and emits no row. Our tool counts the changes
at the first timestamp as toggles from the implicit zero state.

Neither is wrong, but **decide which you mean and say so**, because off-by-one at the
first timestamp is the single most common error in this kind of tool.

## The parser interface

```cpp
parser.set_delta_sink(sink);     // install BEFORE parse(); each delta is
                                 // handed over and dropped
parser.parse(path);              // VCD
parser.parse_fst(path);          // FST (see the note below)
```

Available to the sink, valid from the first delta onward:

| | |
|---|---|
| `get_signal_ids()` | every signal id in the trace |
| `get_signal_info(id)` | name, width, scope, VCD identifier |
| `get_signals_in_scope(prefix)` | ids below a hierarchy prefix |
| `get_signal_ids_by_names(names)` | ids for explicit names, **skipping any not found** |
| `get_initial_values()` | values at time zero, 64 bits or narrower |
| `get_initial_wide_values()` | values at time zero, wider than 64 bits |
| `is_wide_signal(id)` | whether a signal exceeds 64 bits |
| `get_timescale()` | `"1ns"`, `"10ps"`, ... |

And on each `DeltaTimestamp`:

| | |
|---|---|
| `timestamp` | simulation time, in the trace's timescale |
| `changes` | `(signal id, new value)` for signals 64 bits or narrower |
| `wide_changes` | `(signal id, word vector)` for wider signals, low word first |

## Restrict the scope

`get_signals_in_scope` is the difference between measuring your design and measuring
your design plus its testbench. The testbench switches too, and that activity is
noise in every analysis built on the result. It is also the cheapest speedup
available: fewer tracked signals means less work per timestamp.

## Decisions the kernel layer makes

These are the ones that change what your tool computes, as opposed to how fast it
runs.

### No language semantics apply

A kernel sees signal ids and integers. The type rules, `--compat-types`, `--bigint`,
the operator set and the standard library are all interpreter concerns, and none of
them is in scope here. Consequences:

- A mode flag cannot change an extraction result. Setting one before running your
  tool does nothing.
- You own the arithmetic. If a value should be read as two's complement, you do it.
- There is no evaluation and nothing can throw a `wal::EvalError` at you.

That is the trade: you give up the language and get two orders of magnitude.

### Wide signals are exact, unlike in the interpreter

The interpreter hands a signal wider than 64 bits to you as a bit **string**, because
no `int64_t` holds it. A kernel gets the same value as a **word vector** in
`wide_changes`, low word first, and can compute on it exactly.

This is the one place the kernel layer is more capable than the interpreter, and it
is why the Hamming modes count wide buses correctly:

```cpp
for (const auto& [sig_id, words] : delta.wide_changes) {
    for (uint64_t w : words) toggles += __builtin_popcountll(w);
}
```

A kernel that reads only `changes` silently reports zero activity for every wide bus.
On a datapath that is most of what you wanted to measure, and nothing will tell you.

### Timestamps are trace time, not cycles

`delta.timestamp` is in the trace's own timescale, which `get_timescale()` reports
as `"1ns"`, `"10ps"` and so on. It is **not** a cycle count and not an index.

Converting to cycles means finding the clock edges, which is a separate pass or a
separate tracked signal. Do not assume one timestamp is one cycle: a VCD emits a
record whenever anything changes, so a busy cycle produces several and an idle one
produces none.

### Emit on the first timestamp, or not

Hamming weight emits a row at every timestamp, including the first. Hamming distance
emits from the second, because the first only establishes a state to compare
against. So distance always has exactly one row fewer.

Neither is wrong. Decide which your tool means and say so in its help text, because
this is the most common off-by-one in this kind of code and it is invisible in the
output.

### Name resolution skips what it cannot find

`get_signal_ids_by_names` returns ids only for the names it resolved, in order, with
the misses dropped. Zipping its result against your input list by position therefore
mislabels every signal after the first missing one, silently.

```cpp
// wrong when any name is missing
auto ids = parser.get_signal_ids_by_names(names);
for (size_t i = 0; i < ids.size(); ++i) mapping[ids[i]] = names[i];

// right
for (const auto& name : names) {
    auto one = parser.get_signal_ids_by_names({name});
    if (one.empty()) { warn(name); continue; }
    mapping[one[0]] = name;
}
```

This caused a real bug in the streaming reader, where it labelled histories with the
wrong signal names and every value came back as zero.

### The sink is installed before the parse, and resolves on the first delta

The signal table only exists once the header has been read, which happens inside
`parse()`. So a kernel cannot resolve its tracked set in its constructor; it does so
the first time it is called, by which point the header is complete. Every kernel in
`streaming_ops.h` has a `ready_` flag for exactly this reason.

### FST materialises

`parse_fst` goes through GTKWave's fstapi, which drives its own callback structure,
so your sink is not called during the read. Results are identical; memory is not.
See the FST section below.

## If you need the state, not a scalar

`streaming_ops.h` holds the kernels the CLI uses, and they are reusable directly:

```cpp
#include "wal/fast/streaming_ops.h"

fast_vcd::VCDParser parser;
fast_vcd::StreamingHamming kernel(
    parser,
    fast_vcd::TrackedSignals::in_scope("top.core"),
    /* distance = */ true,
    [](int64_t ts, int64_t value) {
        std::cout << ts << "," << value << "\n";
    });

parser.set_delta_sink([&](const fast_vcd::DeltaTimestamp& d) { kernel.on_delta(d); });
parser.parse("trace.vcd");
kernel.finish();
```

`StreamingValues` does the same for raw value changes of named signals. Starting from
these rather than from scratch gets you the wide-signal handling and the
first-timestamp convention for free.

## FST

`parse_fst` goes through GTKWave's fstapi, which drives its own callback structure,
so it materialises the deltas and your sink is not called during the read. The
pattern still works, with the memory characteristics of a full load:

```cpp
parser.parse_fst(path);
for (const auto& delta : parser.get_deltas()) {
    kernel.on_delta(delta);
}
```

Results are identical either way. Only the memory differs.

## Is it worth it?

Before writing one, check whether an existing mode covers it. `--fast-hw-scope`,
`--fast-hd-scope`, `--fast-values-signals` and `--fast-list` between them answer most
questions, and `--fast-values-signals` in particular is more general than it looks:
it gives you every value change of the signals you name, and the arithmetic on top
can live in whatever language you post-process in.

Write a kernel when the per-timestamp computation is the expensive part and doing it
outside would mean shipping every value change through a CSV to get at it. Counting
toggles is a good example: the answer is one number, and extracting every change to
compute it outside would move gigabytes to produce eight bytes.

## See also

- [Embedding WAL++ in a C++ program](cpp-api.md) for the interpreter-level API
- [Scaling to full-core traces](scaling-extraction.md) for what the built-in modes cost
- [Generated API reference](../../api/index.md) for the extraction reader's own types,
  under `fast_vcd`
- `src/fast/streaming_ops.cpp` in this repository for the kernels themselves
