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
