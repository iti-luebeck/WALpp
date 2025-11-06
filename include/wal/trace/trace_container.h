#pragma once

#include "../ast.h"
#include "trace.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace wal {

class TraceContainer;

/**
 * Central hub for managing multiple loaded waveform traces
 * 
 * The TraceContainer:
 * - **Manages multiple traces** - Keeps all loaded traces
 * - **Routes signal queries** - Directs lookups to correct trace
 * - **Coordinates time** - Steps all traces in sync
 * - **Handles format loading** - Auto-detects VCD/FST/CSV
 * 
 * **Usage Flow:**
 * ```cpp
 * auto container = std::make_shared<TraceContainer>();
 * container->load("trace1.vcd", "t1");    // Load VCD
 * container->load("trace2.fst", "t2");    // Load FST
 * 
 * // Access signals from any trace
 * Value clk = container->signal_value("clk");      // From t1
 * Value data = container->signal_value("t2~data"); // From t2
 * 
 * // Step all traces in sync
 * container->step(10);  // All traces forward 10 cycles
 * ```
 * 
 * **Signal Name Resolution:**
 * - Format: `[trace-id~]signal-name`
 * - If trace-id omitted, searches all traces
 * - Returns first match found
 * - Use explicit prefix for disambiguation
 * 
 * @see Trace for signal access details
 * @see @ref Core::Wal::load for loading interface
 */
class TraceContainer {
private:
    std::unordered_map<std::string, std::shared_ptr<Trace>> traces;
    size_t num_traces = 0;
    std::vector<int> index_stack;
    
    friend class Trace;

public:
    TraceContainer() = default;
    ~TraceContainer() = default;

    ///@name Trace Loading
    ///@{
    /**
     * Load a waveform file into container
     * 
     * **Auto-detects format** from file extension:
     * - `.vcd` → VCD format (Verilog Change Dump)
     * - `.fst` → FST format (Fast Signal Trace, requires pylibfst)
     * - `.csv` → CSV format (Comma-Separated Values)
     * 
     * **Trace ID:** If not specified, auto-generated from timestamp.
     * Use explicit IDs for consistent access:
     * ```cpp
     * container->load("trace.vcd", "my-trace");
     * // Access as: (signal "signal-name")  // from "my-trace"
     * ```
     * 
     * **Signal filtering:** Optional keep_signals list to load only specific signals.
     * Useful for large traces with many irrelevant signals.
     * ```cpp
     * std::vector<std::string> keep{"clk", "reset", "data_*"};
     * container->load("trace.vcd", "t1", &keep);
     * ```
     * 
     * @param file Path to waveform file
     * @param tid Trace ID (auto-generated if empty)
     * @param keep_signals Optional filter (default: load all)
     * @throws TraceError if file not found, parse fails, or format invalid
     * @see @ref parser "Parser" for trace format support
     */
    void load(const std::string& file, const std::string& tid = "", 
              const std::vector<std::string>* keep_signals = nullptr);

    /**
     * Unload a trace and free memory
     * 
     * After unload, signals from that trace are no longer accessible.
     * Safe to call if trace doesn't exist (no-op).
     * 
     * @param tid Trace ID to unload
     */
    void unload(const std::string& tid);
    ///@}

    ///@name Signal Access
    ///@{
    /**
     * Get a signal value from any loaded trace
     * 
     * **Signal name formats:**
     * - `signal-name` - searches all traces, first match
     * - `trace-id~signal-name` - specific trace
     * - `~scope.signal-name` - scoped lookup
     * 
     * **Offset semantics:**
     * - offset=0: current time (default)
     * - offset>0: future value (lookahead)
     * - offset<0: past value (lookbehind)
     * 
     * **Example:**
     * ```cpp
     * // Get current clock value
     * Value clk = container->signal_value("clk");
     * 
     * // Get data 10 cycles ahead
     * Value future_data = container->signal_value("data", 10);
     * 
     * // Get from specific trace
     * Value data2 = container->signal_value("trace2~data");
     * 
     * // Scoped signal
     * Value sig = container->signal_value("~cpu.alu.result");
     * ```
     * 
     * @param name Signal name (possibly with trace ID and scope)
     * @param offset Time offset (default: 0 = current)
     * @param scope Optional scope prefix
     * @return Signal value at specified time
     * @throws TraceError if signal not found or time out of bounds
     */
    Value signal_value(const std::string& name, int offset = 0, 
                      const std::string& scope = "");

    /**
     * Get signal width in bits
     * 
     * @param name Signal name
     * @return Width in bits (1+ for vectors)
     * @throws TraceError if signal not found
     */
    int signal_width(const std::string& name);

    /**
     * Check if a signal exists in any loaded trace
     * 
     * @param name Signal name
     * @return true if signal is available
     */
    bool contains(const std::string& name);
    ///@}

    ///@name Time Navigation
    ///@{
    /**
     * Step all traces through time
     * 
     * Advances or rewinds all loaded traces synchronously.
     * Useful for sequential time-series analysis.
     * 
     * **Stepping by trace:**
     * ```cpp
     * container->step(5);          // All traces forward 5 cycles
     * container->step(-3);         // All traces backward 3 cycles
     * container->step(1, "trace1"); // Only trace1 forward 1 cycle
     * ```
     * 
     * @param steps Number of steps (positive=forward, negative=backward)
     * @param tid Optional specific trace to step (empty=all)
     * @return Empty string on success, or trace ID if stepping failed
     */
    std::string step(int steps = 1, const std::string& tid = "");

    /**
     * Get current time index of all traces
     * 
     * @return Map from trace ID to current time index
     */
    std::unordered_map<std::string, int> indices() const;
    ///@}

    ///@name Introspection
    ///@{
    /**
     * Get list of loaded trace IDs
     * 
     * @return Vector of trace IDs
     */
    std::vector<std::string> get_trace_ids() const;

    /**
     * Get a specific trace object
     * 
     * For advanced operations requiring direct trace access.
     * 
     * @param tid Trace ID
     * @return Shared pointer to trace, or nullptr if not found
     */
    std::shared_ptr<Trace> get_trace(const std::string& tid) const;

    /**
     * Get number of loaded traces
     * 
     * @return Count of traces
     */
    size_t size() const { return num_traces; }

    /**
     * Check if any traces are loaded
     * 
     * @return true if no traces loaded
     */
    bool empty() const { return num_traces == 0; }
    ///@}

    ///@name Management
    ///@{
    /**
     * Clear all loaded traces
     * 
     * Frees all loaded traces. Useful for clean reset.
     */
    void clear();

    /**
     * Get all loaded traces (internal use)
     * 
     * @return Const reference to trace map
     */
    const std::unordered_map<std::string, std::shared_ptr<Trace>>& get_traces() const { 
        return traces; 
    }
    ///@}

private:
    /// Load VCD (Verilog Change Dump) format
    void load_vcd(const std::string& file, const std::string& tid,
                  const std::vector<std::string>* keep_signals);
    
    /// Load FST (Fast Signal Trace) format
    void load_fst(const std::string& file, const std::string& tid,
                  const std::vector<std::string>* keep_signals);
    
    /// Load CSV (Comma-Separated Values) format
    void load_csv(const std::string& file, const std::string& tid,
                  const std::vector<std::string>* keep_signals);
};

/**
 * @}
 * @endgroup trace
 */

} // namespace wal
