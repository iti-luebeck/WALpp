#pragma once

#include "ast.h"
#include "evaluator.h"
#include "trace/trace_container.h"
#include <memory>
#include <string>
#include <vector>

namespace wal {

/**
 * @defgroup Core Core API
 * @brief Main user-facing WAL interface
 * 
 * The Wal class is the primary entry point for users of WAL++.
 * It integrates all subsystems:
 * 
 * @mermaid
 * graph TB
 *     A["Wal<br/>Main API"] --> B["SEval<br/>Evaluator"]
 *     A --> C["TraceContainer<br/>Waveforms"]
 *     A --> D["OperatorRegistry<br/>Operators"]
 *     B --> E["Environment<br/>Variables"]
 *     B --> D
 *     C --> F["Trace Formats<br/>VCD/FST/CSV"]
 * @endmermaid
 * 
 * **Usage Flow:**
 * 1. Create Wal() instance
 * 2. load() trace files
 * 3. eval_str() or run_file() for computation
 * 4. Access results via returned Value
 * 
 * **See Also:**
 * - @ref evaluator "Evaluator" - expression evaluation
 * - @ref trace "Trace System" - waveform access
 * - @ref operator_registry "Operator Registry" - extensibility
 * 
 * @{
 */

/**
 * Main WAL interface class
 * 
 * This is the primary class users interact with. It provides:
 * - **Trace management**: load/unload waveform files (VCD, FST, CSV)
 * - **Program execution**: eval_str(), run_file(), REPL
 * - **State tracking**: step through time, manage loaded traces
 * - **Environment access**: query and modify variables
 */
class Wal {
private:
    std::shared_ptr<TraceContainer> traces;
    std::shared_ptr<SEval> eval_context;

public:
    /**
     * Create a new WAL instance with initialized context
     * 
     * Initializes:
     * - Empty trace container
     * - SEval evaluator with builtins
     * - Standard library operators
     * - Global environment
     */
    Wal();

    ///@name Trace Management
    ///@{
    /**
     * Load a waveform file
     * 
     * **Supported formats:**
     * - `.vcd` → Verilog Change Dump
     * - `.fst` → Fast Signal Trace (requires pylibfst)
     * - `.csv` → Comma-Separated Values
     * 
     * Auto-detects format from file extension.
     * Signals become accessible as WAL variables.
     * 
     * @param file Path to the waveform file
     * @param tid Trace ID for this file (default: auto-generated)
     * @throws TraceError if file not found or parse fails
     * @see @ref trace for signal access patterns
     * @anchor Wal::load
     */
    void load(const std::string& file, const std::string& tid = "DEFAULT");

    /**
     * Unload a trace file and free its memory
     * 
     * After unload, signals from this trace are no longer accessible.
     * 
     * @param tid Trace ID to unload (default: "DEFAULT")
     * @throws TraceError if trace not found
     */
    void unload(const std::string& tid = "DEFAULT");

    /**
     * Advance traces through time
     * 
     * Steps all loaded traces forward or backward in time.
     * Useful for sequential trace analysis.
     * 
     * @param steps Number of time steps to move (default: 1)
     *              Positive = forward, Negative = backward
     * @param tid Optional specific trace to step (empty = all)
     * @throws TraceError if step would go out of bounds
     */
    void step(int steps = 1, const std::string& tid = "");
    ///@}

    ///@name Evaluation
    ///@{
    /**
     * Evaluate a WAL expression from a string
     * 
     * Parses an S-expression and evaluates it in the current context.
     * This does NOT reset state - useful for incremental evaluation.
     * 
     * **Example:**
     * ```cpp
     * auto wal = std::make_shared<Wal>();
     * wal->load("trace.vcd");
     * Value result = wal->eval_str("(+ 1 2)");  // 3
     * ```
     * 
     * @param code The WAL S-expression to evaluate
     * @return The result value
     * @throws ParseError if syntax is invalid
     * @throws EvalError if evaluation fails
     * @see @anchor Wal::load for trace access
     */
    Value eval_str(const std::string& code);

    /**
     * Evaluate a parsed WAL expression
     * 
     * @param expr The already-parsed expression
     * @return The result value
     * @throws EvalError if evaluation fails
     * @see eval_str for string parsing
     */
    Value eval(const Value& expr);

    /**
     * Run WAL code with a fresh evaluation context
     * 
     * Similar to eval() but resets state:
     * - Traces return to time 0
     * - User definitions cleared
     * - Global environment preserved
     * 
     * Useful for multiple independent computations.
     * 
     * @param code The WAL code to run
     * @return The result value
     */
    Value run_str(const std::string& code);

    /**
     * Run a parsed expression with fresh context
     * 
     * @param expr The expression to run
     * @return The result value
     * @see run_str for behavior details
     */
    Value run(const Value& expr);

    /**
     * Load and execute a WAL program from a file
     * 
     * Reads .wal or .wo file and evaluates all expressions sequentially.
     * Returns the result of the last expression.
     * 
     * **Example WAL program:**
     * ```scheme
     * (load "trace.vcd")
     * (define counter (signal "clk"))
     * (map (lambda (i) (signal "data" i)) (range 0 100))
     * ```
     * 
     * @param filename Path to .wal or .wo file
     * @return The result of the last expression
     * @throws TraceError if file not found
     * @throws ParseError if syntax is invalid
     */
    Value run_file(const std::string& filename);

    /**
     * Start the interactive REPL
     * 
     * Begins interactive shell loop:
     * - Reads expressions from stdin
     * - Pretty-prints results
     * - Maintains command history
     * - Quits on (quit) or EOF
     * 
     * @return Exit code (0 = success)
     * @see REPL for shell features
     */
    int start_repl();
    ///@}

    ///@name Accessors
    ///@{
    /// Access to loaded traces
    /// @see @ref trace "Trace System" for signal queries
    std::shared_ptr<TraceContainer> get_traces() const { return traces; }

    /// Access to evaluator for low-level control
    /// @see @ref evaluator for SEval documentation
    std::shared_ptr<SEval> get_eval_context() const { return eval_context; }

    /// Access to operator registry for extending WAL
    /// @see @ref operator_registry "Operator Registry"
    OperatorRegistry& get_operator_registry() { 
        return eval_context->get_operator_registry();
    }

    /// Access to global variable bindings
    /// @see Environment for scope chain semantics
    std::shared_ptr<Environment> get_environment() const { 
        return eval_context->get_environment();
    }
    ///@}

    ///@name Error Handling
    ///@{
    /**
     * Print error information for debugging
     * 
     * Helper function for pretty-printing parse or evaluation errors.
     * 
     * @param expr The expression that caused the error
     * @param error The error message or exception
     */
    void print_error(const Value& expr, const std::string& error);
    ///@}
};

/**
 * @}
 * @endgroup Core
 */

} // namespace wal
