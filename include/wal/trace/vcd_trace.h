#pragma once

#include "trace.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

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

public:
    /**
     * @brief Constructor
     */
    VCDTrace(const std::string& tid, const std::string& filename,
             std::shared_ptr<TraceContainer> container)
        : Trace(tid, filename, container) {}

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
};

} // namespace wal
