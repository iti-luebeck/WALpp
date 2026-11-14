# Embedding WAL++ in a C++ program

Link the library, load a trace, run queries, and add operators of your own.

Every example here was compiled and run against an installed WAL++ before being
written down.

## Linking

WAL++ installs a CMake package, so a consumer links one target:

```cmake
cmake_minimum_required(VERSION 3.20)
project(edge_count CXX)
set(CMAKE_CXX_STANDARD 20)

find_package(walpp REQUIRED)

add_executable(edge_count main.cpp)
target_link_libraries(edge_count PRIVATE walpp::walpp)
```

```bash
cmake --install build --prefix /opt/walpp     # from the WAL++ build
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/walpp
cmake --build build
```

`walpp::walpp` carries its own include directories and its `Threads` dependency, so
nothing else needs configuring.

To build WAL++ as part of your own tree instead, `add_subdirectory(walpp)` and link
the plain `walpp` target.

C++20 is required: the library uses concepts and designated initializers.

## A first program

```cpp
#include "wal/core.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: edge_count <trace> <signal>\n";
        return 1;
    }

    wal::Wal w;
    w.load(argv[1], "t");

    std::string query = "(count (rising " + std::string(argv[2]) + "))";
    wal::Value result = w.eval_str(query);

    std::cout << argv[2] << ": " << result.to_int() << " rising edges\n";
    std::cout << "trace has " << w.eval_str("MAX-INDEX").to_int() + 1 << " indices\n";
    return 0;
}
```

```
$ ./edge_count counter.vcd tb.clk
tb.clk: 40 rising edges
trace has 81 indices
```

`wal::Wal` is the whole entry point. It owns a trace container and an evaluator, and
loads the standard library the first time something is evaluated.

## Evaluating

| | |
|---|---|
| `eval_str(code)` | parse and evaluate **one** expression |
| `eval_program(code)` | parse and evaluate **every** expression in the text, returning the last |
| `run_file(path)` | the same, read from a file |
| `eval(value)` | evaluate an already-parsed `Value` |

Use `eval_program` for anything multi-line. `eval_str` stops after the first
expression, which is the right behavior for a single query and the wrong one for a
script.

Errors come back as exceptions derived from `std::exception`, carrying the message
the CLI would print:

```cpp
try {
    w.eval_str("(count (rising no.such.signal))");
} catch (const std::exception& e) {
    std::cerr << "query failed: " << e.what() << "\n";
}
```

## Working with values

`wal::Value` is a tagged union. Ask before you take:

```cpp
wal::Value v = w.eval_str("(+ 1 2)");

if (v.is_int())        int64_t n   = v.to_int();
else if (v.is_float()) double d    = v.to_float();
else if (v.is_string()) const std::string& s = v.to_string();
else if (v.is_list()) {
    auto list = v.to_list();
    for (const auto& item : *list) { /* ... */ }
}
```

`to_string_repr()` renders any value the way the REPL would, which is what you want
for logging.

### The conversion contract

The accessors are not coercions. Each one either gives you the value or throws, with
two exceptions worth memorising. Measured, not inferred:

| value | `to_int()` | `to_float()` | `to_bool()` | `is_truthy()` |
|---|---|---|---|---|
| `42` | 42 | 42 | **throws** | true |
| `3.7` | **3** | 3.7 | throws | true |
| `-3.7` | **-3** | -3.7 | throws | true |
| `"hi"` | throws | throws | throws | true |
| `#t` / `#f` | throws | throws | true / false | true / false |
| `#nil` | throws | throws | **throws** | false |
| `(1 2)` | throws | throws | throws | true |
| a bigint too large for 64 bits | **throws** | lossy | throws | true |

Three of those will catch you out:

**`to_bool()` throws on anything that is not a bool**, including `42`. It is an
accessor, not a truth test. For WAL's notion of truth, which is what `if` and `&&`
use, call `is_truthy()`: it never throws, and it is false for `0`, `0.0`, `""`, `()`,
`#f` and nil.

**`to_int()` truncates a float toward zero** rather than refusing. It is the one
place the C++ API is looser than the language, which has no implicit conversion
between an int and a float at all. If you care, check `is_int()` first.

**`to_float()` on a bigint is silently lossy** above 2^53. `to_int()` on one that
does not fit throws instead, deliberately: a value that overflows 64 bits is exactly
the case `--bigint` exists for, so wrapping it would defeat the point.

## Decisions the API inherits

WAL++ makes a handful of deliberate choices that a C++ caller meets directly. Each
is explained in full in
[Compatibility with upstream WAL](../reference/wal-compatibility.md); this is what
each one means from inside a program.

### A signal wider than 64 bits comes back as a string

No `int64_t` holds it, so the reader returns the exact bit pattern as text:

```cpp
wal::Value v = traces->signal_value("top.core.wide_bus", 0);
if (v.is_string()) {
    // 128 characters of '0' and '1'. Count bits, or convert:
    int64_t signed_value = w.eval_str("(bits->sint \"" + v.to_string() + "\")").to_int();
}
```

This is the decision everything else about types follows from. A `+` that coerced
strings would turn arithmetic on a wide bus into string concatenation and return a
plausible wrong answer, which on a datapath is the worst available failure. So
nothing coerces, and `is_string()` on a wide bus is a check worth writing.

For counting bits across many signals, do not do it through `Value` at all: the
extraction kernels handle wide signals exactly and in one pass. See
[Writing your own extraction tool](writing-fast-tools.md).

### Modes are process-wide, and read at evaluation time

`set_bigint_mode` and `set_compat_types_mode` are **not** per-instance:

```cpp
wal::Wal strict;                    // created first
wal::set_compat_types_mode(true);
wal::Wal loose;

strict.eval_str("(= 0 #f)");        // true  -- the flag applies to it too
loose.eval_str("(= 0 #f)");         // true

wal::set_compat_types_mode(false);
loose.eval_str("(= 0 #f)");         // false -- an existing instance changed behavior
```

They are global because the **parser** consults them, and the parser is used in
several places that have no interpreter to ask. Two consequences:

- You cannot run one instance strict and another loose in the same process. If you
  need both, use two processes.
- Set them once, during start-up, before any thread or any evaluation. Changing a
  mode while something is evaluating is safe in the data-race sense, since both are
  atomic, but the result is whichever value the read happened to see.

### `eval_str` evaluates one expression

```cpp
w.eval_str("(define x 1) (define y 2)");   // defines x; y is never seen
w.eval_program("(define x 1) (define y 2)");  // defines both, returns the last
```

This catches people out because the single-expression form is the right one for a
query and the wrong one for anything with more than one form in it. Use
`eval_program` or `run_file` for scripts.

### Signal names: `.` for hierarchy, `^` for a trace id

```cpp
w.load("before.vcd", "a");
w.load("after.vcd",  "b");
w.eval_str("a^top.core.counter");     // the counter in trace `a`
```

`top.core.counter` is a path; `a^` selects which trace it is read from. The
extraction modes and upstream WAL spell it the same way. Names written with the old
`^`-joined hierarchy still resolve, so an existing script keeps working.

### `mod` is Euclidean

`(mod -7 3)` is `2`, not `-1`: the result is the least non-negative residue. This
matters if you write an operator that does index arithmetic, because C++'s `%` does
not agree. Wrapping an index that has gone negative is the usual case, and `%` gets
it wrong.

### Two engines, one binary

The interpreter and the `--fast-*` extraction kernels share the file readers and
nothing else. No language semantics apply to the kernels: the type rules, the modes
and the operator set are all interpreter concerns. A kernel sees signal ids and
integers.

That is why the kernels are two orders of magnitude faster, and why changing a mode
cannot affect an extraction result.

## Reading the trace directly

For bulk work, going through the evaluator per sample is wasteful. Reach the trace
layer instead:

```cpp
auto traces = w.get_traces();
auto trace  = traces->get_trace(traces->get_trace_ids().front());

std::cout << trace->get_max_index() << " indices\n";

for (int i = 0; i <= trace->get_max_index(); ++i) {
    trace->set_index(i);
    wal::Value v = traces->signal_value("tb.dut.counter", 0);
    // ...
}
```

`signal_value(name, offset)` reads relative to the current index, so `offset = -1` is
the previous sample. It accepts a name in any accepted spelling.

If this loop is the hot part of your program, you are probably writing an extraction
tool. See [Writing your own extraction tool](writing-fast-tools.md), which skips the
`Value` layer entirely.

## Adding an operator

A C++ operator is a callable registered under a name. It receives its arguments
**unevaluated**, so it decides what to evaluate and when, which is what makes control
flow expressible:

```cpp
#include "wal/core.h"
#include "wal/evaluator.h"

w.get_eval_context()->get_operator_registry().register_operator(
    "duty-cycle",
    [](wal::SEval& eval, const std::vector<wal::Value>& args) -> wal::Value {
        if (args.size() != 1) {
            throw wal::EvalError("duty-cycle expects one argument (a signal name)");
        }
        wal::Value name = eval.eval(args[0]);
        std::string signal = name.is_symbol() ? name.to_symbol()->get_name()
                                              : name.to_string();

        auto traces = eval.get_traces();
        auto trace  = traces->get_trace(traces->get_trace_ids().front());

        int high = 0, total = trace->get_max_index() + 1;
        int saved = trace->get_index();
        for (int i = 0; i < total; ++i) {
            trace->set_index(i);
            if (traces->signal_value(signal, 0).is_truthy()) ++high;
        }
        trace->set_index(saved);            // leave the trace where you found it

        return wal::Value(static_cast<double>(high) / total);
    });
```

```cpp
std::cout << w.eval_str("(duty-cycle \"tb.clk\")").to_float() << "\n";   // 0.493827
```

Four conventions worth following, because every built-in operator does:

- **Check arity first** and throw `wal::EvalError` with a message naming the operator
  and the expected form. That message is what the user sees.
- **Accept a symbol or a string** for a signal name. Upstream scripts write
  `(duty-cycle 'tb.clk)`; a C++ caller writes a string.
- **Restore any trace position you change.** An operator that moves the index and
  leaves it moved breaks the sweep that called it.
- **Return a `Value`**, not a native type. `Value(double)`, `Value(int64_t)`,
  `Value(std::string)` and `Value(std::make_shared<wal::WList>())` all exist.

Register before the first evaluation if the operator might shadow a standard-library
name: the library is loaded lazily on first use, and it skips any definition whose
name is already registered.

## Driving many traces

A `Wal` instance can hold several traces at once, addressed by id:

```cpp
wal::Wal w;
w.load("before.vcd", "a");
w.load("after.vcd",  "b");

auto same = w.eval_str("(= a^MAX-INDEX b^MAX-INDEX)");
```

For a campaign, one `Wal` per trace is usually simpler and lets you parallelise
across files. The object is cheap; the trace is not.

## Threading

**Separate instances are safe to use at the same time. One instance is not.**

That is the whole guarantee, and it is the shape a campaign already has: one trace
per unit of work, one `Wal` per unit of work.

```cpp
std::vector<std::thread> pool;
for (int t = 0; t < threads; ++t) {
    pool.emplace_back([&, t] {
        for (const auto& trace : my_share_of(t)) {
            wal::Wal w;                   // one per unit of work
            w.load(trace, "t");
            record(w.eval_str("(count (rising tb.clk))").to_int());
        }
    });
}
for (auto& t : pool) t.join();
```

Measured on a 16-core machine, one analysis being a load plus a whole-trace sweep:

| threads | per analysis |
|---|---|
| 1 | 1.03 ms |
| 2 | 0.55 ms |
| 4 | 0.29 ms |
| 8 | 0.15 ms |

About sevenfold on eight threads. Construction is not the bottleneck: a bare `Wal`
costs 0.016 ms, and 0.17 ms with the standard library loaded, against roughly 1 ms
for the analysis itself. Do not bother pooling instances.

### Why one instance cannot be shared

A `Trace` carries a cursor. `signal_value(name, offset)` reads relative to the
current index, and `step` and `set-index` move it. Two threads sweeping the same
trace would interleave their positions and get plausible wrong answers rather than a
crash, which is the worse failure. Making this safe would mean taking the index out
of the trace and passing it through every read, which is a different library.

### What is shared, and what it cost

Three pieces of process-wide state used to break even the per-instance guarantee.
All three are fixed, and `tests/implementation/test_concurrency.cpp` holds them down:

- **`gensym`** kept a plain counter, so two threads could be handed the same name.
  Measured at 1314 collisions in 3200 before the fix. Duplicate gensyms break macro
  hygiene silently, as variable capture rather than as an error. Now atomic.
- **`require`** tracked loaded modules process-wide, which was wrong on one thread
  too: a *second* instance skipped a module it had never loaded and never received
  its definitions. Module state now belongs to the interpreter that loaded it.
- **The `--bigint` and `--compat-types` flags** are read on every arithmetic
  operation and may be set from another thread. Now atomic with relaxed ordering,
  which measured no difference in a 400,000-iteration arithmetic loop (728 ms
  against 727 ms).

Set the mode flags once, before starting threads. They are process-wide by design,
because the parser consults them and the parser is used in several places.

## See also

- [Writing your own extraction tool](writing-fast-tools.md) for the single-pass path
- [API quick reference](../API_QUICK_REFERENCE.md) for more entry points
- [Python API](../reference/python-api.md) if scripting suits better
