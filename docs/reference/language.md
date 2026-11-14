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

That is upstream's behavior and it is useful for windowing an analysis, but it
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
| Boolean | `#t`, `#f` | Logical values. `true` and `false` are bound to them, and are also how one prints |
| Nil | prints as `nil` | Empty/null value. There is no `nil` literal: `(if #f 1)` produces one |

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
`not`. The literals are `#t` and `#f`, which is also how a boolean prints. `true` and
`false` are bound to them as ordinary globals, the way upstream binds them, so an
upstream script written with those names runs. There is no `nil` literal, though nil
is what prints when there is no value: `(if #f 1)` produces one to test with.

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

## One analysis over repeated structure: groups and scopes

A design repeats itself. Every handshake interface has a `valid` and a `ready`, every
core has the same pipeline registers. Writing the analysis once and applying it to all
of them is what groups and scopes are for, and it is the part of WAL that has no
equivalent in a waveform viewer.

![Three interfaces share a naming convention. groups finds all three prefixes without
any of them being written into the query, and in-groups then evaluates one expression
once per group, so a single line yields one answer per
interface.](../images/groups-and-scopes.svg)

Everything below runs in the [playground](../playground.md) against `fabric.vcd`,
which has three such interfaces. Open **One analysis, every interface** there to
change the query and see the answers move.

### Groups

`groups` answers a structural question: which name prefixes exist such that *every*
suffix you name exists under them.

```scheme
(groups "valid" "ready")
```

On `fabric.vcd` that gives `("top.cmd_" "top.in_" "top.out_")`, because each of those
prefixes has both a `valid` and a `ready` under it. Nothing was hardcoded, so the same
expression finds the interfaces in a design you have not seen. A prefix with only one
of the two suffixes is not a group, which is what makes the answer structural rather
than a name search.

`in-groups` then evaluates a body once per group, and inside it `#name` resolves
against the current group while `CG` is the group itself:

```scheme
(in-groups (groups "valid" "ready") (list CG (count (&& #valid #ready))))
```

That is a complete transaction count for every handshake interface in the design:

```
(("top.cmd_" 70) ("top.in_" 32) ("top.out_" 52))
```

One answer per group, which is how the manual types `in-groups`. `#valid` is shorthand
for `(resolve-group valid)`, which takes its argument unevaluated the way the `#`
reader macro does, and `CG` is the current group, so each answer says which interface
it belongs to.

`in-group` does the same for a single prefix you already have:

```scheme
(in-group "top.in_" (count (&& #valid #ready)))
```

### Scopes

A scope is a level of the design hierarchy rather than a name prefix. `SCOPES` lists
them, and `in-scope` evaluates a body with one selected:

```scheme
SCOPES                                            ; ('top)
(in-scope 'top (list CS (length LOCAL-SIGNALS)))  ; ("top" 10)
(in-scope 'top ~clk)                              ; 0
```

Inside a scope, `CS` is its name, `LOCAL-SIGNALS` is the signals directly in it, and
`~name` reads a signal relative to it, so `~clk` is `top.clk`. `all-scopes` returns
the same list as `SCOPES`, for when a call reads better than a variable. It is a
WAL++ spelling in practice: upstream registers it but raises when it is called.

`set-scope` and `unset-scope` do it as statements rather than around a body, which
suits a REPL session:

```scheme
(set-scope 'top)
LOCAL-SIGNALS
(unset-scope)
```

Scopes answer "the same question in every module", groups answer "the same question
for every interface", and they compose: a group search inside a scope only finds
prefixes directly in that scope.

## Waveform handling

A trace loaded with `-l` is `t0`, then `t1`, which is upstream's scheme, so `t0^cpu.clk`
names a signal explicitly when more than one trace is open. `load` takes an id of your
own, as a string or a symbol:

```lisp
(load "trace.vcd")             ; becomes t0
(load "after.vcd" 'after)      ; becomes after
(loaded-traces)                ; ("after" "t0")
(unload 'after)
```

`step` moves the cursor. With no argument it advances every trace by one, with a count
it advances them all by that much, and with ids it advances only those. The count goes
last, as the manual writes it, though either order is accepted because a count is an
integer and an id never is:

```scheme
(step)                ; every trace, one index
(step 5)              ; every trace, five
(step 'after 5)       ; only `after`
```

It returns false once a trace cannot advance, which is what makes `(while (step 1) ...)`
terminate.

`alias` gives a signal a short name, which is worth it for a deep hierarchy:

```lisp
(alias clk "tb.dut.core.clock")
(count (rising clk))
(unalias clk)
```

### Stepping without losing your place

`timeframe` records every trace's index, evaluates the body, and restores them:

```scheme
(list INDEX (timeframe (step 10) INDEX) INDEX)   ; (0 10 0)
```

That is what overlapping transactions need. When one starts, you step forward to find
its end, then resume from where it began to find the next one. Without it, the search
for the end of the first transaction has already consumed the start of the second, and
the second is never seen.

![Two transactions overlap. The search for the end of the first carries the cursor
past the start of the second, so without timeframe the scan resumes too late and
misses it. With timeframe the cursor is restored after the body
runs.](../images/timeframe.svg)

Run it in the [playground](../playground.md) as **Step forward without losing your
place**.

## Reading a signal by name, and reading its bits

A bare symbol reads a signal, but a computed name needs `get-signal`, which takes a
string or a symbol:

```scheme
(get-signal "cpu.clk")                      ; 0
(map (fn [s] (get-signal s)) (signal-names))
(signal? "cpu.clk")                         ; true
(signal-width "cpu.count")                  ; 8
```

`get` is upstream's spelling of the same thing.

`slice` reads a bit field, inclusive and upper bit first, the way hardware is written.
With one index it reads one bit:

```scheme
(set-index 62)
cpu.count               ; 31, so the bits are 00011111
(slice cpu.count 7 4)   ; 1    the high nibble, 0001
(slice cpu.count 3 0)   ; 15   the low nibble, 1111
(slice cpu.count 0)     ; 1    bit 0
```

![An eight bit bus holding 00011111. slice 7 4 selects the high nibble and reads as 1,
slice 3 0 selects the low nibble and reads as 15, and a single index reads one
bit.](../images/bit-slice.svg)

This is how a field is pulled out of a bus without arithmetic, and it is how an
opcode or an address range gets decoded. On a list or a string, `slice` indexes and
slices as Python does, so `(slice '(1 2 3 4 5) 1 3)` is `(2 3)`.

Run it in the [playground](../playground.md) as **Pull a field out of a bus**.

`reval` evaluates an expression at an offset from the cursor, and `expr@off` is
shorthand for it. An offset past either end of the trace reads as `#f` rather than
failing, which is what lets the change-detection idiom work at index 0:

```scheme
(whenever (!= cpu.count cpu.count@-1)
  (printf "%d,%d\n" INDEX cpu.count))
```

## Arrays as accumulators

Arrays are string-keyed maps, and they are how a histogram or a per-signal tally gets
built during a sweep.

```scheme
(define hits (array))
(seta hits "read" 3)
(geta hits "read")                       ; 3
(geta/default hits 0 "write")            ; 0, because "write" is absent
(dela hits "read")
(mapa (fn [k v] (list k v)) (array ["hits" 3] ["misses" 1]))
```

`geta/default` is the one that matters in a loop: `geta` raises on a missing key, so
counting without it means checking first. The argument order is the array, then the
default, then the key. `mapa` turns an array into a list of pairs, which is how a
tally gets printed or sorted at the end.

A histogram built during a sweep is the usual shape, and it is in the
[playground](../playground.md) as **Build a histogram while sweeping**:

```scheme
(define tally (array))
(define nibble (lambda () (convert/bin (slice cpu.count 7 4) 4)))
(whenever (rising cpu.clk) (seta tally (nibble) (+ 1 (geta/default tally 0 (nibble)))))
(mapa (lambda (k v) (list k v)) tally)
```

## Printing

`print` writes its arguments with no separator and a newline. A string contributes its
own characters, so it labels output directly:

```scheme
(print "Total: " 42)         ; Total: 42
(print (list "a" "b"))       ; ("a" "b")
```

A string reached through a list or an array is quoted, so a printed collection still
shows which elements are strings.

`printf` takes a C-style format string, following Python's rules:

```scheme
(printf "%d,%s,%04x,%.2f\n" 42 "ok" 255 3.14159)   ; 42,ok,00ff,3.14
```

For machine-readable output, `printf` is the one to reach for, because the format
string is the contract.

## Types and conversions

```scheme
(int? 1)        (string? "s")    (list? (list 1))    (symbol? 'x)
(atom? 1)       ; true    anything that is not a list
(atom? (list 1)); false
(type 1)        ; 'int
(convert/bin 5)   ; "101"
(convert/bin 5 8) ; "00000101"   padded to a width
```

`convert/bin` with a width is how a value becomes a bit pattern of the right size,
which `bits->sint` then reads back as a signed number. That pair is what `signed`
in the standard library is built from.

`is-int`, `is-bool` and the other `is-` spellings are WAL++'s own and do the same
thing as the `?` forms. The `?` forms are upstream's, so prefer them in anything
meant to run on both.

## More list operations

```scheme
(second (list 1 2 3))        ; 2
(last (list 1 2 3))          ; 3
(rest (list 1 2 3))          ; (2 3)
(zip (list 1 2) (list 3 4))  ; ((1 3) (2 4))
(average (list 1 2 3 4))     ; 2.5
(fold + 0 (list 1 2 3))      ; 6
```

`zip` against a list of indices is a quick way to pair a measurement with when it
happened.

## Running other files

```lisp
(eval-file "lib/helpers")      ; resolves lib/helpers.wal
(require 'lib/helpers)         ; same, but only once per run
(exit 1)
```

`eval-file` adds the `.wal` suffix when the name as given is not a file, so both
spellings work. `require` is the one to use for a shared library of definitions,
because requiring it twice does not run it twice.

---
