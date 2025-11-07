#pragma once

#include <variant>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <stdexcept>
#include <iostream>
#include <unordered_map>

namespace wal {

// Forward declarations
class Symbol;
class WList;
class Closure;
class Macro;
class Environment;
class UserOperator;

/**
 * @defgroup AST Abstract Syntax Tree (AST) Types
 * @brief Core value types and AST node classes for WAL
 * 
 * The AST module provides the fundamental data structures used throughout WAL++:
 * - **Value**: The universal type representing all possible WAL values
 * - **Symbol**: Represents variable/signal identifiers
 * - **WList**: Lisp-like homogeneous lists
 * - **Closure**: First-class function objects (lambda)
 * - **Macro**: Macro definitions for metaprogramming
 * - **Environment**: Variable binding scope chains
 * 
 * @see @ref evaluator "Evaluator" for how these values are processed
 * @see @ref operator_registry "Operator Registry" for extensibility
 * @{
 */

/**
 * Architecture of WAL Value System
 * 
 * @mermaid
 * graph TD
 *     A["Value<br/>(std::variant)"] --> B["Scalars"]
 *     A --> C["Collections"]
 *     A --> D["Functions"]
 *     A --> E["Environment"]
 *     B --> B1["nil"]
 *     B --> B2["bool"]
 *     B --> B3["int64_t"]
 *     B --> B4["double"]
 *     B --> B5["string"]
 *     C --> C1["Symbol"]
 *     C --> C2["WList"]
 *     D --> D1["Closure<br/>lambda"]
 *     D --> D2["Macro"]
 *     D --> D3["UserOperator"]
 *     E --> E1["Environment<br/>scope chain"]
 * @endmermaid
 */

/**
 * Universal value type for WAL expressions
 * 
 * Represents all possible values in WAL expressions using std::variant for type safety.
 * This replaces Python's dynamic typing with C++'s type-safe alternative.
 * 
 * **Type Categories:**
 * - **Atomic**: nil, bool, int64_t, double, string
 * - **Symbolic**: Symbol (with scope/group tracking)
 * - **Collections**: WList (heterogeneous lisp lists)
 * - **Functions**: Closure (lambda), Macro, UserOperator
 * - **Context**: Environment (scope chains)
 * 
 * @see Symbol
 * @see WList
 * @see Closure
 * @see Environment
 * @ingroup AST
 */
class Value {
public:
    using ValueVariant = std::variant<
        std::nullptr_t,                           // nil/null
        bool,                                     // Boolean
        int64_t,                                  // Integer
        double,                                   // Float
        std::string,                              // String
        std::shared_ptr<Symbol>,                  // Symbol
        std::shared_ptr<WList>,                   // List
        std::shared_ptr<Closure>,                 // Function
        std::shared_ptr<Macro>,                   // Macro
        std::shared_ptr<Environment>,             // Environment
        std::shared_ptr<UserOperator>             // User-defined operator
    >;

private:
    ValueVariant data;

public:
    // Constructors
    Value();
    Value(std::nullptr_t);
    Value(bool b);
    Value(int64_t i);
    Value(int i) : Value(static_cast<int64_t>(i)) {}
    Value(double d);
    Value(const std::string& s);
    Value(std::string_view s);
    Value(const char* s);
    Value(std::shared_ptr<Symbol> sym);
    Value(std::shared_ptr<WList> list);
    Value(std::shared_ptr<Closure> closure);
    Value(std::shared_ptr<Macro> macro);
    Value(std::shared_ptr<Environment> env);
    Value(std::shared_ptr<UserOperator> op);

    ///@name Type Checking
    ///@{
    bool is_nil() const;
    bool is_bool() const;
    bool is_int() const;
    bool is_float() const;
    bool is_number() const;
    bool is_string() const;
    bool is_symbol() const;
    bool is_list() const;
    bool is_closure() const;
    bool is_macro() const;
    bool is_environment() const;
    bool is_user_operator() const;
    ///@}

    ///@name Type Extraction
    ///@{
    /// @anchor Value::to_bool
    bool to_bool() const;
    int64_t to_int() const;
    double to_float() const;
    const std::string& to_string() const;
    std::shared_ptr<Symbol> to_symbol() const;
    std::shared_ptr<WList> to_list() const;
    std::shared_ptr<Closure> to_closure() const;
    std::shared_ptr<Macro> to_macro() const;
    std::shared_ptr<Environment> to_environment() const;
    ///@}

    ///@name Safe Type Extraction
    ///@{
    /// Safely extract type if value is of type T, returns nullopt otherwise
    template<typename T>
    std::optional<T> try_as() const {
        if (auto* ptr = std::get_if<T>(&data)) {
            return *ptr;
        }
        return std::nullopt;
    }
    ///@}

    ///@name Conversions & Comparisons
    ///@{
    std::string to_string_repr() const;
    bool is_truthy() const;

    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const;
    ///@}

    ///@name Internals
    ///@{
    const ValueVariant& get_variant() const { return data; }
    ValueVariant& get_variant_mut() { return data; }
    ///@}
};

/**
 * Represents a variable or signal identifier with optional scope and group information
 * 
 * Symbols are the basic building blocks for identifiers in WAL. They support:
 * - **Simple names**: `x`, `my-signal`
 * - **Scoped names**: `~scope.signal` (hardware hierarchies)
 * - **Grouped names**: `#group-signal` (signal classification)
 * 
 * Used in signal lookups through the TraceContainer. The scope mechanism
 * allows WAL to navigate hierarchical signal hierarchies in hardware traces.
 * 
 * @see Trace for signal resolution
 * @see TraceContainer::signal_value
 * @ingroup AST
 */
class Symbol {
private:
    std::string name;
    std::string scope;      // For scoped symbols (~signal)
    std::string group;      // For grouped symbols (#signal)

public:
    Symbol(const std::string& name);
    Symbol(std::string_view name);
    Symbol(const std::string& name, const std::string& scope);

    const std::string& get_name() const { return name; }
    const std::string& get_scope() const { return scope; }
    const std::string& get_group() const { return group; }

    std::string full_name() const;
    bool operator==(const Symbol& other) const;
};

/**
 * Heterogeneous list container inheriting from std::vector<Value>
 * 
 * WList provides Lisp-like list semantics with:
 * - Arbitrary element types (elements are Values)
 * - First-class list objects
 * - Sequence operations (car, cdr, cons, length)
 * 
 * Lists form the structure of WAL S-expressions:
 * - Function calls: `(+ 1 2)` → WList{Symbol("+"), 1, 2}
 * - Data structures: `(list 1 2 3)` → WList{1, 2, 3}
 * - Nested forms: `(func (nested 1 2) 3)` → WList with nested WList
 * 
 * @see Value::is_list
 * @see SEval::eval for list evaluation
 * @ingroup AST
 */
class WList : public std::vector<Value> {
public:
    WList() = default;
    WList(const std::vector<Value>& values) : std::vector<Value>(values) {}
    WList(std::vector<Value>&& values) : std::vector<Value>(std::move(values)) {}

    std::string to_string() const;
};

/**
 * Lambda (anonymous) function object with captured environment
 * 
 * Closures implement first-class functions in WAL:
 * - Capture a lexical environment at definition time
 * - Store parameter list (args)
 * - Store function body (expression)
 * - Support recursive self-reference via name
 * 
 * Evaluation:
 * - Parameters bound to arguments in captured environment
 * - Body evaluated with new bindings
 * - Returns the body's evaluation result
 * 
 * ```
 * (lambda (x y) (+ x y))
 * ↓
 * Closure {
 *   environment: <current>,
 *   args: (x y),
 *   expression: (+ x y),
 *   name: "lambda"
 * }
 * ```
 * 
 * @see Environment for scope chains
 * @see SEval::eval_closure
 * @ingroup AST
 */
class Closure {
private:
    std::shared_ptr<Environment> environment;
    Value args;                          // Symbol or WList
    Value expression;                    // The body
    std::string name;

public:
    Closure(std::shared_ptr<Environment> env, const Value& args, 
            const Value& expr, const std::string& name = "lambda");

    std::shared_ptr<Environment> get_environment() const { return environment; }
    const Value& get_args() const { return args; }
    const Value& get_expression() const { return expression; }
    const std::string& get_name() const { return name; }

    std::string to_string() const;
};

/**
 * Lisp-style macro for code generation and metaprogramming
 * 
 * Macros allow compile-time code transformation:
 * - Similar structure to Closure (captured environment, args, body)
 * - Arguments are NOT evaluated before passing to macro
 * - Macro body must return code (a Value)
 * - Returned code is then evaluated by the evaluator
 * 
 * This enables DSL creation, special control flow, and code generation.
 * 
 * Unlike Closure:
 * - Arguments passed unevaluated (like special forms)
 * - Return value must be quoted/unquoted correctly
 * - Used for syntax transformation
 * 
 * @see Closure for comparison
 * @see Value for result type
 * @ingroup AST
 */
class Macro {
private:
    std::shared_ptr<Environment> environment;
    Value args;
    Value expression;
    std::string name;

public:
    Macro(std::shared_ptr<Environment> env, const Value& args, 
          const Value& expr, const std::string& name = "macro");

    std::shared_ptr<Environment> get_environment() const { return environment; }
    const Value& get_args() const { return args; }
    const Value& get_expression() const { return expression; }
    const std::string& get_name() const { return name; }

    std::string to_string() const;
};

/**
 * Reference to a user-defined operator
 * 
 * Allows WAL programs to reference operators defined in Python or other modules.
 * Stores the operator name and originating module for later dispatch.
 * 
 * Use cases:
 * - Python-implemented operators (via bindings)
 * - Module-qualified operators (`module:operator`)
 * - Dynamic operator resolution
 * 
 * @see OperatorRegistry for registration
 * @see SEval::dispatch_user
 * @ingroup AST
 */
class UserOperator {
private:
    std::string name;
    std::string module;

public:
    UserOperator(const std::string& name, const std::string& module = "");

    const std::string& get_name() const { return name; }
    const std::string& get_module() const { return module; }

    std::string to_string() const;
};

/**
 * Variable binding context with lexical scope chain
 * 
 * Environments form chains for lexical scoping:
 * 
 * @mermaid
 * graph LR
 *     A["Global Env<br/>parent: nullptr"] --> B["Outer Scope<br/>parent: Global"]
 *     B --> C["Inner Scope<br/>parent: Outer"]
 *     C --> D["Innermost<br/>parent: Inner"]
 * @endmermaid
 * 
 * **Variable Lookup** (lexical scoping):
 * - Check local bindings first
 * - Follow parent chain to global scope
 * - Throws EvalError if not found
 * 
 * **Variable Definition**:
 * - Always local to current environment
 * - Does not shadow parent bindings
 * - Use `write` to modify parent-defined variables
 * 
 * Enables:
 * - Closures with captured environments
 * - Function parameter bindings
 * - Local variable scopes
 * - Proper recursion support
 * 
 * @see Closure for environment capture
 * @see SEval for scope management
 * @ingroup AST
 */
class Environment : public std::enable_shared_from_this<Environment> {
private:
    std::unordered_map<std::string, Value> bindings;
    std::shared_ptr<Environment> parent;

public:
    Environment();
    explicit Environment(std::shared_ptr<Environment> parent);

    // Variable access
    /// Lookup variable in scope chain, throws EvalError if undefined
    Value get(const std::string& name) const;
    /// Define variable in local scope (does not shadow parents)
    void define(const std::string& name, const Value& value);
    /// Modify existing variable in scope chain
    void write(const std::string& name, const Value& value);
    /// Remove variable from local scope
    void undefine(const std::string& name);
    /// Check if variable is defined anywhere in chain
    bool is_defined(const std::string& name) const;

    // Environment access
    /// @see @ref Environment for scope chain architecture
    std::shared_ptr<Environment> get_parent() const { return parent; }
    /// Access to local bindings for introspection
    const std::unordered_map<std::string, Value>& get_bindings() const { 
        return bindings; 
    }

    std::string to_string() const;
};

/**
 * @defgroup Errors Exception Hierarchy
 * @brief WAL exception types for error handling
 * 
 * Exception hierarchy:
 * ```
 * std::runtime_error
 *     ↓
 * WalError (base for all WAL errors)
 *     ├─ ParseError
 *     ├─ EvalError (with stack traces)
 *     └─ TraceError
 * ```
 * 
 * @{
 */

/**
 * Base exception for all WAL errors
 * 
 * Derived exception classes should inherit from this for consistent
 * error handling and reporting.
 */
class WalError : public std::runtime_error {
public:
    explicit WalError(const std::string& message) 
        : std::runtime_error(message) {}
};

/**
 * Parsing failed - S-expression syntax invalid
 * 
 * Thrown by Parser when encountering:
 * - Unmatched parentheses
 * - Invalid number format
 * - Unexpected end of input
 * - Invalid string escapes
 * 
 * @see Parser for parsing details
 */
class ParseError : public WalError {
public:
    explicit ParseError(const std::string& message) 
        : WalError("Parse error: " + message) {}
};

/**
 * Evaluation failed - runtime error during expression evaluation
 * 
 * Thrown by SEval when encountering:
 * - Undefined variables (@ anchor: ast/eval_error/undefined)
 * - Type errors (wrong types for operation)
 * - Division by zero
 * - Invalid function calls
 * - Trace access failures
 * 
 * Supports stack trace annotation via add_trace() for debugging.
 * 
 * @see SEval::eval for evaluation context
 */
class EvalError : public WalError {
public:
    explicit EvalError(const std::string& message) 
        : WalError("Eval error: " + message) {}
    
private:
    std::vector<std::string> stack_trace;
    
public:
    /// Add a frame to the stack trace for debugging
    void add_trace(const std::string& frame) { stack_trace.push_back(frame); }
};

/**
 * Trace file loading or signal access failed
 * 
 * Thrown by TraceContainer or Trace subclasses when:
 * - File doesn't exist or is corrupted
 * - Invalid VCD/FST/CSV format
 * - Signal lookup fails
 * - Time index out of range
 * 
 * @see TraceContainer for trace loading
 * @see Trace for signal access
 */
class TraceError : public WalError {
public:
    explicit TraceError(const std::string& message) 
        : WalError("Trace error: " + message) {}
};

/**
 * @}
 * @endgroup Errors
 */

/**
 * @}
 * @endgroup AST
 */

} // namespace wal