# Getting started

A first trace, a first query, and the shape of everything else.

## Command Line Interface

### 1. Interactive REPL

```bash
wal
# WAL++ Interactive Shell
# Type (quit) to exit or (help) for help
wal> (+ 1 2 3)
6
wal> (define square (lambda (x) (* x x)))
square
wal> (square 5)
25
wal> (quit)
Goodbye!
```

### 2. Evaluate Expression

```bash
wal -e '(+ 1 2 3)'
# 6

wal -e '(list "hello" "world")'
# ("hello" "world")
```

### 3. Load and Analyze Trace

```bash
# Load VCD file and query signals
wal -l simulation.vcd -e '(signal-names)'

# Load and define variables
wal -l trace.vcd -e '(define signals (signal-names))'

# Pipe trace and analyze
cat simulation.vcd | wal --stdin -e '(length (signal-names))'
```

### 4. Load Script File

```bash
# Execute WAL script on loaded trace
wal -l trace.vcd -f analysis.wal

# Pipe trace and execute script
cat trace.vcd | wal --stdin -f analysis.wal

# Read from named pipe
wal --pipe /tmp/trace.fifo -f analysis.wal
```

## Python API

### 1. Basic Usage

```python
import walpp

# Create WAL instance
wal = walpp.Wal()

# Evaluate expressions
result = wal.eval_str("(+ 1 2)")
print(result)  # 3

result = wal.eval_str("(list 1 2 3)")
print(result)  # [1, 2, 3]
```

### 2. Load and Analyze Trace

```python
import walpp

wal = walpp.Wal()

# Load trace file
wal.load("simulation.vcd")

# Query signals
signals = wal.eval_str("(signal-names)")
print(f"Found {len(signals)} signals")

# Analyze signal properties
result = wal.eval_str("(map signal-width (signal-names))")
print(result)  # Signal widths
```

### 3. Working with Variables

```python
import walpp

wal = walpp.Wal()
wal.load("trace.vcd")

# Define analysis variables
wal.eval_str("(define signals (signal-names))")
wal.eval_str("(define num-signals (length signals))")

# Query results
num_sigs = wal.eval_str("num-signals")
print(f"Total signals: {num_sigs}")

# Conditional analysis
result = wal.eval_str(
    "(if (> num-signals 10) "
    "  (list \"Many signals\" num-signals) "
    "  (list \"Few signals\" num-signals))"
)
print(result)
```

### 4. Signal Analysis

```python
import walpp

wal = walpp.Wal()
wal.load("trace.vcd")

# Get signal names
signals = wal.eval_str("(signal-names)")

if signals:
    first_signal = signals[0]
    
    # Query signal properties
    width = wal.eval_str(f"(signal-width \"{first_signal}\")")
    print(f"Signal '{first_signal}' width: {width} bits")
    
    # Check if signal exists
    exists = wal.eval_str(f"(signal? \"{first_signal}\")")
    print(f"Signal exists: {exists}")
```

---

## Usage Guide

## CLI Modes

### File Mode (`-l` or `--load`)

Load a waveform file for analysis:

```bash
# Basic trace loading
wal -l trace.vcd

# With expression evaluation
wal -l trace.vcd -e '(define signals (signal-names))'

# With streaming options
wal -l huge.vcd --streaming --chunk-size 50

# Disable streaming for small files
wal -l tiny.vcd --no-streaming
```

### Expression Mode (`-e` or `--eval`)

Evaluate a WAL expression directly:

```bash
# Simple arithmetic
wal -e '(+ 1 2 3)'

# Complex expressions
wal -e '(map (lambda (x) (* x 2)) (list 1 2 3 4 5))'

# With trace analysis (requires -l first)
wal -l trace.vcd -e '(length (signal-names))'
```

### Script Mode (`-f` or `--file`)

Execute a WAL script file:

```bash
# Simple script execution
wal -f analysis.wal

# With trace loading
wal -l trace.vcd -f analysis.wal

# Script from stdin
cat script.wal | wal --stdin
```

### Stdin Mode (`--stdin`)

Read trace data from standard input:

```bash
# Pipe VCD directly
cat simulation.vcd | wal --stdin

# Pipe with expression
simulator_output | wal --stdin -e '(length (signal-names))'

# Pipe with script
vcd_generator | wal --stdin -f analysis.wal
```

### Pipe Mode (`--pipe`)

Read trace from named pipe (FIFO):

```bash
# Create named pipe and write to it in background
cat trace.vcd > /tmp/trace.fifo &

# Read from pipe
wal --pipe /tmp/trace.fifo -e '(signal-names)'

# In a script
mkfifo /tmp/trace.fifo
(simulator > /tmp/trace.fifo &)
wal --pipe /tmp/trace.fifo -f analysis.wal
rm /tmp/trace.fifo
```

## Input Methods

### Direct File Input

```bash
# Standard file loading
wal -l circuit.vcd

# Auto-detected format
wal -l trace.fst          # FST format
wal -l data.csv           # CSV format
wal -l waveform.vcd       # VCD format
```

### Streaming for Large Files

```bash
# Automatically enable streaming for files > 100 MB
wal -l huge_trace.vcd --streaming

# Customize chunk size
wal -l trace.vcd --chunk-size 256   # 256 MB chunks

# Customize cache capacity
wal -l trace.vcd --cache-capacity 5000

# Display cache statistics
wal -l trace.vcd --cache-stats
```

### Real-time Simulation Input

```bash
# Stream simulation output directly
simulator --output-vcd /dev/stdout | wal --stdin -e '(signal-names)'

# Named pipe for parallel processing
simulator --output-fifo /tmp/sim.fifo &
wal --pipe /tmp/sim.fifo -f analysis.wal
```

## Trace Analysis

### Signal Discovery

```bash
# List all signals
wal -l trace.vcd -e '(signal-names)'

# Count signals
wal -l trace.vcd -e '(length (signal-names))'

# Filter signals by pattern
wal -l trace.vcd -e '(filter (lambda (s) (string? s)) (signal-names))'
```

### Signal Properties

```bash
# Get signal width
wal -l trace.vcd -e '(signal-width "clk")'

# Check if signal exists
wal -l trace.vcd -e '(signal? "data[7:0]")'

# List all signal widths
wal -l trace.vcd -e '(map signal-width (signal-names))'
```

### Time-based Analysis

```bash
# Navigate through timesteps
wal -l trace.vcd -e '(set-index 100)'   # Jump to timestep 100
wal -l trace.vcd -e '(step 10)'         # Step forward 10 timesteps
wal -l trace.vcd -e '(get-index)'       # Get current index
```

## Expression Evaluation

### Basic Arithmetic

```scheme
(+ 1 2 3 4 5)              ; 15
(- 10 3)                   ; 7
(* 2 3 4)                  ; 24
(/ 100 4)                  ; 25
(mod 17 5)                 ; 2
(abs -42)                  ; 42
```

### List Operations

```scheme
(list 1 2 3)               ; (1 2 3)
(length '(a b c))          ; 3
(map (lambda (x) (* x 2)) '(1 2 3))  ; (2 4 6)
(filter (lambda (x) (> x 2)) '(1 2 3 4 5))  ; (3 4 5)
(fold + 0 '(1 2 3 4))      ; 10
```

### Conditionals

```scheme
(if (> 5 3) "yes" "no")    ; "yes"

(define n -3)
(cond
  ((= n 0) "zero")
  ((> n 0) "positive")
  (else "negative"))       ; "negative"
```

### Lambda Functions

```scheme
(define square (lambda (x) (* x x)))
(square 5)                 ; 25

(map (lambda (x) (* x 2)) '(1 2 3 4))  ; (2 4 6)
```

---
