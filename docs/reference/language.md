# Language reference

The S-expression language: values, operators, signal access and the
standard library.

## Basic Syntax

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

## Data Types

| Type | Example | Description |
|------|---------|-------------|
| Integer | `42` | Whole numbers |
| Float | `3.14` | Decimal numbers |
| String | `"hello"` | Text values |
| Symbol | `'foo` | Named identifiers |
| List | `(1 2 3)` | Ordered collections |
| Boolean | `true`, `false` | Logical values |
| Nil | `nil` | Empty/null value |

## Operators

### Arithmetic

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

### Comparison

```scheme
(= 5 5)       ; Equal: true
(< 3 5)       ; Less than: true
(> 5 3)       ; Greater than: true
(<= 3 3)      ; Less or equal: true
(>= 5 3)      ; Greater or equal: true
(!= 3 5)      ; Not equal: true
```

### Logical

```scheme
(and true true)     ; true
(or false true)     ; true
(not false)         ; true
```

### List Operations

```scheme
(list 1 2 3)        ; Create list: (1 2 3)
(length '(a b c))   ; Length: 3
(extend '(1 2) '(3 4))  ; (1 2 3 4)   concatenate whole lists
(append '(1 2) 3)       ; (1 2 3)     add one element (upstream's append)
(reverse '(1 2 3))  ; (3 2 1)
(sort '(3 1 2))     ; (1 2 3)
```

### String Operations

```scheme
(concat "hello" " " "world")  ; "hello world"
(string-length "hello")       ; 5
(substr "hello" 1 3)          ; "ell"
(string-upper "hello")        ; "HELLO"
(string-lower "HELLO")        ; "hello"
```

### Type Checking

```scheme
(is-int 42)         ; true
(is-float 3.14)     ; true
(is-string "hi")    ; true
(is-symbol 'foo)    ; true
(is-list '(1 2))    ; true
(is-bool true)      ; true
(is-nil nil)        ; true
```

## Control Flow

### If Expression

```scheme
(if condition
  then-expr
  else-expr)

(if (> 5 3)
  "five is greater"
  "five is not greater")
```

### Cond Expression

```scheme
(cond
  (test1 result1)
  (test2 result2)
  (else default-result))
```

### Define Variables

```scheme
(define x 42)
(define square (lambda (x) (* x x)))

; Using defined values
(+ x 10)            ; 52
(square 5)          ; 25
```

## Functions

### Lambda (Anonymous Functions)

```scheme
((lambda (x) (* x 2)) 5)  ; 10

(define double (lambda (x) (* x 2)))
(double 7)                 ; 14

(map (lambda (x) (+ x 1)) '(1 2 3))  ; (2 3 4)
```

### Map, Filter, Reduce

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

## Streaming Mode

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

## Pipe Input

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

## Script Files

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

## Signal Analysis

### Statistical Analysis

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

### Bus Analysis

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

### Complex Filtering

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
