# WAL++ API Quick Reference

## Core Entry Points

### Main Interface: `Wal`
```cpp
// Create WAL instance
auto wal = std::make_shared<Wal>();

// Load waveform files
wal->load("trace.vcd");      // Supports: .vcd, .fst, .csv

// Evaluate expressions
Value result = wal->eval_str("(+ 1 2)");

// Execute programs
Value program = wal->run_file("program.wal");

// Start interactive shell
wal->start_repl();
```

**Documentation**: `@ref Wal` in `@ref Core`

---

## AST Value Types

### Universal Value Type
```cpp
Value val;
if (val.is_int()) {
    int64_t i = val.to_int();
} else if (val.is_symbol()) {
    auto sym = val.to_symbol();
} else if (val.is_list()) {
    auto lst = val.to_list();
}
```

**Type Categories:**
- Scalars: nil, bool, int64_t, double, string
- Composite: Symbol, WList, Closure, Macro
- Context: Environment, UserOperator

**Documentation**: `@ref Value` and `@ref AST`

### Symbol - Variable/Signal Names
```cpp
auto sym = std::make_shared<Symbol>("signal_name");
sym->get_name();        // "signal_name"
sym->get_scope();       // Scope prefix (~module.signal)
```

**Documentation**: `@ref Symbol`

### WList - Lisp-like Lists
```cpp
auto lst = std::make_shared<WList>();
lst->push_back(Value(1));
lst->push_back(Value(2));
```

**Documentation**: `@ref WList`

### Closure - Lambda Functions
```cpp
// Closures capture environment and body
// Created by: (lambda (x y) (+ x y))
// Called as: (closure 1 2)
```

**Documentation**: `@ref Closure`

### Environment - Scope Chains
```cpp
auto env = std::make_shared<Environment>();
env->define("x", Value(42));
Value x = env->get("x");
```

**Documentation**: `@ref Environment`

---

## Evaluator

### SEval - Expression Evaluator
```cpp
auto eval = std::make_shared<SEval>(traces);

// Evaluate S-expression
Value result = eval->eval(parsed_expr);

// Evaluate multiple arguments
auto results = eval->eval_args(argument_list);

// Evaluate closure (lambda)
Value lambda_result = eval->eval_closure(closure, args);
```

**Documentation**: `@ref SEval` in `@ref evaluator`

**Evaluation Pipeline:**
1. Parse S-expression
2. Dispatch to operator
3. Operator receives unevaluated arguments
4. Operator decides when to evaluate
5. Return result

---

## Operator Registry

### OperatorFunc Signature
```cpp
using OperatorFunc = std::function<Value(
    SEval& evaluator,
    const std::vector<Value>& args
)>;
```

All operators follow this signature for unified dispatch.

### Register Custom Operator
```cpp
registry.register_operator("+", [](SEval& eval, const std::vector<Value>& args) {
    auto evaluated = eval.eval_args(args);
    int64_t sum = 0;
    for (auto& val : evaluated) {
        sum += val.to_int();
    }
    return Value(sum);
});
```

### Special Forms (Unevaluated Arguments)
```cpp
// if: evaluate condition, then eval appropriate branch
registry.register_operator("if", [](SEval& eval, const std::vector<Value>& args) {
    if (eval.eval(args[0]).is_truthy()) {
        return eval.eval(args[1]);
    } else {
        return eval.eval(args[2]);
    }
});
```

**Documentation**: `@ref operator_registry` and `@ref OperatorFunc`

---

## Parser

### Parse S-expressions
```cpp
Parser parser;

// Parse single expression
Value expr = parser.parse("(+ 1 2)");

// Parse entire program
auto program = parser.parse_program("(define x 10) (print x)");
```

**Supported Syntax:**
- Atoms: `42`, `3.14`, `"string"`, `symbol`
- Lists: `(+ 1 2)`, `(if true a b)`
- Quotes: `'x` → `(quote x)`
- Comments: `; comment to end of line`

**Documentation**: `@ref Parser` in `@ref parser`

**Grammar:**
```
program    ::= expression*
expression ::= atom | list | quote
atom       ::= symbol | number | string
list       ::= '(' expression* ')'
quote      ::= '\'' expression
```

---

## Trace System

### Load Traces
```cpp
auto traces = std::make_shared<TraceContainer>();

// Auto-detects format
traces->load("trace.vcd", "my-trace");   // VCD format
traces->load("trace.fst", "trace2");     // FST format
traces->load("trace.csv", "trace3");     // CSV format
```

### Access Signals
```cpp
// Get signal value at current time
Value clk = traces->signal_value("clk");

// Get signal at offset (+10 = 10 cycles ahead, -5 = 5 back)
Value future = traces->signal_value("data", 10);

// Access from specific trace
Value sig = traces->signal_value("my-trace~signal");

// Get signal width
int width = traces->signal_width("data");
```

### Time Navigation
```cpp
// Step all traces forward
traces->step(1);         // Forward 1 cycle
traces->step(10);        // Forward 10 cycles

// Step backward
traces->step(-5);        // Backward 5 cycles

// Step specific trace only
traces->step(1, "my-trace");

// Get current indices
auto indices = traces->indices();  // Map of tid -> index
```

### Special Signals
- `SIGNALS` - list of all signal names
- `INDEX` - current time index
- `MAX-INDEX` - maximum time index
- `TIMESTAMP` (or `TS`) - simulation timestamp
- `SCOPES` - hierarchical scope names

**Documentation**: `@ref trace` and `@ref Trace`

---

## Error Handling

### Exception Hierarchy
```
std::runtime_error
    ↓
WalError (base)
    ├─ ParseError      // Syntax error
    ├─ EvalError       // Runtime error
    └─ TraceError      // Trace loading/access error
```

### Catch Exceptions
```cpp
try {
    wal->eval_str("(undefined-function)");
} catch (const wal::EvalError& e) {
    std::cerr << "Evaluation failed: " << e.what() << std::endl;
} catch (const wal::ParseError& e) {
    std::cerr << "Parse failed: " << e.what() << std::endl;
} catch (const wal::TraceError& e) {
    std::cerr << "Trace error: " << e.what() << std::endl;
} catch (const wal::WalError& e) {
    std::cerr << "WAL error: " << e.what() << std::endl;
}
```

**Documentation**: `@ref Errors` in `@ref AST`

---

## CLI and REPL

### Command Line Interface
```bash
# Evaluate expression
./wal -e "(+ 1 2)"

# Run WAL program file
./wal -f program.wal
./wal program.wal

# Load and execute
./wal -l trace.vcd program.wal

# Start interactive REPL
./wal
```

**Documentation**: `@ref CLI` in `@ref Core`

### Interactive REPL
```
wal> (+ 1 2)
3
wal> (load "trace.vcd")
[Trace loaded: t0]
wal> (signal "clk")
0
wal> (step)
wal> (signal "clk")
1
wal> (quit)
Goodbye!
```

**Documentation**: `@ref REPL` in `@ref Core`

---

## Architecture Overview

```
Input (String)
    ↓
Parser
    ↓ (Value/AST)
SEval (Evaluator)
    ↓
OperatorRegistry (Dispatch)
    ├─ Builtin Operators (50+)
    ├─ User Operators
    └─ Special Forms (if, lambda, etc.)
    ↓
Environment (Variable Bindings)
    ↓
TraceContainer (Signal Access)
    ├─ Trace 1 (VCD, FST, CSV)
    ├─ Trace 2
    └─ Trace N
    ↓
Result (Value)
```

**Documentation**: Architecture diagrams in `@ref Core`, `@ref evaluator`, `@ref operator_registry`

---

## Documentation Resources

The full API, generated from the headers, is under the **API** tab, or at
[`api/`](../api/index.md). Every type named on this page has an entry there with its
members.

It is built by [doxide](https://doxide.org), not Doxygen, from the `files:` globs in
`doxide.yaml`:

```bash
make docs          # doxide build, then mkdocs build, into build/site
open build/site/api/index.html
```

See also:

- [Embedding WAL++ in a C++ program](guides/cpp-api.md) for the same entry points
  with working code, lifetimes and threading
- [Writing your own extraction tool](guides/writing-fast-tools.md) for the
  non-interpreter path

---

## Common Patterns

### Pattern 1: Load, Query, Step
```cpp
auto wal = std::make_shared<Wal>();
wal->load("trace.vcd");
for (int i = 0; i < 100; ++i) {
    Value clk = wal->eval_str("(signal \"clk\")");
    // Process clk value
    wal->eval_str("(step)");
}
```

### Pattern 2: Define Functions
```cpp
wal->eval_str("(define square (lambda (x) (* x x)))");
Value result = wal->eval_str("(square 5)");  // 25
```

### Pattern 3: Conditional Logic
```cpp
wal->eval_str("(if (= (signal \"enable\") 1) "
              "  (print \"enabled\") "
              "  (print \"disabled\"))");
```

### Pattern 4: Custom Operators
```cpp
// Register custom operator
auto& registry = wal->get_operator_registry();
registry.register_operator("double", 
    [](SEval& eval, const std::vector<Value>& args) {
        int val = eval.eval(args[0]).to_int();
        return Value(val * 2);
    });
wal->eval_str("(double 21)");  // 42
```

