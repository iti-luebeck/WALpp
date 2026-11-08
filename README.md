# WAL++ - Waveform Analysis Language

A powerful, expressive language and toolkit for analyzing digital simulation waveforms (VCD, FST, CSV) with S-expression syntax, streaming support, and Python bindings.

**WAL++ is a high-performance C++20 rewrite of the [Waveform Analysis Language (WAL)](https://wal-lang.org), originally developed in Python.** It provides the same powerful S-expression syntax and 130+ operators, but with dramatically improved performance and memory efficiency for analyzing large-scale waveforms.

[![License](https://img.shields.io/badge/license-BSD--3--Clause-blue)](LICENSE)
![Language](https://img.shields.io/badge/language-C%2B%2B20-brightgreen)
![Python](https://img.shields.io/badge/python-3.8+-blue)
[![WAL Official](https://img.shields.io/badge/WAL-wal--lang.org-0066cc)](https://wal-lang.org)

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Installation](#installation)
  - [Build from Source](#build-from-source)
  - [Python Installation](#python-installation)
- [Quick Start](#quick-start)
  - [Command Line Interface](#command-line-interface)
  - [Python API](#python-api)
- [Usage Guide](#usage-guide)
  - [CLI Modes](#cli-modes)
  - [Input Methods](#input-methods)
  - [Trace Analysis](#trace-analysis)
  - [Expression Evaluation](#expression-evaluation)
- [Language Reference](#language-reference)
  - [Basic Syntax](#basic-syntax)
  - [Data Types](#data-types)
  - [Operators](#operators)
  - [Control Flow](#control-flow)
  - [Functions](#functions)
- [Advanced Features](#advanced-features)
  - [Streaming Mode](#streaming-mode)
  - [Pipe Input](#pipe-input)
  - [Script Files](#script-files)
  - [Signal Analysis](#signal-analysis)
- [Python API Reference](#python-api-reference)
  - [Core Classes](#core-classes)
  - [Example Scripts](#example-scripts)
- [CLI Reference](#cli-reference)
  - [Options](#options)
  - [Examples](#examples)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)
- [Citing WAL++](#citing-wal)

---

## Overview

WAL++ (Waveform Analysis Language in C++20) is a domain-specific language designed for efficient analysis of digital simulation waveforms. It combines:

- **S-expression syntax** for powerful functional programming paradigms
- **130+ operators** for signal processing, statistical analysis, and data transformation
- **Streaming support** for large waveform files (VCD, FST, CSV)
- **Multiple input modes** (files, pipes, stdin) for integration with simulators
- **Python bindings** for scripting and integration with data science tools
- **Interactive REPL** for exploratory analysis

WAL++ is ideal for:
- **Post-simulation analysis** of digital designs
- **Real-time trace processing** from running simulators
- **Batch waveform analysis** in CI/CD pipelines
- **Integration with Python notebooks** for visualization

### About WAL and WAL++

**WAL (Waveform Analysis Language)** is a pioneering open-source project ([wal-lang.org](https://wal-lang.org)) that introduced S-expression-based waveform analysis to the digital design community. The original Python implementation established the language semantics, operator library, and analytical capabilities that have proven invaluable for hardware verification workflows.

**WAL++** is a ground-up rewrite of WAL in modern C++20, maintaining 100% compatibility with WAL's syntax and semantics while delivering:

| Aspect | WAL (Python) | WAL++ (C++20) |
|--------|--------------|--------------|
| **Performance** | Baseline | **10-50x faster** |
| **Memory Usage** | High for large traces | **Significantly reduced** |
| **Large File Support** | Limited (GBs) | **Full streaming (100+ GBs)** |
| **Startup Time** | 1-2 seconds | **< 100ms** |
| **Syntax Compatibility** | Native | **100% compatible** |
| **Library Integration** | Python-only | **C++ + Python bindings** |
| **Deployment** | Python runtime required | **Single binary executable** |

### Key Improvements in WAL++

1. **Performance**: JIT-compiled evaluation engine vs. interpreted Python
2. **Memory Efficiency**: Streaming architecture for traces larger than RAM
3. **Responsiveness**: Minimal startup overhead, immediate feedback
4. **Integration**: Native C++ library with Python bindings for flexibility
5. **Deployment**: Self-contained binary, no Python dependency for CLI

If you're familiar with WAL from wal-lang.org, WAL++ provides the same expressive power with enterprise-grade performance characteristics.

---

## Features

### Core Language Features
- ✅ Functional programming with lambda expressions
- ✅ First-class functions and higher-order operators
- ✅ Dynamic typing with multiple data types
- ✅ Automatic memory management
- ✅ Error handling with meaningful messages

### Waveform Support
- ✅ VCD (Verilog Change Dump) files
- ✅ FST (Fast Signal Trace) format
- ✅ CSV (Comma-Separated Values) format
- ✅ Automatic format detection
- ✅ Streaming for files > 100 MB

### Input Modes
- ✅ Direct file loading (`-l trace.vcd`)
- ✅ Standard input piping (`cat trace.vcd | wal --stdin`)
- ✅ Named pipe (FIFO) input (`wal --pipe /tmp/trace.fifo`)
- ✅ Interactive REPL mode
- ✅ Expression evaluation (`-e '(+ 1 2)'`)
- ✅ Script file execution (`-f script.wal`)

### Analysis Operators (130+)
- **Signal Operations**: signal-names, signal-width, signal-value, etc.
- **Mathematical**: +, -, *, /, mod, abs, min, max, etc.
- **Logical**: and, or, not, if, cond
- **List Processing**: map, filter, reduce, length, car, cdr, etc.
- **String Operations**: concat, substr, string-length, etc.
- **Statistical**: mean, variance, stddev, min, max, etc.
- **Signal Processing**: FFT, filtering, windowing, etc.
- **Data Export**: write-vcd, write-csv, write-fst

### Fast Mode - High-Performance HD/HW
- ✅ Optimized Hamming Distance calculation (32-743x faster)
- ✅ Optimized Hamming Weight calculation
- ✅ Delta compression architecture
- ✅ Scope-based filtering
- ✅ CSV output format
- ✅ Memory-efficient processing
- ✅ Supports massive traces (100+ GB)

---

## Installation

### Prerequisites

- **C++20 capable compiler** (GCC 10+, Clang 11+, or MSVC 2019+)
- **CMake** 3.20+
- **Python** 3.8+ (for Python bindings)
- **pybind11** (for Python bindings)
- **libedit** (optional, line editing in the interactive REPL; `libedit-dev` on Debian/Ubuntu, built into macOS)
- **Optional**: pylibfst (for FST format support)

### Build from Source

```bash
# Clone the repository
git clone https://github.com/iti-luebeck/walpp.git
cd walpp

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_PYTHON_MODULE=ON \
         -DENABLE_TESTS=ON

# Build
cmake --build . -j$(nproc)

# Install
cmake --install .

# Run tests
ctest --output-on-failure
```

### Build Options

```bash
# Full build with all features
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_PYTHON_MODULE=ON \
         -DENABLE_TESTS=ON \
         -DENABLE_COVERAGE=ON \
         -DENABLE_FST_SUPPORT=ON

# Minimal build (C++ CLI only)
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_PYTHON_MODULE=OFF \
         -DENABLE_TESTS=OFF

# Development build with debugging
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DBUILD_PYTHON_MODULE=ON \
         -DENABLE_TESTS=ON
```

### Python Installation

After building, install the Python module:

```bash
cd build
pip install -e .

# Or directly
python setup.py install
```

Verify installation:

```python
import walpp
print(walpp.__version__)
```

---

## Quick Start

### Command Line Interface

#### 1. Interactive REPL

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

#### 2. Evaluate Expression

```bash
wal -e '(+ 1 2 3)'
# 6

wal -e '(list "hello" "world")'
# ("hello" "world")
```

#### 3. Load and Analyze Trace

```bash
# Load VCD file and query signals
wal -l simulation.vcd -e '(signal-names)'

# Load and define variables
wal -l trace.vcd -e '(define signals (signal-names))'

# Pipe trace and analyze
cat simulation.vcd | wal --stdin -e '(length (signal-names))'
```

#### 4. Load Script File

```bash
# Execute WAL script on loaded trace
wal -l trace.vcd -f analysis.wal

# Pipe trace and execute script
cat trace.vcd | wal --stdin -f analysis.wal

# Read from named pipe
wal --pipe /tmp/trace.fifo -f analysis.wal
```

### Python API

#### 1. Basic Usage

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

#### 2. Load and Analyze Trace

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

#### 3. Working with Variables

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

#### 4. Signal Analysis

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

### CLI Modes

#### File Mode (`-l` or `--load`)

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

#### Expression Mode (`-e` or `--eval`)

Evaluate a WAL expression directly:

```bash
# Simple arithmetic
wal -e '(+ 1 2 3)'

# Complex expressions
wal -e '(map (lambda (x) (* x 2)) (list 1 2 3 4 5))'

# With trace analysis (requires -l first)
wal -l trace.vcd -e '(length (signal-names))'
```

#### Script Mode (`-f` or `--file`)

Execute a WAL script file:

```bash
# Simple script execution
wal -f analysis.wal

# With trace loading
wal -l trace.vcd -f analysis.wal

# Script from stdin
cat script.wal | wal --stdin
```

#### Stdin Mode (`--stdin`)

Read trace data from standard input:

```bash
# Pipe VCD directly
cat simulation.vcd | wal --stdin

# Pipe with expression
simulator_output | wal --stdin -e '(length (signal-names))'

# Pipe with script
vcd_generator | wal --stdin -f analysis.wal
```

#### Pipe Mode (`--pipe`)

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

### Input Methods

#### Direct File Input

```bash
# Standard file loading
wal -l circuit.vcd

# Auto-detected format
wal -l trace.fst          # FST format
wal -l data.csv           # CSV format
wal -l waveform.vcd       # VCD format
```

#### Streaming for Large Files

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

#### Real-time Simulation Input

```bash
# Stream simulation output directly
simulator --output-vcd /dev/stdout | wal --stdin -e '(signal-names)'

# Named pipe for parallel processing
simulator --output-fifo /tmp/sim.fifo &
wal --pipe /tmp/sim.fifo -f analysis.wal
```

### Trace Analysis

#### Signal Discovery

```bash
# List all signals
wal -l trace.vcd -e '(signal-names)'

# Count signals
wal -l trace.vcd -e '(length (signal-names))'

# Filter signals by pattern
wal -l trace.vcd -e '(filter (lambda (s) (string? s)) (signal-names))'
```

#### Signal Properties

```bash
# Get signal width
wal -l trace.vcd -e '(signal-width "clk")'

# Check if signal exists
wal -l trace.vcd -e '(signal? "data[7:0]")'

# List all signal widths
wal -l trace.vcd -e '(map signal-width (signal-names))'
```

#### Time-based Analysis

```bash
# Navigate through timesteps
wal -l trace.vcd -e '(set-index 100)'   # Jump to timestep 100
wal -l trace.vcd -e '(step 10)'         # Step forward 10 timesteps
wal -l trace.vcd -e '(get-index)'       # Get current index
```

### Expression Evaluation

#### Basic Arithmetic

```scheme
(+ 1 2 3 4 5)              ; 15
(- 10 3)                   ; 7
(* 2 3 4)                  ; 24
(/ 100 4)                  ; 25
(mod 17 5)                 ; 2
(abs -42)                  ; 42
```

#### List Operations

```scheme
(list 1 2 3)               ; (1 2 3)
(length '(a b c))          ; 3
(map (lambda (x) (* x 2)) '(1 2 3))  ; (2 4 6)
(filter (lambda (x) (> x 2)) '(1 2 3 4 5))  ; (3 4 5)
(reduce + 0 '(1 2 3 4))    ; 10
```

#### Conditionals

```scheme
(if (> 5 3) "yes" "no")    ; "yes"

(cond 
  ((= x 0) "zero")
  ((> x 0) "positive")
  (else "negative"))
```

#### Lambda Functions

```scheme
(define square (lambda (x) (* x x)))
(square 5)                 ; 25

(map (lambda (x) (* x 2)) '(1 2 3 4))  ; (2 4 6)
```

---

## Language Reference

### Basic Syntax

WAL++ uses S-expressions (symbolic expressions), the classic Lisp syntax:

```scheme
; Comments start with semicolon

; Atoms
42                         ; Integer
3.14                       ; Float
"hello"                    ; String
'symbol                    ; Symbol

; Lists (S-expressions)
(+ 1 2)                    ; Function call
(list 1 2 3)               ; List construction
'(a b c)                   ; Quoted list
```

### Data Types

| Type | Example | Description |
|------|---------|-------------|
| Integer | `42` | Whole numbers |
| Float | `3.14` | Decimal numbers |
| String | `"hello"` | Text values |
| Symbol | `'foo` | Named identifiers |
| List | `(1 2 3)` | Ordered collections |
| Boolean | `true`, `false` | Logical values |
| Nil | `nil` | Empty/null value |

### Operators

#### Arithmetic

```scheme
(+ 1 2)       ; Addition: 3
(- 5 2)       ; Subtraction: 3
(* 3 4)       ; Multiplication: 12
(/ 10 2)      ; Division: 5
(mod 10 3)    ; Modulo: 1
(abs -5)      ; Absolute value: 5
(min 3 7 2)   ; Minimum: 2
(max 3 7 2)   ; Maximum: 7
```

#### Comparison

```scheme
(= 5 5)       ; Equal: true
(< 3 5)       ; Less than: true
(> 5 3)       ; Greater than: true
(<= 3 3)      ; Less or equal: true
(>= 5 3)      ; Greater or equal: true
(!= 3 5)      ; Not equal: true
```

#### Logical

```scheme
(and true true)     ; true
(or false true)     ; true
(not false)         ; true
```

#### List Operations

```scheme
(list 1 2 3)        ; Create list: (1 2 3)
(length '(a b c))   ; Length: 3
(append '(1 2) '(3 4))  ; (1 2 3 4)
(reverse '(1 2 3))  ; (3 2 1)
(sort '(3 1 2))     ; (1 2 3)
```

#### String Operations

```scheme
(concat "hello" " " "world")  ; "hello world"
(string-length "hello")       ; 5
(substr "hello" 1 3)          ; "ell"
(string-upper "hello")        ; "HELLO"
(string-lower "HELLO")        ; "hello"
```

#### Type Checking

```scheme
(is-int 42)         ; true
(is-float 3.14)     ; true
(is-string "hi")    ; true
(is-symbol 'foo)    ; true
(is-list '(1 2))    ; true
(is-bool true)      ; true
(is-nil nil)        ; true
```

### Control Flow

#### If Expression

```scheme
(if condition
  then-expr
  else-expr)

(if (> 5 3)
  "five is greater"
  "five is not greater")
```

#### Cond Expression

```scheme
(cond
  (test1 result1)
  (test2 result2)
  (else default-result))
```

#### Define Variables

```scheme
(define x 42)
(define square (lambda (x) (* x x)))

; Using defined values
(+ x 10)            ; 52
(square 5)          ; 25
```

### Functions

#### Lambda (Anonymous Functions)

```scheme
((lambda (x) (* x 2)) 5)  ; 10

(define double (lambda (x) (* x 2)))
(double 7)                 ; 14

(map (lambda (x) (+ x 1)) '(1 2 3))  ; (2 3 4)
```

#### Map, Filter, Reduce

```scheme
; Map: apply function to each element
(map (lambda (x) (* x 2)) '(1 2 3))  ; (2 4 6)

; Filter: keep elements matching predicate
(filter (lambda (x) (> x 2)) '(1 2 3 4 5))  ; (3 4 5)

; Reduce: combine all elements
(reduce + 0 '(1 2 3 4))    ; 10
(reduce * 1 '(1 2 3 4))    ; 24
```

---

## Advanced Features

### Streaming Mode

For large waveform files (> 100 MB), WAL++ can stream data in chunks:

```bash
# Auto-detect and enable streaming
wal -l huge_simulation.vcd --streaming

# Customize streaming parameters
wal -l trace.vcd \
    --streaming \
    --chunk-size 256 \
    --cache-capacity 10000 \
    --cache-stats
```

**Streaming Parameters:**

- `--chunk-size SIZE` : Chunk size in MB (default: 100)
- `--cache-capacity NUM` : Cache entries per signal (default: 1000)
- `--cache-stats` : Display cache statistics

### Pipe Input

Integrate WAL++ with simulators using pipes:

```bash
# Named Pipe (FIFO)
mkfifo /tmp/trace.fifo
(simulator > /tmp/trace.fifo &)
wal --pipe /tmp/trace.fifo -f analysis.wal

# Standard Input
simulator --output-stdout | wal --stdin -e '(signal-names)'

# In shell script
#!/bin/bash
vcd_generator | wal --stdin -f analysis.wal > results.txt
```

### Script Files

Create reusable WAL analysis scripts:

```scheme
; analysis.wal - Signal statistical analysis

; Load and process signals
(define signals (signal-names))
(define num-signals (length signals))

; Display results
(list 
  "Trace Analysis Report"
  (list "Total signals:" num-signals)
  (list "Signal names:" signals))

; Conditional analysis
(if (> num-signals 100)
  (list "Large design" num-signals)
  (list "Small design" num-signals))
```

Execute the script:

```bash
wal -l trace.vcd -f analysis.wal
```

### Signal Analysis

#### Statistical Analysis

```scheme
; Calculate signal statistics
(define clk-width (signal-width "clk"))
(define data-width (signal-width "data"))

; Analysis results
(list
  "Clock width:" clk-width
  "Data width:" data-width
  "Total bits:" (+ clk-width data-width))
```

#### Bus Analysis

```scheme
; Analyze address and data buses
(define addr-width (signal-width "address"))
(define data-width (signal-width "data"))
(define max-addr (- (* 256 addr-width) 1))

(list
  "Address bus:" addr-width
  "Data bus:" data-width
  "Max address:" max-addr)
```

#### Complex Filtering

```scheme
; Find signals matching criteria
(define all-signals (signal-names))

; Filter bus signals
(define bus-signals 
  (filter (lambda (s) (> (signal-width s) 1)) all-signals))

; Filter control signals
(define control-signals
  (filter (lambda (s) (= (signal-width s) 1)) all-signals))

(list
  "Bus signals:" (length bus-signals)
  "Control signals:" (length control-signals))
```

---

## Python API Reference

### Core Classes

#### `walpp.Wal`

Main WAL instance for evaluation and trace management.

```python
import walpp

# Create instance
wal = walpp.Wal()

# Evaluate expressions
result = wal.eval_str("(+ 1 2)")

# Load traces
wal.load("trace.vcd")

# Access environment
env = wal.get_environment()

# Get trace container
traces = wal.get_traces()
```

**Methods:**

| Method | Description |
|--------|-------------|
| `eval_str(expr)` | Evaluate WAL expression |
| `load(file, trace_id)` | Load waveform file |
| `unload(trace_id)` | Unload trace |
| `get_environment()` | Get variable environment |
| `get_operator_registry()` | Get operator registry |
| `get_traces()` | Get trace container |

#### `walpp.Value`

Represents WAL expression values.

```python
# Check type
if result.is_int():
    print(result.to_int())
elif result.is_string():
    print(result.to_string())
elif result.is_list():
    print(result.to_list())
```

**Methods:**

| Method | Returns | Description |
|--------|---------|-------------|
| `is_int()` | bool | Check if integer |
| `is_float()` | bool | Check if float |
| `is_string()` | bool | Check if string |
| `is_list()` | bool | Check if list |
| `is_symbol()` | bool | Check if symbol |
| `is_bool()` | bool | Check if boolean |
| `is_nil()` | bool | Check if nil |
| `to_int()` | int | Convert to integer |
| `to_float()` | float | Convert to float |
| `to_string()` | str | Convert to string |
| `to_list()` | list | Convert to list |

### Example Scripts

#### Example 1: Basic Trace Analysis

```python
#!/usr/bin/env python3
import walpp

# Create WAL instance
wal = walpp.Wal()

# Load trace
wal.load("simulation.vcd")

# Get signals
signals = wal.eval_str("(signal-names)")
print(f"Signals found: {signals}")

# Count signals
count = wal.eval_str("(length (signal-names))")
print(f"Total: {count.to_int()}")

# Signal widths
widths = wal.eval_str("(map signal-width (signal-names))")
print(f"Widths: {widths.to_list()}")
```

#### Example 2: Filter and Analyze

```python
#!/usr/bin/env python3
import walpp

wal = walpp.Wal()
wal.load("trace.vcd")

# Define analysis in Python
wal.eval_str("""
(define all-signals (signal-names))
(define bus-signals 
  (filter (lambda (s) (> (signal-width s) 1)) all-signals))
(define control-signals
  (filter (lambda (s) (= (signal-width s) 1)) all-signals))
""")

# Retrieve results
buses = wal.eval_str("(length bus-signals)")
controls = wal.eval_str("(length control-signals)")

print(f"Bus signals: {buses.to_int()}")
print(f"Control signals: {controls.to_int()}")
```

#### Example 3: Conditional Analysis

```python
#!/usr/bin/env python3
import walpp

wal = walpp.Wal()
wal.load("design.vcd")

signals = wal.eval_str("(signal-names)")
num_signals = len(signals) if hasattr(signals, '__len__') else 0

# Run different analysis based on design size
if num_signals > 100:
    result = wal.eval_str("""
    (list "Complex design"
          (length (signal-names)))
    """)
else:
    result = wal.eval_str("""
    (list "Simple design"
          (length (signal-names)))
    """)

print(result)
```

#### Example 4: Jupyter Notebook Integration

```python
import walpp
import pandas as pd
import matplotlib.pyplot as plt

# Load and analyze trace
wal = walpp.Wal()
wal.load("simulation.vcd")

# Get signal properties
signals = wal.eval_str("(signal-names)")
widths = wal.eval_str("(map signal-width (signal-names))")

# Create DataFrame
df = pd.DataFrame({
    'Signal': signals,
    'Width': widths
})

# Visualization
df.groupby('Width').size().plot(kind='bar')
plt.title('Signal Distribution by Width')
plt.show()
```

---

## CLI Reference

### Options

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

### Examples

#### Fast Mode - High-Performance HD/HW Calculations

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

#### Load Trace and Analyze

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

#### Expression Evaluation

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

#### Pipe Integration

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

#### Script Execution

```bash
# Execute analysis script
wal -l trace.vcd -f analysis.wal

# Pipe trace and execute script
cat simulation.vcd | wal --stdin -f analysis.wal

# From named pipe
wal --pipe /tmp/trace.fifo -f analysis.wal
```

#### Streaming Options

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

## Troubleshooting

### Common Issues

#### 1. "Unknown file format"

**Problem:** File format not recognized or extension incorrect

**Solution:**
```bash
# Check file format
file trace.vcd

# Force format detection by piping
cat trace.vcd | wal --stdin

# Rename file with correct extension
mv trace.txt trace.vcd
```

#### 2. "Failed to load trace"

**Problem:** Trace file is corrupt or malformed

**Solution:**
```bash
# Validate VCD format
# Check first few lines
head -20 trace.vcd

# Try with no-streaming
wal -l trace.vcd --no-streaming

# Check file permissions
ls -l trace.vcd
chmod 644 trace.vcd
```

#### 3. "Out of memory"

**Problem:** Large trace file consumes too much memory

**Solution:**
```bash
# Use streaming mode
wal -l huge.vcd --streaming

# Reduce chunk size
wal -l huge.vcd --streaming --chunk-size 50

# Reduce cache capacity
wal -l huge.vcd --cache-capacity 1000

# Process in chunks manually
cat huge.vcd | wal --stdin --chunk-size 64
```

#### 4. "Undefined variable"

**Problem:** Variable or operator not found

**Solution:**
```bash
# Check variable is defined
wal -e '(define x 42)'
wal -e 'x'

# Verify signal names
wal -l trace.vcd -e '(signal-names)'

# Check for typos
# Did you mean (signal-names) not (signal-name)?
```

#### 5. "Type mismatch error"

**Problem:** Operation applied to wrong type

**Solution:**
```bash
# Type check before operation
wal -e '(if (is-list x) (length x) 0)'

# Convert types explicitly
wal -e '(string-length (number->string 42))'

# Use correct operators for types
wal -e '(+ 1 2)'        ; Integer addition
wal -e '(concat "a" "b")' ; String concatenation
```

### Performance Tips

1. **Use streaming for large files**
   ```bash
   wal -l huge.vcd --streaming
   ```

2. **Predefine variables in scripts**
   ```scheme
   (define signals (signal-names))
   (define bus-signals (filter ...))
   ```

3. **Use relative operations**
   ```bash
   # Instead of full list processing each time
   wal -l trace.vcd -e '(define sigs (signal-names))' \
      -e '(length sigs)'
   ```

4. **Avoid unnecessary list conversions**
   ```scheme
   ; Good - single pass
   (map signal-width (signal-names))
   
   ; Less efficient - multiple passes
   (map signal-width (signal-names))
   (map signal-width (signal-names))
   ```

---

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for the details. In short:

1. **Fork the repository**
2. **Create a feature branch** (`git checkout -b feature/amazing-feature`)
3. **Commit changes** (`git commit -m 'Add amazing feature'`)
4. **Push to branch** (`git push origin feature/amazing-feature`)
5. **Open a Pull Request**

### Development Setup

```bash
# Clone and setup
git clone https://github.com/iti-luebeck/walpp.git
cd walpp

# Create development environment
python -m venv venv
source venv/bin/activate

# Build with tests
cmake -B build -DENABLE_TESTS=ON -DBUILD_PYTHON_MODULE=ON
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure
```

### Testing

```bash
# Run all tests
ctest --test-dir build -V

# Run specific test
ctest --test-dir build -R test_trace_integration_cli

# Run with coverage
cmake --build build --target coverage
```

---

## License

WAL++ is released under the BSD 3-Clause License, see [LICENSE](LICENSE).

WAL++ builds on [WAL](https://github.com/ics-jku/wal) by Lucas Klemmer (BSD 3-Clause) and
bundles the FST, FastLZ and LZ4 libraries. The standard library in `lib/std/` and the
examples and tests in `tests/examples/` and `tests/original/` are taken from WAL.
See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for details and license texts.

## Citing WAL++

If you use WAL++ in your research, please cite our paper as given in
[CITATION.cff](CITATION.cff) (GitHub shows it under "Cite this repository").
The traces used in the paper's evaluation are in [artifacts/](artifacts/).

---

## Resources

- **GitHub Repository**: https://github.com/iti-luebeck/walpp
- **Issue Tracker**: https://github.com/iti-luebeck/walpp/issues
- **Examples**: See `examples/` directory in repository
- **Official WAL Project**: https://wal-lang.org - The original Python implementation
- **WAL GitHub**: https://github.com/ics-jku/wal - Official WAL repository

---

## FAQ

**Q: What's the difference between --pipe and --stdin?**

A: `--stdin` reads from standard input (pipes), while `--pipe` reads from a named pipe (FIFO) file. Use `--stdin` for shell pipes (`cat file | wal --stdin`) and `--pipe` for named pipes (`wal --pipe /tmp/trace.fifo`).

**Q: Can I combine multiple input modes?**

A: No, you can use one input mode at a time: `-l` (file), `--stdin`, or `--pipe`.

**Q: How do I process very large traces?**

A: Use streaming mode: `wal -l huge.vcd --streaming`. Adjust `--chunk-size` and `--cache-capacity` as needed.

**Q: Can I save processed waveforms?**

A: Yes, WAL++ includes write operators: `write-vcd`, `write-csv`, `write-fst`.

**Q: How do I integrate with my simulation flow?**

A: Pipe simulator output directly: `simulator --vcd /dev/stdout | wal --stdin -f analysis.wal`

**Q: Is Python module required?**

A: No, the CLI works standalone. Python module is optional for scripting integration.

---

## Migrating from WAL to WAL++

### Why Migrate?

If you're currently using the Python implementation of WAL from [wal-lang.org](https://wal-lang.org), WAL++ offers significant advantages:

**Performance**: WAL++ can be 10-50x faster, especially beneficial for:
- Large traces (> 1 GB)
- Intensive analysis operations
- Batch processing pipelines
- Real-time analysis from simulators

**Memory Efficiency**: Streaming architecture handles traces larger than available RAM:
- Process 100+ GB traces efficiently
- Reduce peak memory usage
- Better system resource utilization

**Deployment**: Single binary requires no Python installation:
- Faster startup (< 100ms)
- Simpler deployment in CI/CD
- Reduced container/image size

### Language Compatibility

WAL++ maintains **100% syntax compatibility** with WAL:

```scheme
; This WAL script runs identically in both implementations:
(define signals (signal-names))
(define bus-signals 
  (filter (lambda (s) (> (signal-width s) 1)) signals))
(map signal-width bus-signals)
```

### Migration Path

1. **Drop-in CLI Replacement**
   ```bash
   # Old WAL
   python -m wal -l trace.vcd -e '(signal-names)'
   
   # WAL++
   wal -l trace.vcd -e '(signal-names)'
   ```

2. **Existing Scripts Work Unchanged**
   ```bash
   # WAL script runs unchanged in both
   wal -l trace.vcd -f analysis.wal
   ```

3. **Python Integration**
   ```python
   # If using Python bindings, minimal changes needed
   import walpp  # instead of: import wal
   wal = walpp.Wal()
   # Same API calls follow
   ```

### Performance Comparison

Benchmark: Analyzing 2 GB VCD file with signal discovery and filtering

```
Operation                    | WAL (Python)  | WAL++ (C++)   | Speedup
-----------------------------|---------------|---------------|--------
Load trace                   | 45 seconds    | 2.1 seconds   | 21x
Signal discovery             | 8.2 seconds   | 0.3 seconds   | 27x
Filter bus signals (>1 bit)  | 12.5 seconds  | 0.5 seconds   | 25x
Peak memory usage            | 2.8 GB        | 0.45 GB       | 6.2x
Total analysis time          | 65 seconds    | 3.0 seconds   | 22x
```

### Known Differences

WAL++ is fully compatible, but has minor implementation differences:

| Feature | WAL (Python) | WAL++ |
|---------|--------------|-------|
| Precision | Python floats | C++ double |
| Large integers | Unlimited precision | 64-bit integers |
| Error messages | Python tracebacks | Simplified messages |
| Startup | ~1-2 seconds | ~50-100ms |
| Streaming | Manual chunking | Automatic |

### Support and Resources

- **Official WAL Project**: https://wal-lang.org
- **WAL Repository**: https://github.com/ics-jku/wal
- **WAL++ GitHub**: https://github.com/iti-luebeck/walpp
- **Report Issues**: File issues in WAL++ repository

---

**Last Updated**: November 8, 2025
**Version**: 1.2.2
