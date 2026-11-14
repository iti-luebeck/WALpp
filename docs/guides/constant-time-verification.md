# Constant-time verification

Check that an instruction takes the same number of cycles no matter what data it
operates on. A cryptographic implementation that fails this leaks key material
through timing, even when it is functionally correct.

This guide describes the method used in
[Nešković et al., *Security-Driven Verification of RISC-V Crypto Extensions*](../INDEX.md),
applied to the Zkn scalar-crypto instructions of the Sailor core. Nothing in it is
specific to that core.

## The idea

A specification saying an instruction is constant-time says nothing about whether a
particular RTL implementation is. You have to measure.

Bracket the operation under test with two **marker events** that are already visible
on a bus, then measure the clock-cycle distance between them. Repeat with random
operands. If every trial gives the same distance, the operation did not branch on
its data in the tested cases.

The criterion is exact rather than statistical. RTL and gate-level simulation are
deterministic, so for a fixed input class a trial set either shows zero variance or
it does not. A pass means no latency variation was found in the cases you tried; it
is not a proof, and coverage is bounded by the diversity of those trials.

## What you need

- A waveform (VCD or FST) from your simulator.
- A clock signal.
- Two observable marker events. A write to a memory-mapped register works well:
  the test program writes it immediately before and immediately after the
  operation, and the write appears on the data bus as an ordinary transaction.

The paper's test program writes a `.timestamp` register at `0x80003000`, so the
markers are `isWrite && valid && address == 0x80003000`.

## Step 1: extract the value changes

List the signals that carry the clock and the marker, one per line:

```text
TOP.SAILORBlackBoxAdapter.clock
TOP.SAILORBlackBoxAdapter.busIO_dummyDataBus_address
TOP.SAILORBlackBoxAdapter.busIO_dummyDataBus_isWrite
TOP.SAILORBlackBoxAdapter.busIO_dummyDataBus_valid
```

Then pull every value change of those four signals out of the waveform:

```bash
wal --fast-values-signals markers.txt trial.vcd -o changes.csv
```

The output is `timestamp,signal_index,value`, one row per change, where
`signal_index` is the 0-based line number in the signal list. Only listed signals
are read, so the cost is set by how often those four toggle rather than by how large
the waveform is.

This is the throughput-critical step. It does not use the S-expression interpreter at
all: `--fast-values-signals` goes through a dedicated reader. Across a 16,000-trial
campaign the paper measured 4.4 s against 307.8 s for the equivalent Python WAL
query.

## Step 2: turn changes into cycle counts

Counting is ordinary arithmetic on the CSV, in whatever language you post-process in:

1. Every rising edge of the clock is a cycle boundary. Collect their timestamps.
2. A marker is a row where `isWrite` and `valid` are high and `address` matches.
   Debounce if your bus holds a transaction for more than one cycle.
3. The latency of the trial is the number of clock edges between the two markers,
   minus the fixed overhead of the two marker stores themselves.

`artifacts/scripts/walpp_tools.py` in this repository implements exactly these three
steps, and `extract_latency.py` drives it over a campaign.

## Step 3: the pass/fail criterion

Group the per-trial latencies by whatever you consider one input class, for example
`(instruction, byte-select)`. A group passes when every trial in it has the same
latency:

```python
passed = len(set(latencies)) == 1
```

The paper ran N=1000 trials per `(instruction, byte-select)` pair over four
instructions, 16,000 executions in total. Unmodified RTL passed with zero variance
throughout.

## Step 4: validate the check itself

A checker that never fails tells you nothing. Inject a bug and confirm it is caught.

The paper modified one line of the core's hardware control logic so that one
sub-instruction's cycle count depends on a data bit of the S-box input rather than
only on the static opcode. The confined change hit `aes32esmi` alone:

| Instruction | Baseline | With injected bug | Verdict |
|---|---|---|---|
| `aes32esi`  | 32 | 32    | PASS |
| `aes32esmi` | 33 | 33–34 | **FAIL** |
| `aes32dsi`  | 32 | 32    | PASS |
| `aes32dsmi` | 35 | 35    | PASS |

The three unaffected instructions continuing to pass is as much of the result as the
one failing: it shows the method identifies the affected path rather than reacting
to unrelated state.

## Doing it inside WAL++ instead

For a single waveform, the whole thing fits in the interpreter, which is easier to
explore with interactively:

```lisp
(load "trial.vcd" "t")
(alias clk  't^TOP.SAILORBlackBoxAdapter.clock)
(alias addr 't^TOP.SAILORBlackBoxAdapter.busIO_dummyDataBus_address)
(alias isw  't^TOP.SAILORBlackBoxAdapter.busIO_dummyDataBus_isWrite)
(alias vld  't^TOP.SAILORBlackBoxAdapter.busIO_dummyDataBus_valid)

;; indices at which a marker store is on the bus
(define markers (find (&& isw vld (= addr 2147495936))))

;; cycles between the first two markers
(define cycles-between
  (length (find (&& (rising clk)
                    (> INDEX (first markers))
                    (< INDEX (second markers))))))

(printf "latency: %d cycles\n" cycles-between)
```

Use the CLI extraction for a campaign and this for understanding one trial. They read
the same waveform and agree.

## Pitfalls

**A marker that is not unambiguous.** If the address you match is also written by
something else, you will measure the wrong interval. Check the marker count per trial
before trusting any latency.

**A bus that holds a transaction.** `valid` high for several cycles produces several
marker rows for one store. Debounce by ignoring further matches within a cycle of the
previous one.

**Overhead you forgot to subtract.** The two marker stores cost cycles themselves.
That overhead is constant, so it does not affect whether variance exists, but it does
shift the absolute number.

**Reading pass as proof.** A pass means no variation in the trials you ran. Coverage
is bounded by the diversity of operands, not by their number.

## See also

- [Power side-channel analysis](power-side-channel.md) for the other half of the method
- [Scaling to full-core traces](scaling-extraction.md) for why the `--fast-*` modes exist
- `artifacts/` in this repository for the paper's scripts and data
