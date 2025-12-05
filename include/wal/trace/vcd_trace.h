#pragma once

#include "trace.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>

namespace wal {

/**
 * @brief VCD (Verilog Change Dump) file format trace reader
 * 
 * Reads and parses VCD waveform files produced by Verilog/VHDL simulators.
 * Stores signal values at different time points and provides query interface.
 */
class VCDTrace : public Trace {
private:
    // Signal metadata
    struct SignalInfo {
        std::string name;
        std::string identifier;  // Single character ID in VCD
        int width;               // Bit width of signal
        std::string scope;       // Hierarchical scope
    };

    std::unordered_map<std::string, SignalInfo> signal_info;
    std::unordered_map<std::string, int> signal_id_map;  // Map identifier to index

    // Waveform data: per timestamp, per signal ID
    struct TimeStep {
        int64_t timestamp;
        std::unordered_map<std::string, Value> signal_values;
    };
    std::vector<TimeStep> time_steps;
    std::unordered_map<int64_t, int> timestamp_index_map;

    // Current values at current index
    std::unordered_map<std::string, Value> current_values;

    // Optional scope filter for parse-time filtering
    std::string scope_filter;
    std::unordered_set<std::string> filtered_identifiers;  // IDs of signals to keep

    // String interning for identifiers to avoid repeated allocations
    std::unordered_map<std::string, std::string> identifier_intern_pool;
    
    // Helper to get interned identifier string
    const std::string& intern_identifier(const std::string& id);

public:
    /**
     * @brief Constructor
     * @param tid Trace ID
     * @param filename VCD file path
     * @param container Trace container
     * @param scope_filter Optional scope prefix to filter signals during parsing (e.g., "TOP^cpu")
     */
    VCDTrace(const std::string& tid, const std::string& filename,
             std::shared_ptr<TraceContainer> container,
             const std::string& scope_filter = "")
        : Trace(tid, filename, container), scope_filter(scope_filter) {}

    /**
     * @brief Parse and load VCD file
     */
    void load();

    /**
     * @brief Get signal value at given index + offset
     */
    Value signal_value(const std::string& name, int offset = 0,
                      const std::string& scope = "") override;

    /**
     * @brief Get signal width in bits
     */
    int signal_width(const std::string& name) override;

    /**
     * @brief Check if signal exists
     */
    bool contains_signal(const std::string& name) const override;

    /**
     * @brief Get all signal names
     */
    std::vector<std::string> get_all_signals() const override;

    /**
     * @brief Get actual timestamp value at current index
     */
    int64_t get_timestamp() const;

    /**
     * @brief Get direct access to time steps (for performance-critical operations)
     */
    const std::vector<TimeStep>& get_time_steps() const { return time_steps; }

    /**
     * @brief Get signal info map (for performance-critical operations)
     */
    const std::unordered_map<std::string, SignalInfo>& get_signal_info() const { return signal_info; }

private:
    /**
     * @brief Parse VCD header section
     */
    void parse_header(const std::vector<std::string>& lines, size_t& line_idx);

    /**
     * @brief Parse VCD data section
     */
    void parse_data(const std::vector<std::string>& lines, size_t& line_idx);

    /**
     * @brief Parse a single value change
     */
    void parse_value_change(const std::string& line, int& timestamp);

    /**
     * @brief Update current values at specific index
     */
    void update_current_values(int index);

    /**
     * @brief Convert binary string to integer value
     */
    static Value parse_bit_value(const std::string& bits);

    /**
     * @brief Read file into lines
     */
    static std::vector<std::string> read_file_lines(const std::string& filename);
    
    /**
     * @brief Read VCD file lines using memory-mapped I/O (faster for large files)
     */
    static std::vector<std::string> read_file_lines_mmap(const std::string& filename);
    
    /**
     * @brief Parse data section in parallel (thread-safe)
     */
    void parse_data_parallel(const std::vector<std::string>& lines, size_t line_idx);
    
    /**
     * @brief Parse a chunk of VCD data lines (for parallel processing)
     */
    void parse_data_chunk(const std::vector<std::string>& lines, 
                          size_t start_idx, 
                          size_t end_idx,
                          std::vector<TimeStep>& chunk_timesteps);
};

} // namespace wal
