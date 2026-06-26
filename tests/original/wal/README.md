# WAL Python Shim

This is a Python implementation of the WAL (Waveform Analysis Language) interpreter that wraps walpp C++ functionality and provides a pure Python AST evaluator for compatibility with the original WAL test suite.

## Modules

### `wal/__init__.py`
Package marker file that makes this directory a Python package.

### `wal/ast_defs.py`
Defines the AST (Abstract Syntax Tree) classes:
- **`Symbol`**: Represents an identifier/variable name
- **`Operator`**: Represents an operator application or list
- **`WalEvalError`**: Exception raised for evaluation errors
- **`WalError`**: Base exception class

### `wal/reader.py`
Implements the S-expression parser and tokenizer:
- **`tokenize(source)`**: Converts WAL source code into tokens
- **`parse_expr(tokens, pos)`**: Parses tokens into an AST
- **`read_wal_sexpr(source)`**: Main entry point for parsing WAL code

### `wal/core.py`
Main evaluator and environment management:
- **`Environment`**: Manages variable bindings
- **`EvalContext`**: Holds the evaluation environment
- **`Wal`**: Main interpreter class with eval methods

## Features Implemented

### Core Language Features
- ✅ Arithmetic operators: `+`, `-`, `*`, `/`, `**`, `%`
- ✅ Comparison operators: `=`, `!=`, `<`, `>`, `<=`, `>=`
- ✅ Logical operators: `and`, `or`, `not`, `&&`, `||`, `!`
- ✅ Bitwise operators: `band`, `bor`, `bxor`, `bnot`, `<<`, `>>`
- ✅ List operations: `list`, `car`, `cdr`, `cons`, `append`, `reverse`, `length`, `nth`
- ✅ Type operations: `type`, `int`, `float`, `str`

### Special Forms
- ✅ `quote`: Prevent evaluation of expressions
- ✅ `define`: Define variables
- ✅ `set`: Update variable values
- ✅ `if`: Conditional evaluation
- ✅ `lambda`/`fn`: Function definitions
- ✅ `let`: Local variable bindings
- ✅ `begin`/`do`: Sequential execution

### Function Features
- ✅ Lambda expressions: `(fn (x) (+ x 1))`
- ✅ Nested function calls: `((fn (x) (+ x 1)) 5)`
- ✅ Closures with captured environment
- ✅ Multi-argument functions

### Error Handling
- ✅ Type checking for operations (e.g., can't compare lists with `<`)
- ✅ Argument count validation
- ✅ Undefined variable detection
- ✅ Proper exception propagation

## Usage

```python
from wal.core import Wal

wal = Wal()

# Evaluate expressions
result = wal.eval_str("(+ 1 2)")  # Returns 3

# Define variables
wal.eval_str("(define x 10)")
result = wal.eval_str("(* x 2)")  # Returns 20

# Use lambda functions
result = wal.eval_str("((fn (x) (+ x 1)) 5)")  # Returns 6

# List operations
result = wal.eval_str("(append '(1 2) '(3 4))")  # Returns [1, 2, 3, 4]
```

## Test Results

**Current Status**: 18/30 tests passing

### Passing Test Classes
- ✅ `BasicOpTest` (15/15): All arithmetic, comparison, and logical operators
- ✅ `EvalFunctionTest` (2/2): Lambda definitions and applications
- ✅ `EvalControlFlowTest.test_do` (1/1): Sequential execution

### Known Limitations / TODO

The following features are not yet implemented:
- `print`, `printf`: Output functions
- `cond`, `case`, `when`, `unless`, `while`: Advanced control flow
- `quote`, `unquote`: Quoting mechanisms
- `load`: Loading trace files
- Trace-related operators: `signal-names`, `signal-width`, etc.
- `alias`, `unalias`: Signal aliasing

These features would require additional implementation work but are not blocking the core evaluation functionality.

## Architecture

The evaluator uses a tree-walking interpreter pattern:

1. **Tokenization**: Source code → Tokens
2. **Parsing**: Tokens → AST (S-expression tree)
3. **Evaluation**: AST → Values (with environment management)

The environment is managed as a dictionary of variable bindings, and closures capture the environment at definition time.

## Error Handling

All errors are raised as `WalEvalError` exceptions with descriptive messages:

```python
try:
    wal.eval_str("(< '(1 2) 3)")  # Type error
except WalEvalError as e:
    print(f"Error: {e}")  # "Type error in <: cannot compare list and int"
```

## Performance Notes

This is a pure Python implementation designed for correctness and compatibility, not performance. For production use with large datasets, use the C++ walpp library directly.
