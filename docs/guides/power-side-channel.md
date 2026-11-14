# Power side-channel analysis

Estimate what a design leaks through its power consumption, before it exists in
silicon, by deriving an activity trace from a simulation waveform and running the
same attacks an evaluator would.

This guide describes the method used in
[Nešković et al., *Security-Driven Verification of RISC-V Crypto Extensions*](../INDEX.md),
applied to AES-128 on the Sailor core at both RTL and post-route netlist level.

## The idea

![Four signals toggling over eight timestamps. Hamming weight counts the bits set at
each timestamp; Hamming distance counts the bits that changed since the previous one.
Either series is the activity trace.](../images/activity-extraction.svg)

The grid at the top is the raw switching the trace already records. Everything below
it is arithmetic on that grid: weight counts the bits that are set now, distance
counts the bits that differ from the previous column.

Two details in the figure matter later. Distance has **one row fewer** than weight,
because the first timestamp has nothing before it to compare against. And the two
series are not interchangeable: at a timestamp where the same number of bits are set
as before but different ones, weight is flat and distance is not.

You cannot measure power in a simulator, but you can measure a **proxy** for it.
Switching activity correlates with dynamic power, and a waveform records every
switch. Two proxies are standard:

- **Hamming weight** — how many bits are set across the design at a given moment.
  Good when the leakage follows the value held.
- **Hamming distance** — how many bits changed since the previous moment. Good when
  the leakage follows the transition, which is usually closer to CMOS behavior.

WAL++ computes either across a whole scope, one timestamp at a time. What you do
with the resulting series is ordinary side-channel analysis: correlation power
analysis (CPA) to recover a key, or test vector leakage assessment (TVLA) to decide
whether any leakage is present at all.

WAL++ does the extraction. It introduces no analysis technique of its own.

## Step 1: one trace per operation

Each activity trace must come from an independent simulation run, one encryption per
waveform, so that traces can be compared across varying inputs.

Generate plaintexts with a seeded generator and keep the key fixed, so the campaign
is exactly reproducible. The paper used a standard NIST AES-128 test vector as the
key and N=500 traces per configuration.

## Step 2: extract the activity series

Restrict extraction to the part of the hierarchy that matters. Arbitrary hardware
contains signals unrelated to the computation under study, and including them adds
noise without adding information:

```bash
# Hamming weight of everything under the core, per timestamp
wal --fast-hw-scope top.core trace_0001.vcd -o activity_0001.csv

# Hamming distance instead, if transitions are the better proxy
wal --fast-hd-scope top.core trace_0001.vcd -o activity_0001.csv
```

The output is `timestamp,value`. Distance has exactly one row fewer than weight: the
first timestamp only establishes a state to compare against.

These modes read the trace in a single forward pass and keep only the current state,
so memory does not grow with the length of the trace. A 201 MB VCD costs about 4 MB
beyond the file mapping itself.

## Step 3: resample to cycles

A waveform has a row per timestamp, and timestamps are not cycles. Glitches inside a
cycle are real information at netlist level, so do not discard them: **sum** the
activity within each clock cycle rather than sampling at the edge.

That is what makes the netlist analysis meaningfully different from the RTL one. An
RTL simulation is glitch-free, so within-cycle summing and edge sampling agree. A
post-route netlist with SDF annotation has real intra-cycle transitions, and they
carry leakage.

Find the clock edges the same way as in
[constant-time verification](constant-time-verification.md): extract the clock with
`--fast-values-signals` and count its rising edges.

## Step 4: CPA or TVLA

With one activity row per cycle per trace, you have the matrix every side-channel
tool expects.

**CPA** correlates a hypothetical leakage model against the measured activity across
candidate key values. Target an intermediate that depends on one key byte and one
plaintext byte, conventionally the first-round S-box output, which gives 256
hypotheses per byte. The correct key byte is the one whose correlation stands out.

**TVLA** asks only whether leakage exists, without recovering anything. Split the
campaign into a fixed-plaintext set and a random-plaintext set, and run Welch's
t-test per cycle. The standard threshold is |t| > 4.5.

TVLA is the cheaper question and the one to ask first: it tells you whether there is
anything to find before you spend a campaign trying to find it.

## What the paper found

Both configurations were fully compromised at N=500.

| | CPA correlation peaks | TVLA max \|t\| |
|---|---|---|
| Plain software AES (byte-oriented C S-box) | 0.79 – 0.99 | 151.9 |
| Zkn/Zknd-accelerated AES | 0.75 – 0.92 | 75.2 |
| Accelerated, post-route netlist | 0.72 – 0.78 | 90.7 |

All 16 key bytes were recovered in every case.

The accelerated implementation has lower peaks than the software one, and that is
the result worth stating carefully: **lower peaks are not higher attack resistance.**
Dedicated crypto hardware with an unmasked single-cycle S-box leaks just as the
software baseline does. Relocating a computation from software into hardware is
often implicitly treated as a security improvement, and it is not one. Masking or
another countermeasure is still required.

## Doing it inside WAL++

For exploring one trace rather than running a campaign:

```lisp
(load "trace_0001.vcd" "t")

;; the activity series itself: ((timestamp hd) (timestamp hd) ...)
(define activity (hamming-distance-scope "top.core"))

(printf "timestamps:      %d\n" (length activity))
(printf "total switching: %d\n" (fold + 0 (map (fn [row] (second row)) activity)))
(printf "busiest moment:  %d\n" (max (map (fn [row] (second row)) activity)))
```

Note the shape: `hamming-weight-scope` and `hamming-distance-scope` return the
**whole series** as `(timestamp value)` rows, not a value at the current index. That
is the same data `--fast-hd-scope` writes to CSV, which is why the two agree.

Use the CLI for a campaign and the interpreter to understand one trace.

## Pitfalls

**Including the testbench.** Memories, drivers and checkers switch too, and none of
it is the design. Scope the extraction.

**Sampling instead of summing.** At netlist level this discards exactly the
intra-cycle activity that makes a netlist analysis worth running.

**Traces that are not independent.** One simulation run per trace. Reusing a run with
different post-processing produces correlated traces and an optimistic result.

**Reading a low correlation as safety.** It means the model fits less well, not that
less leaks. TVLA answers the leakage question directly.

## See also

- [Constant-time verification](constant-time-verification.md) for the timing half
- [Scaling to full-core traces](scaling-extraction.md) for the throughput this depends on
- `artifacts/` in this repository for the paper's scripts and data
