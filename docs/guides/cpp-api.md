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

Two things to know, both of which differ from what a C++ programmer might assume and
both of which are deliberate:

**Types do not coerce.** `v.to_int()` on a float value throws rather than truncating.
See [Compatibility with upstream WAL](../reference/wal-compatibility.md).

**A signal wider than 64 bits reads as a string** holding its exact bit pattern,
because no `int64_t` holds it. Check `is_string()` on wide buses, and use the
`bits->sint` operator to interpret one as signed.

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

A `Wal` instance is **not** thread-safe. Give each thread its own. The trace readers
hold mutable cursor state, so sharing one across threads will produce wrong answers
rather than a crash, which is the worse failure.

Parallelising a campaign across files, one `Wal` per worker, is the intended shape
and scales linearly.

## See also

- [Writing your own extraction tool](writing-fast-tools.md) for the single-pass path
- [API quick reference](../API_QUICK_REFERENCE.md) for more entry points
- [Python API](../reference/python-api.md) if scripting suits better
