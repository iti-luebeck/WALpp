#pragma once

#include <functional>
#include <unordered_map>
#include <string>
#include <memory>
#include <optional>
#include <vector>

#include "ast.h"

namespace wal {

// Forward declaration
class SEval;

/**
 * @defgroup operator_registry Operator Registry
 * @brief Extensible operator registration and dispatch system
 * 
 * The Operator Registry is the key to WAL++'s extensibility.
 * It allows both C++ and Python code to register new operators dynamically.
 * 
 * **Operator Registration Model:**
 * 
 * @mermaid
 * graph TD
 *     A["OperatorRegistry"] -->|register| B["Operator Map"]
 *     B --> C["Built-in<br/>50+ ops"]
 *     B --> D["Python<br/>Operators"]
 *     B --> E["User<br/>Custom"]
 *     C --> F["Math"]
 *     C --> G["List Ops"]
 *     C --> H["Waveform"]
 *     D --> I["PyO3 Bridge"]
 *     E --> J["Closures"]
 * @endmermaid
 * 
 * **Key Features:**
 * - Dynamic registration (add operators at runtime)
 * - Unified dispatch (same call for all operator types)
 * - Unevaluated arguments (allows special forms)
 * - Easy Python integration via PyO3
 * 
 * **Operator Categories:**
 * - Math: +, -, *, /, **, %
 * - Bitwise: band, bor, bxor, lshift, rshift
 * - List: car, cdr, cons, length, list
 * - Array: arange, array, extend
 * - Waveform: signal, signal-names, load, step
 * - Type: type, is?, ...
 * - Control: if, while, for, let
 * - Definition: define, lambda, macro
 * - Special: quote, unquote, eval
 * 
 * @see @ref evaluator "Evaluator" for usage
 * @see @ref Core "Core API" for public interface
 * 
 * @{
 */

/**
 * Function signature for all WAL operators
 * 
 * All operators in WAL share this signature:
 * 
 * ```cpp
 * Value my_operator(SEval& evaluator, const std::vector<Value>& args)
 * ```
 * 
 * **Parameters:**
 * - `evaluator`: Reference to evaluator (for recursive eval, environment access)
 * - `args`: Unevaluated argument expressions
 * 
 * **Return:** The computed result value
 * 
 * **Unevaluated Arguments:**
 * Operators receive arguments unevaluated. This is crucial for:
 * - Special forms (if, define, lambda) that need control flow
 * - Lazy evaluation patterns
 * - Quoting and metaprogramming
 * 
 * Regular operators typically evaluate arguments first:
 * ```cpp
 * Value builtin_plus(SEval& eval, const std::vector<Value>& args) {
 *     auto evaluated = eval.eval_args(args);  // Evaluate first
 *     // ... now compute sum
 * }
 * ```
 * 
 * Special forms skip evaluation:
 * ```cpp
 * value builtin_if(SEval& eval, const std::vector<Value>& args) {
 *     // args[0] is condition unevaluated
 *     auto cond = eval.eval(args[0]);
 *     if (cond.is_truthy()) {
 *         return eval.eval(args[1]);  // Evaluate then-branch
 *     } else {
 *         return eval.eval(args[2]);  // Evaluate else-branch
 *     }
 * }
 * ```
 * 
 * @see SEval::dispatch_builtin for operator lookup
 * @see SEval::eval_args for argument evaluation helper
 */
using OperatorFunc = std::function<Value(
    SEval& evaluator, 
    const std::vector<Value>& args
)>;

/**
 * Central registry for operator definitions
 * 
 * Manages:
 * - Registration of new operators
 * - Lookup by name
 * - Batch registration of operator categories
 * - Dynamic operator injection (from Python)
 * 
 * Used by SEval to dispatch function calls and special forms.
 * Typically one registry per evaluator instance.
 * 
 * @see SEval for usage context
 */
class OperatorRegistry {
private:
    std::unordered_map<std::string, OperatorFunc> operators;

public:
    OperatorRegistry() = default;

    ///@name Registration
    ///@{
    /**
     * Register a new operator
     * 
     * Once registered, the operator is available for dispatch in eval().
     * If operator already exists, replaces it.
     * 
     * **Example - Simple math operator:**
     * ```cpp
     * registry.register_operator("+", [](SEval& eval, const std::vector<Value>& args) {
     *     auto evaluated = eval.eval_args(args);
     *     int64_t sum = 0;
     *     for (auto& val : evaluated) {
     *         sum += val.to_int();
     *     }
     *     return Value(sum);
     * });
     * ```
     * 
     * **Example - Special form:**
     * ```cpp
     * registry.register_operator("if", [](SEval& eval, const std::vector<Value>& args) {
     *     if (eval.eval(args[0]).is_truthy()) {
     *         return eval.eval(args[1]);
     *     } else {
     *         return eval.eval(args[2]);
     *     }
     * });
     * ```
     * 
     * @param name The operator name (e.g., "+", "my-op", "special-form")
     * @param func The implementation function (callable with OperatorFunc signature)
     * @see @ref operator_registry for architecture overview
     */
    void register_operator(const std::string& name, OperatorFunc func);

    ///@name Lookup
    ///@{
    /**
     * Get an operator if it exists
     * 
     * Used by evaluator to dispatch function calls:
     * ```cpp
     * auto op = registry.get_operator("+");
     * if (op) {
     *     result = op.value()(evaluator, arguments);
     * }
     * ```
     * 
     * @param name The operator name to look up
     * @return Optional containing the operator function, or nullopt if not found
     */
    std::optional<OperatorFunc> get_operator(const std::string& name) const;

    /**
     * Check if an operator exists
     * 
     * Faster than get_operator() if you only need existence check.
     * 
     * @param name The operator name
     * @return true if operator is registered, false otherwise
     */
    bool has_operator(const std::string& name) const;

    ///@}

    ///@name Introspection
    ///@{
    /**
     * Get all registered operator names
     * 
     * Useful for listing available operators or debugging.
     * 
     * @return Vector of operator names
     */
    std::vector<std::string> get_all_operators() const;

    /**
     * Get number of registered operators
     * 
     * @return Count of registered operators
     */
    size_t size() const { return operators.size(); }

    ///@}

    ///@name Maintenance
    ///@{
    /**
     * Clear all registered operators
     * 
     * Use with caution - removes all operators including builtins.
     * Typically only used in testing or extreme reset scenarios.
     */
    void clear() { operators.clear(); }
    ///@}

    ///@name Batch Registration Helpers
    ///@{
    /// Register standard math operators (+, -, *, /, **, %)
    void register_math_operators();
    
    /// Register bitwise operators (band, bor, bxor, lshift, rshift)
    void register_bitwise_operators();
    
    /// Register list operators (car, cdr, cons, length, list)
    void register_list_operators();
    
    /// Register array operators (arange, array, extend)
    void register_array_operators();
    
    /// Register core operators (define, lambda, if, etc.)
    void register_core_operators();
    
    /// Register type operators (type, is?, etc.)
    void register_type_operators();
    
    /// Register waveform operators (signal, step, load, etc.)
    void register_waveform_operators();
    
    /// Register special forms (quote, eval, etc.)
    void register_special_operators();
    
    /// Register virtual signal operators
    void register_virtual_operators();
    ///@}
};

/**
 * @}
 * @endgroup operator_registry
 */

} // namespace wal
