# Language reference

The S-expression language: values, operators, signal access and the
standard library.

## Control flow, definitions and macros

WAL++ runs upstream WAL's language, including its standard library, so the full set
of forms is available. Earlier versions of this page described a much smaller
language; that is no longer accurate.

```lisp
(if (> 5 3) "bigger" "smaller")        ; "bigger"
(let ([a 1] [b 2]) (+ a b))            ; 3
(when (> 3 2) 7)                       ; 7
(unless (> 2 3) 7)                     ; 7
(cond [(> 1 2) 10] [else 20])          ; 20
(case 2 (1 "one") (2 "two") (default "other"))   ; "two"

(define n 0)
(while (< n 5) (set! n (+ n 1)))       ; n is 5

(defun dbl [x] (* x 2))                ; or (define dbl (fn [x] (* x 2)))
(dbl 21)                               ; 42

(defmacro twice [x] `(+ ,x ,x))        ; quasiquote, unquote, unquote-splicing
(twice 21)                             ; 42
```

Arrays are string-keyed maps, and `[...]` subscripts a list, an array or a string:

```lisp
(define a (array (1 "one") (2 "two")))
(geta a 2)                             ; "two"
(seta a 3 "three")                     ; mutates in place

(define l (list 10 20 30))
l[1]                                   ; 20
```

## Reading signals

A bare symbol that names a signal reads its value at the current index. `@` reads at
an offset from it, and the special variables describe the trace:

```lisp
tb.dut.counter                         ; value now
tb.clk@-1                              ; value one index earlier
INDEX   MAX-INDEX   TS                 ; where we are, how long, simulation time
SIGNALS   SCOPES                       ; what is in the trace
a^tb.dut.counter                       ; the same signal in the trace loaded as `a`
```

The standard library gives the analysis vocabulary, all of it upstream's:

```lisp
(count (rising tb.clk))                ; 40
(count (falling tb.clk))               ; 40
(find (= tb.dut.counter 3))            ; the indices where it held
(whenever (rising tb.clk) (inc cnt))   ; run a body at every matching index
```

### Sweeps start at the cursor

`find`, `whenever`, `count` and `fold/signal` walk from the **current index** to the
end, then put the cursor back where they found it. So a `set-index` before a sweep
narrows it:

```lisp
(count (rising tb.clk))                       ; 40, from index 0
(begin (set-index 40) (count (rising tb.clk))) ; 20, from index 40
```

That is upstream's behaviour and it is useful for windowing an analysis, but it
surprises people who expect a whole-trace answer. `(set-index 0)` first if you want
one.


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

The names are `&&`, `||` and `!`, as in upstream WAL. There is no `and`, `or` or
`not`. The literals are `#t` and `#f`: `true` and `false` are how a boolean
*prints*, not how you write one.

```scheme
(&& #t #t)          ; true
(|| #f #t)          ; true
(! #f)              ; true
```

Any value works, not just booleans. Zero and the empty string are false:

```scheme
(&& 1 1)            ; true
(! 0)               ; true
(! "")              ; true
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
(substring "hello" 1 3)       ; "ell"
(uppercase "hello")           ; "HELLO"
(lowercase "HELLO")           ; "hello"
```

### Type Checking

```scheme
(is-int 42)              ; true
(is-float 3.14)          ; true
(is-string "hi")         ; true
(is-symbol 'foo)         ; true
(is-list '(1 2))         ; true
(is-bool #t)             ; true
(is-nil (if #f 1))       ; true   an `if` with no else arm yields nil
```

Each has a `?` spelling as well, which is upstream's: `int?`, `float?`, `string?`,
`symbol?`, `list?`, `bool?`, `nil?`. They are the same operators.

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
(fold + 0 '(1 2 3 4))      ; 10
(fold * 1 '(1 2 3 4))      ; 24
```

---

## Advanced Features

## Streaming Mode

Three states, which the flags distinguish:

```bash
wal -l trace.vcd                  # stream only if the file is above 100 MB
wal -l trace.vcd --streaming      # always stream
wal -l trace.vcd --no-streaming   # never stream, whatever the size
```

Streaming reads the header once and builds a signal's history only when that signal
is first read, so memory follows what the analysis touches rather than the size of
the trace. Full load decodes everything up front, which is faster for repeated
queries and is why it stays the default below the threshold.

All three give the same answers. See
[Working with very large traces](../guides/large-traces.md) for which to pick.

**Parameters**

- `--chunk-size SIZE` : chunk size in MB (default: 100)
- `--cache-capacity NUM` : cache entries per signal (default: 1000)

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
; Ask the trace rather than naming signals you hope are there
(define names (signal-names))
(define widths (map signal-width names))

(list
  "signals" (length names)
  "widest" (max-val widths)
  "total bits" (sum widths))
```

Against `handshake.vcd`: `("signals" 4 "widest" 8 "total bits" 11)`.

A name can be given in full, `dut.data`, or by its leaf alone, `data`, when that
leaf is unambiguous in the trace.

### Bus Analysis

Width tells a bus from a control line without knowing anything about the design.

```scheme
; Anything wider than one bit is carrying a value, not a flag
(define buses
  (filter (lambda (s) (> (signal-width s) 1)) (signal-names)))
(define control
  (filter (lambda (s) (= (signal-width s) 1)) (signal-names)))

(list "buses" buses "control" control)
```

Against the playground's `handshake.vcd` that gives
`("buses" ('dut.data) "control" ('dut.clk 'dut.valid 'dut.ready))`.

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
