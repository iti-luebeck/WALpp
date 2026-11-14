#pragma once

#include "../ast.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace wal {

class TraceContainer;

/**
 * @defgroup trace Trace System
 * @brief Waveform file loading and signal access
 * 
 * The Trace System provides unified access to hardware waveforms in multiple formats:
 * - **VCD** (Verilog Change Dump) - standard format
 * - **FST** (Fast Signal Trace) - compressed format
 * - **CSV** (Comma-Separated Values) - custom format
 * 
 * **Architecture:**
 * 
 * @mermaid
 * graph TD
 *     A["TraceContainer"] --> B["Trace<br/>Base Class"]
 *     B --> B1["VCDTrace"]
 *     B --> B2["FSTTrace"]
 *     B --> B3["CSVTrace"]
 *     A -->|routes to| B1
 *     A -->|routes to| B2
 *     A -->|routes to| B3
 *     B1 --> S1["VCD Parser"]
 *     B2 --> S2["FST Parser"]
 *     B3 --> S3["CSV Parser"]
 * @endmermaid
 * 
 * **Signal Access Pattern:**
 * 
 * Once a trace is loaded, signals are accessed as WAL variables:
 * ```scheme
 * (load "trace.vcd")           ; Load trace file
 * (signal "clk")              ; Get clock signal value
 * (signal "data" 10)          ; Get data signal 10 cycles ahead
 * (signal-names)              ; List all signals
 * (step)                      ; Advance time by 1
 * ```
 * 
 * **Special Signals:**
 * - `SIGNALS` - list of all signal names
 * - `INDEX` - current time index
 * - `MAX-INDEX` - maximum time index in trace
 * - `TIMESTAMP` (or `TS`) - current simulation time
 * - `TRACE-NAME` - name of loaded trace
 * - `SCOPES` - hierarchical scope names
 * 
 * @see @ref Core "Wal::load" for loading traces
 * @see @ref evaluator for signal query operators
 * 
 * @{
 */

/**
 * Abstract base class for waveform traces
 * 
 * Provides unified interface for different waveform file formats.
 * Subclasses (VCDTrace, FSTTrace, CSVTrace) implement format-specific parsing.
 * 
 * **Key Concepts:**
 * - **Time Index**: Index into the trace timeline (0 to max_index)
 * - **Time Offset**: Relative offset from current index (for lookahead/lookbehind)
 * - **Signal Value**: Value of a signal at current time+offset
 * - **Virtual Signals**: Derived signals computed by operators
 * 
 * **Scope Hierarchy:**
 * Traces support hierarchical signal naming (e.g., `top.module.signal`)
 * using the `SCOPE_SEPARATOR` ('.') character.
 */
class Trace {
public:
    // Special signal names that have built-in meaning
    /// All available signals in this trace
    static constexpr const char* SIGNALS = "SIGNALS";
    
    /// All signals excluding aliases
    static constexpr const char* SIGNALS_NO_ALIAS = "SIGNALS-NO-ALIAS";
    
    /// Virtual/derived signals created by operators
    static constexpr const char* VIRTUAL_SIGNALS = "VIRTUAL-SIGNALS";
    
    /// Signals defined in current scope only
    static constexpr const char* LOCAL_SIGNALS = "LOCAL-SIGNALS";
    
    /// Current time index (integer)
    static constexpr const char* INDEX = "INDEX";
    
    /// Maximum valid time index
    static constexpr const char* MAX_INDEX = "MAX-INDEX";
    
    /// Simulation timestamp at current index
    static constexpr const char* TIMESTAMP = "TS";
    
    /// Name of the trace file
    static constexpr const char* TRACE_NAME = "TRACE-NAME";
    
    /// Path to trace file
    static constexpr const char* TRACE_FILE = "TRACE-FILE";
    
    /// All hierarchical scope names
    static constexpr const char* SCOPES = "SCOPES";
    
    /// Scopes in current hierarchy level
    static constexpr const char* LOCAL_SCOPES = "LOCAL-SCOPES";

    /**
     * @brief Character that separates hierarchy levels in a signal name.
     *
     * `.`, as upstream WAL (ics-jku/wal) and as WAL++'s own `--fast-*` paths have
     * always spelled it: `tb.dut.counter`.
     *
     * It used to be `^`, which disagreed with BOTH of those at once. That left the
     * interpreter and the fast paths of the same binary naming the same signal two
     * different ways, and it took `^` away from the meaning upstream gives it, which
     * is to prefix a TRACE ID: `a^tb.dut.clk` selects the signal from the trace
     * loaded under id `a`.
     *
     * A name written the old way still resolves, so existing scripts keep working:
     * see resolve_signal_name() in signal_resolve.h.
     */
    static constexpr char SCOPE_SEPARATOR = '.';

protected:
    std::string tid;                    // Trace ID
    std::string filename;               // Source file path
    std::shared_ptr<TraceContainer> container;

    int current_index = 0;              // Current time index
    int max_index = 0;                  // Maximum time index

    std::vector<std::string> signal_names;
    std::vector<std::string> scope_names;
    std::string query_scope;  ///< see set_query_scope()
    std::unordered_map<std::string, Value> virtual_signals;
    std::vector<int> sampling_points;   ///< see set_sampling_points()

public:
    virtual ~Trace() = default;

    ///@name Signal Access
    ///@{
    /**
     * Get signal value at given time
     * 
     * Retrieves the value of a signal at (current_index + offset).
     * 
     * **Offset semantics:**
     * - offset=0: Current time
     * - offset>0: Future value (lookahead)
     * - offset<0: Past value (lookbehind)
     * 
     * **Scope handling:**
     * - Empty scope: Search in all scopes, prefer matching base name
     * - Specific scope: Only search in that scope
     * 
     * @param name Signal name (possibly with '~' scope prefix)
     * @param offset Time offset from current index (default: 0)
     * @param scope Optional scope prefix for hierarchical signals
     * @return The signal value at specified time
     * @throws TraceError if signal not found or index out of bounds
     */
    virtual Value signal_value(const std::string& name, int offset = 0,
                              const std::string& scope = "") = 0;

    /**
     * Get signal width in bits
     * 
     * @param name Signal name
     * @return Width in bits (1+ for vectors, 1 for scalars)
     * @throws TraceError if signal not found
     */
    virtual int signal_width(const std::string& name) = 0;

    /**
     * Check if signal exists in this trace
     * 
     * @param name Signal name
     * @return true if signal is present
     */
    virtual bool contains_signal(const std::string& name) const = 0;

    /**
     * Get all signal names
     * 
     * @return Vector of available signal names
     */
    virtual std::vector<std::string> get_all_signals() const = 0;

    /**
     * Get all scope names
     * 
     * @return Vector of hierarchical scope names
     */
    virtual std::vector<std::string> get_scopes() const;

    /**
     * @brief Scope that LOCAL-SCOPES and LOCAL-SIGNALS are resolved against.
     *
     * Upstream keeps this in the global variable CS and passes it down on every
     * special-signal read. The evaluator sets it before reading those two names.
     */
    void set_query_scope(const std::string& scope) { query_scope = scope; }
    const std::string& get_query_scope() const { return query_scope; }
    ///@}

    ///@name Time Navigation
    ///@{
    /**
     * Set current time index
     * 
     * All subsequent signal_value() calls use this index.
     * 
     * @param idx New time index (0 to max_index)
     * @throws TraceError if index out of range
     */
    virtual void set_index(int idx);

    /**
     * Step forward or backward in time
     * 
     * Moves current_index by steps amount.
     * Clamped to valid range [0, max_index].
     * 
     * @param steps Number of steps (positive=forward, negative=backward)
     * @return True if successful, false if clamped to boundary
     */
    virtual bool step(int steps = 1);
    ///@}

    ///@name Accessors
    ///@{
    /// Get trace ID assigned at load time
    const std::string& get_tid() const { return tid; }
    
    /// Get source file path
    const std::string& get_filename() const { return filename; }
    
    /// Get current time index
    int get_index() const { return current_index; }
    
    /// Get maximum time index
    int get_max_index() const { return max_index; }

    /**
     * @brief Shorten the trace, for `trim-trace`.
     *
     * Only ever downwards: a trace cannot grow data it never read.
     */
    void set_max_index(int idx) {
        if (idx >= 0 && idx <= max_index) {
            max_index = idx;
        }
    }

    /**
     * @brief Restrict the trace to the listed indices, for `sample-at`.
     *
     * Sampling points are a view, not a rewrite: the data stays, and the points say
     * which indices an analysis should visit. An empty list clears the restriction.
     */
    void set_sampling_points(const std::vector<int>& points) {
        sampling_points = points;
    }

    const std::vector<int>& get_sampling_points() const { return sampling_points; }

    /**
     * @brief Write this trace out as VCD, for `dump-trace`.
     *
     * With no path, the text goes to stdout and the returned string is empty;
     * otherwise it is the path written. Virtual signals are not included: their
     * values are expressions evaluated against a trace, not stored samples.
     */
    std::string dump_vcd(const std::string& path = "");

    /// Get actual simulation timestamp at current index (default: returns index)
    virtual int64_t get_timestamp() const { return static_cast<int64_t>(current_index); }
    
    /// Get all signal names
    const std::vector<std::string>& get_signal_names() const { return signal_names; }
    ///@}

    ///@name Virtual Signals
    ///@{
    /// Access virtual/derived signals
    const std::unordered_map<std::string, Value>& get_virtual_signals() const {
        return virtual_signals;
    }
    
    /// Create a virtual signal
    void set_virtual_signal(const std::string& name, const Value& value) {
        virtual_signals[name] = value;
    }
    ///@}

    ///@name Special Signals
    ///@{
    /**
     * Get special signal values
     * 
     * Handles meta-signals like INDEX, MAX-INDEX, SIGNALS, etc.
     * Override in subclasses to provide format-specific info.
     * 
     * @param name Special signal name (e.g., "SIGNALS")
     * @return Special signal value or nil if not found
     * @see SIGNALS, INDEX, MAX-INDEX, etc.
     */
    virtual Value get_special_signal(const std::string& name);
    ///@}

protected:
    /**
     * Constructor for subclasses
     * 
     * @param tid Trace ID for identification
     * @param filename Source file path
     * @param container Parent TraceContainer
     */
    Trace(const std::string& tid, const std::string& filename,
          std::shared_ptr<TraceContainer> container)
        : tid(tid), filename(filename), container(container) {}

    /**
     * Check if time index is valid
     * 
     * @param idx Index to check
     * @return true if 0 <= idx <= max_index
     */
    bool is_valid_index(int idx) const {
        return idx >= 0 && idx <= max_index;
    }
};

/**
 * @}
 * @endgroup trace
 */

} // namespace wal
