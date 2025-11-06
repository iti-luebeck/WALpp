#pragma once

#include "ast.h"
#include "operator_registry.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <optional>

namespace wal {

// Forward declaration
class TraceContainer;

/**
 * @defgroup evaluator Evaluator & Execution Engine
 * @brief S-expression evaluation and runtime state management
 * 
 * The SEval class implements the core evaluation engine:
 * 
 * @mermaid
 * graph LR
 *     A["S-Expression"] --> B["Parse"]
 *     B --> C["SEval::eval"]
 *     C --> D{"Is it a<br/>list?"}
 *     D -->|No| E["Return Atom"]
 *     D -->|Yes| F["Get Operator"]
 *     F --> G{"Builtin or<br/>User?"}
 *     G -->|Builtin| H["dispatch_builtin"]
 *     G -->|User| I["dispatch_user"]
 *     H --> J["Result"]
 *     I --> J
 * @endmermaid
 * 
 * **Key Responsibilities:**
 * - Evaluation dispatch to operators
 * - Environment and scope chain management
 * - Closure and macro handling
 * - Macro expansion
 * - Virtual signal management
 * 
 * **Evaluation Model:**
 * WAL uses an applicative-order (eager) evaluator:
 * 1. Evaluate all arguments first (except special forms)
 * 2. Dispatch to operator with evaluated arguments
 * 3. Operator returns result
 * 4. Continue with next expression
 * 
 * **Special Forms:**
 * Some operators (like `if`, `define`, `lambda`) receive unevaluated arguments
 * and handle evaluation themselves for proper semantics.
 * 
 * @see @ref operator_registry "Operator Registry" for extending evaluator
 * @see @ref AST for value types
 * 
 * @{
 */

/**
 * S-expression evaluator - the core evaluation engine
 * 
 * The SEval class is responsible for:
 * - Tree-walking evaluation of S-expressions
 * - Environment and scope management  
 * - Operator dispatch (builtin and user-defined)
 * - Closure and macro evaluation
 * - State tracking for traces and virtual signals
 */
class SEval {
private:
    std::shared_ptr<TraceContainer> traces;
    std::shared_ptr<Environment> global_environment;
    std::shared_ptr<Environment> current_environment;

    OperatorRegistry builtin_operators;
    std::unordered_map<std::string, OperatorFunc> user_operators;

    // State
    int gensym_counter = 0;
    std::unordered_map<std::string, std::string> imports;
    std::unordered_map<std::string, std::string> aliases;
    std::unordered_map<std::string, Value> macros;

    // Scope and group tracking (for hardware signal management)
    std::string current_scope;
    std::string current_group;

    // Virtual signals storage
    std::unordered_map<std::string, Value> virtual_signals;

public:
    /**
     * Create a new evaluator for a trace container
     * 
     * Initializes:
     * - Global environment with builtins
     * - Operator registry with all standard operators
     * - Trace container reference
     * 
     * @param traces Shared TraceContainer for signal access
     * @see @anchor Wal::load for loading traces
     */
    explicit SEval(std::shared_ptr<TraceContainer> traces);

    /**
     * Reset evaluator state
     * 
     * Clears user definitions and returns traces to time 0.
     * Preserves loaded traces and global operators.
     * 
     * Useful between independent computations.
     */
    void reset();

    ///@name Evaluation
    ///@{
    /**
     * Evaluate an S-expression
     * 
     * The core evaluation function. Handles:
     * - Atoms: returns value as-is
     * - Symbols: looks up in environment
     * - Lists: dispatches to operator
     * 
     * **Evaluation Order:**
     * 1. If not a list, return atomic value
     * 2. If symbol starts with '~', resolve signal
     * 3. First element must be operator name (symbol)
     * 4. Remaining elements are arguments
     * 5. Dispatch to operator with unevaluated args
     * 
     * @param expr The expression to evaluate (atom or list)
     * @return The evaluated result
     * @throws EvalError on undefined variables or operators
     * @throws ParseError on syntax errors (if reparsing)
     */
    Value eval(const Value& expr);

    /**
     * Evaluate multiple arguments
     * 
     * Convenience function to evaluate a vector of expressions.
     * Each expression evaluated in current environment.
     * 
     * @param args Expressions to evaluate
     * @return Vector of evaluated results (same order)
     * @throws EvalError if any argument evaluation fails
     */
    std::vector<Value> eval_args(const std::vector<Value>& args);

    /**
     * Evaluate a closure (lambda application)
     * 
     * Creates new environment with parameters bound to arguments,
     * then evaluates closure body in that environment.
     * 
     * **Process:**
     * 1. Create new environment with closure's captured env as parent
     * 2. Bind parameters to arguments
     * 3. Evaluate body in new environment
     * 4. Restore previous environment
     * 
     * Supports recursion via named closures.
     * 
     * @param closure The closure to call
     * @param args The evaluated arguments to pass
     * @return Result of closure body
     * @throws EvalError if parameter count mismatch
     * @see @anchor Value::to_closure
     */
    Value eval_closure(std::shared_ptr<Closure> closure, const std::vector<Value>& args);

    /**
     * Dispatch to a built-in operator
     * 
     * Looks up operator in registry and calls it with unevaluated arguments.
     * Operator decides when/if to evaluate args.
     * 
     * Used for special forms (if, define, lambda, etc.) that need
     * control over argument evaluation.
     * 
     * @param name Operator name (e.g., "+", "if", "define")
     * @param args Unevaluated arguments (raw expressions)
     * @return Result from operator
     * @throws EvalError if operator not found
     */
    Value dispatch_builtin(const std::string& name, const std::vector<Value>& args);

    /**
     * Dispatch to a user-defined operator
     * 
     * Calls operator with already-evaluated arguments.
     * Used for closures, Python-imported functions, etc.
     * 
     * @param name Operator name
     * @param args Evaluated arguments
     * @return Result from operator
     * @throws EvalError if operator not found
     */
    Value dispatch_user(const std::string& name, const std::vector<Value>& args);
    ///@}

    ///@name Environment Access
    ///@{
    /// Get current evaluation environment (may be nested in closures)
    std::shared_ptr<Environment> get_environment() const { 
        return current_environment; 
    }
    
    /// Switch to a different environment (advanced usage)
    void set_environment(std::shared_ptr<Environment> env) { 
        current_environment = env; 
    }
    
    /// Get global (top-level) environment
    std::shared_ptr<Environment> get_global_environment() const { 
        return global_environment; 
    }
    ///@}

    ///@name Operator Registry
    ///@{
    /// Access registry for operator queries and extensions
    /// @see @ref operator_registry "Operator Registry"
    OperatorRegistry& get_operator_registry() { 
        return builtin_operators; 
    }
    
    /// Const access for introspection
    const OperatorRegistry& get_operator_registry() const { 
        return builtin_operators; 
    }
    ///@}

    ///@name Trace Access
    ///@{
    /// Get reference to loaded traces for signal queries
    /// @see @ref trace "Trace System"
    std::shared_ptr<TraceContainer> get_traces() const { return traces; }
    ///@}

    ///@name Scope & Group Management
    ///@{
    /// Get current scope for signal hierarchies (e.g., "module.submodule")
    const std::string& get_scope() const { return current_scope; }
    
    /// Set current scope context
    void set_scope(const std::string& scope) { current_scope = scope; }

    /// Get current group for signal classification
    const std::string& get_group() const { return current_group; }
    
    /// Set current group context
    void set_group(const std::string& group) { current_group = group; }
    ///@}

    ///@name Virtual Signals
    ///@{
    /// Access virtual signals (user-defined derived signals)
    /// @see @ref waveform-operators for virtual signal creation
    std::unordered_map<std::string, Value>& get_virtual_signals() { 
        return virtual_signals; 
    }
    ///@}

    ///@name Aliases & Macros
    ///@{
    /// Get all registered aliases (operator name mappings)
    const std::unordered_map<std::string, std::string>& get_aliases() const { 
        return aliases; 
    }
    
    /// Register an operator alias (e.g., rest -> cdr)
    void add_alias(const std::string& from, const std::string& to) { 
        aliases[from] = to; 
    }
    
    /// Remove an alias
    void remove_alias(const std::string& from) { 
        aliases.erase(from); 
    }
    
    /// Check if an alias exists
    bool has_alias(const std::string& name) const { 
        return aliases.count(name) > 0; 
    }

    /// Get all registered macros
    const std::unordered_map<std::string, Value>& get_macros() const { 
        return macros; 
    }
    
    /// Register a new macro
    void add_macro(const std::string& name, const Value& macro) { 
        macros[name] = macro; 
    }
    
    /// Check if a macro exists
    bool has_macro(const std::string& name) const { 
        return macros.count(name) > 0; 
    }
    ///@}

    ///@name Symbol Generation
    ///@{
    /// Generate unique symbol name
    /// Useful for macro hygiene and gensym patterns
    /// @param prefix Default prefix for generated symbols
    /// @return Unique symbol (e.g., "gensym-123")
    std::string gensym(const std::string& prefix = "gensym");
    ///@}

private:
    /// Initialize all builtin operators
    void initialize_builtins();
};

/**
 * @}
 * @endgroup evaluator
 */

} // namespace wal
