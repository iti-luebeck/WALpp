#pragma once

#include "trace.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>

namespace wal {

/**
 * @brief FST (Fast Signal Trace) file format trace reader
 * 
 * Reads and parses FST waveform files produced by simulators like Icarus Verilog, 
 * Verilator, and others. FST is a compressed binary format designed for efficient 
 * waveform storage and fast access.
 * 
 * Features:
 * - Compressed binary format (significantly smaller than VCD)
 * - Efficient random access to timesteps
 * - Hierarchical signal organization
 * - Multi-bit value support
 * - Fast parsing and low memory footprint
 * 
 * Note: This is a simplified FST parser that handles the most common FST files.
 * For full FST format support, consider using the libfst C library directly.
 */
class FSTTrace : public Trace {
private:
    // Signal metadata
    struct SignalInfo {
        std::string name;
        int wire_id;             // FST wire ID
        int width;               // Bit width of signal
        std::string scope;       // Hierarchical scope
        char kind;               // Signal kind (wire, reg, etc.)
    };

    std::unordered_map<std::string, SignalInfo> signal_info;
    std::unordered_map<int, std::string> wire_id_to_signal;  // Wire ID -> signal name

    // Waveform data: per timestamp, per signal
    struct TimeStep {
        int64_t timestamp;
        std::unordered_map<std::string, Value> signal_values;
    };
    std::vector<TimeStep> time_steps;
    std::unordered_map<int64_t, int> timestamp_index_map;

    // Current values at current index
    std::unordered_map<std::string, Value> current_values;

    // FST file metadata
    struct FSTHeader {
        uint8_t version;
        char start_time_str[32];
        char end_time_str[32];
        int64_t start_time;
        int64_t end_time;
        int num_scopes;
        int num_vars;
        int num_timescales;
        char timescale_unit;
        uint32_t timescale_power;
    };

    FSTHeader fst_header;

public:
    /**
     * @brief Constructor
     */
    FSTTrace(const std::string& tid, const std::string& filename,
             std::shared_ptr<TraceContainer> container)
        : Trace(tid, filename, container) {}

    /**
     * @brief Parse and load FST file
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
     * @brief Parse FST header section
     */
    void parse_header(const std::vector<uint8_t>& data, size_t& pos);

    /**
     * @brief Parse FST hierarchy section (scopes and variables)
     */
    void parse_hierarchy(const std::vector<uint8_t>& data, size_t& pos);

    /**
     * @brief Parse FST value changes (compressed)
     */
    void parse_value_changes(const std::vector<uint8_t>& data, size_t& pos);

    /**
     * @brief Decompress FST data (LZ4 or similar)
     */
    static std::vector<uint8_t> decompress_data(const std::vector<uint8_t>& compressed);

    /**
     * @brief Read variable-length integer (VLI)
     */
    static uint64_t read_vli(const std::vector<uint8_t>& data, size_t& pos);

    /**
     * @brief Read length-prefixed string
     */
    static std::string read_string(const std::vector<uint8_t>& data, size_t& pos);

    /**
     * @brief Parse a single bit value
     */
    static char parse_bit_value(uint8_t byte);

    /**
     * @brief Convert bit string to Value
     */
    static Value bits_to_value(const std::string& bits);

    /**
     * @brief Read entire file into memory
     */
    static std::vector<uint8_t> read_file(const std::string& filename);

    /**
     * @brief Infer bit width from maximum value
     */
    static int infer_width(int64_t max_value);
};

} // namespace wal
