# Command-line reference

Every flag, what it does, and which ones change results rather than
performance.

## Options

```bash
wal [OPTIONS] [FILE]

OPTIONS:
  -h, --help                  Show help message
  -v, --version               Show version information
  -e, --eval EXPR             Evaluate expression
  -f, --file FILE             Execute WAL program file
  -l, --load FILE             Load waveform trace file

FAST MODE (optimized HD/HW calculation with CSV output):
  --fast-hd-all FILE -o OUT.csv       Hamming Distance (all signals)
  --fast-hd-scope SCOPE FILE -o OUT   Hamming Distance (scoped)
  --fast-hd-signals LIST FILE -o OUT  Hamming Distance (signal list)
  --fast-hw-all FILE -o OUT.csv       Hamming Weight (all signals)
  --fast-hw-scope SCOPE FILE -o OUT   Hamming Weight (scoped)
  --fast-hw-signals LIST FILE -o OUT  Hamming Weight (signal list)

INPUT MODES:
  --pipe FILE                 Read trace from named pipe
  --stdin                     Read trace from standard input

STREAMING OPTIONS:
  --streaming                 Enable streaming (auto for files > 100 MB)
  --no-streaming              Disable streaming
  --chunk-size SIZE           Set chunk size in MB (default: 100)
  --cache-capacity NUM        Set cache entries per signal (default: 1000)
  --cache-stats               Show cache statistics after loading
```

## Examples

### Fast Mode - High-Performance HD/HW Calculations

Fast mode provides 32-743x speedup over standard WAL evaluation for Hamming Distance and Hamming Weight calculations. It uses optimized delta compression and outputs results in CSV format.

```bash
# Hamming Distance for all signals (32x faster)
wal --fast-hd-all netlist.vcd -o hamming_distance.csv

# Hamming Distance for specific scope (743x faster for large traces)
wal --fast-hd-scope "TOP.cpu.core" design.vcd -o core_hd.csv

# Hamming Weight for all signals
wal --fast-hw-all trace.vcd -o hamming_weight.csv

# Hamming Weight for specific module
wal --fast-hw-scope "TestHarness.dut" testbench.vcd -o dut_hw.csv

# Hamming Distance for specific signal list (e.g., tainted signals from CellIFT)
wal --fast-hd-signals signals.txt trace.vcd -o tainted_hd.csv

# Hamming Weight for specific signal list
wal --fast-hw-signals signals.txt trace.vcd -o tainted_hw.csv
```

**Signal List Format:**
The signal list file should contain one signal name per line (full hierarchical names):
```
TOP.top.core.ex_reg_valid
TOP.top.core.mem_reg_valid
TOP.top.core.wb_reg_valid
TOP.top.core.rf[0]
TOP.top.core.rf[1]
```

**Working with CellIFT Taint Reports:**
```bash
# Parse CellIFT taint report and extract signal names
python parse_taint_report.py report.txt -o tainted_signals.txt

# Calculate Hamming Distance for tainted signals only
wal --fast-hd-signals tainted_signals.txt trace.vcd -o tainted_hd.csv

# Extract signals from specific scope
python parse_taint_report.py report.txt --scope "TOP.top.core" -o core_signals.txt
wal --fast-hw-signals core_signals.txt trace.vcd -o core_hw.csv

# List available modules in taint report
python parse_taint_report.py report.txt --list-modules
```

**Fast Mode Performance:**
- AES netlist (147K signals, 383 timestamps): 13.6s → 0.43s (32x speedup)
- Rocket core (32K signals, 65K timestamps): 2:36 → 0.21s (743x speedup)
- Output: CSV format with timestamp,value per line

**Visualize Results:**
```bash
# Generate HD/HW analysis
wal --fast-hd-all design.vcd -o results.csv

# Plot with included script
python plot_results.py results.csv plot.png --title "Hamming Distance"
```

### Load Trace and Analyze

```bash
# List all signals
wal -l circuit.vcd -e '(signal-names)'

# Count signals
wal -l trace.vcd -e '(length (signal-names))'

# Find signal widths
wal -l design.vcd -e '(map signal-width (signal-names))'

# Define and use variables
wal -l trace.vcd -e '(define sigs (signal-names))'
```

### Expression Evaluation

```bash
# Arithmetic
wal -e '(+ 1 2 3 4 5)'

# List processing
wal -e '(map (lambda (x) (* x 2)) (list 1 2 3 4 5))'

# String manipulation
wal -e '(concat "Hello " "World")'

# Complex expressions
wal -e '(filter (lambda (x) (> x 5)) (list 1 3 5 7 9))'
```

### Pipe Integration

```bash
# From VCD file
cat simulation.vcd | wal --stdin -e '(signal-names)'

# From simulator
ghdl run --stop-time=1ms --vcd=out.vcd | \
  wal --stdin -f analysis.wal

# Named pipe
(verilog_sim -o /tmp/trace.vcd &)
wal --pipe /tmp/trace.vcd -e '(length (signal-names))'
```

### Script Execution

```bash
# Execute analysis script
wal -l trace.vcd -f analysis.wal

# Pipe trace and execute script
cat simulation.vcd | wal --stdin -f analysis.wal

# From named pipe
wal --pipe /tmp/trace.fifo -f analysis.wal
```

### Streaming Options

```bash
# Large file with streaming
wal -l huge_trace.vcd --streaming --cache-stats

# Custom chunk size
wal -l trace.vcd --streaming --chunk-size 256

# Large cache
wal -l trace.vcd --cache-capacity 50000

# Disable streaming
wal -l small.vcd --no-streaming
```

---

## Verbs

| verb | what it does |
|---|---|
| `inspect TRACE` | what is in a trace, as JSON |
| `lint SCRIPT.wal` | check a script without running it |
| `eval EXPR [EXPR...]` | evaluate expressions |
| `run SCRIPT.wal` | run a script |
| `repl` | start the interactive REPL |
| `extract KIND TRACE -o OUT.csv` | one pass over a trace, CSV out |

`extract` takes a kind and an optional selector:

| kind | what you get |
|---|---|
| `weight` | Hamming weight: bits set at each timestamp |
| `distance` | Hamming distance: bits changed since the previous timestamp |
| `values` | the decoded value of each listed signal, when it changes |
| `names` | every signal as `width name`, plus a `# timescale` header |

| selector | |
|---|---|
| *(none)* | every signal in the trace |
| `--scope SCOPE` | only signals under a hierarchy prefix |
| `--signals FILE` | only the signals named in FILE |

```bash
wal extract weight trace.vcd --scope top.core -o activity.csv
wal extract values trace.vcd --signals markers.txt -o changes.csv
wal inspect trace.vcd --json
wal lint analysis.wal
```

### The flag spelling

Every verb has an equivalent flag, and the flags are **not going away**:

| verb | flag |
|---|---|
| `extract weight T --scope S` | `--fast-hw-scope S T` |
| `extract weight T --signals L` | `--fast-hw-signals L T` |
| `extract weight T` | `--fast-hw-all T` |
| `extract distance T --scope S` | `--fast-hd-scope S T` |
| `extract values T --signals L` | `--fast-values-signals L T` |
| `extract names T` | `--fast-list T` |
| `inspect T` | `--inspect T` |
| `lint S` | `--lint S` |
| `run S` | `-f S` |
| `eval E` | `-e E` |

The verbs are **sugar**: a verb invocation is rewritten into the flag form before
anything else runs, so there is one implementation underneath and no second code
path that could drift.

That matters more than tidiness. The flag forms are what
[Nešković et al.](../INDEX.md) documents, and what its Table II throughput was
measured through: the artifact scripts in `artifacts/scripts/` run them literally.
A test asserts byte-identical output from both spellings of every extraction mode,
and another runs the paper's four published invocations, so neither can be broken
by a later tidy-up without the build failing.

If you are reproducing the paper, or you have scripts already, keep using the flags.

A first argument counts as a verb only when it matches one *and* no file of that
name exists, so `wal script.wal` still runs a script. `extract list` is accepted as
an older spelling of `extract names`.

## Agent and scripting modes

Machine-readable entry points, for a program driving `wal` rather than a person
reading its output.

### The envelope

Every `--json` answer has the same shape, which is the one the ITI `ekumen` tool
uses, so an assistant that has learned one can read the other:

```json
{
  "envelope_version": 1,
  "tool": "walpp",
  "tool_version": "1.2.2",
  "command": "inspect",
  "generated_at": "2026-11-14T12:25:39Z",
  "source": {
    "path": "counter.vcd",
    "modified_at": "2026-11-13T23:06:12Z",
    "age_seconds": 47966
  },
  "ok": true,
  "warnings": [],
  "data": { }
}
```

Two parts of that are safety properties rather than conveniences.

**A failure has the same envelope as a success.** `ok` is always present, and a
failure adds an `error` with a stable `code`:

```json
{ "ok": false,
  "error": { "code": "walpp.eval.failed",
             "message": "Eval error: Undefined variable: no.such",
             "subject": "(count (rising no.such))",
             "hint": "the expressions before this one are in data.results" },
  "data": { "results": [40] } }
```

A consumer that had to tell "the command failed" from "the command returned nothing"
by reading prose would eventually get it wrong in the direction that matters. Note
that `data` still carries what did succeed, so a caller can see how far the run got.

**The source's age is materialised.** A trace is a file, and an agent can easily be
reading one produced before the change it is trying to evaluate. `source.age_seconds`
makes that visible rather than leaving an answer from a week-old trace to be read as
current. No freshness budget is applied: WAL++ cannot know what the trace ought to be
newer *than*, so it reports and the caller decides.

Error codes are namespaced: `walpp.trace.unreadable`, `walpp.eval.failed`,
`walpp.lint.failed`, and per-finding codes such as `walpp.lint.unknown-operator`.

### `--inspect FILE`

What is in a trace: its length, its scopes, and every signal with the width and the
exact spelling the engine accepts.

```bash
wal --inspect trace.vcd | jq -r '.data.signals[].name'
```

```json
"data": {
  "max_index": 80,
  "index_count": 81,
  "signal_count": 7,
  "hierarchy_separator": ".",
  "trace_id_separator": "^",
  "scopes": ["tb", "tb.dut"],
  "signals": [
    { "name": "tb.dut.counter", "leaf": "counter", "scope": "tb.dut", "width": 4 }
  ]
}
```

The two separators are stated rather than left to be assumed, because the caller most
likely to read this is one building signal names by hand.

### `--lint FILE.wal`

Parse and check a script **without running it**; exit non-zero if it will not run.

```bash
wal --lint analysis.wal
```

```
analysis.wal:2:1: error: call to 'cont', which is not an operator, a macro, or
                        defined in this file [unknown-operator]
    check the spelling, or define it before use
```

Worth running on anything long. At runtime a mistyped operator is only noticed when
evaluation reaches it, by which point the earlier half of the script has already run
and possibly written files.

An unresolvable symbol in **argument** position is not reported, because that is how
a signal is read and `--lint` loads no trace. Only a call to something that does not
exist is an error.

### `--json`, `--compact-json`

Wrap `-e` results and `--lint` findings in the envelope. `--compact-json` is the same
content on one line.

```bash
wal -l trace.vcd --json -e '(count (rising tb.clk))' -e 'SIGNALS'
```

```json
"data": { "results": [40, ["tb.overflow", "tb.clk", "..."]] }
```

One command produces one envelope, so `ok` describes the whole run rather than
whichever `-e` printed last.

A bigint is emitted as a string, because JSON numbers carry no more precision than a
double and a bigint is used exactly when that is not enough.
