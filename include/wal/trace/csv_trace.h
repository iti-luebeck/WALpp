#pragma once

#include "trace.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace wal {

/**
 * @brief CSV (Comma-Separated Values) file format trace reader
 * 
 * Reads and parses CSV waveform files. Supports both time-series and signal-based formats.
 * 
 * Format variants:
 * 1. Time-based: Each row is a timestep, columns are signals
 *    timestamp,signal1,signal2,...
 *    0,1,0
 *    10,0,1
 * 
 * 2. Signal-based: Each column is a signal with timestamp,value pairs
 *    time,clk,data,valid
 *    0,1,255,0
 *    10,0,128,1
 */
class CSVTrace : public Trace {
private:
    // Signal metadata
    struct SignalInfo {
        std::string name;
        int width;               // Bit width of signal (inferred from max value)
        size_t column_index;     // Position in CSV
    };

    std::unordered_map<std::string, SignalInfo> signal_info;
    std::unordered_map<size_t, std::string> column_to_signal;  // Column index -> signal name

    // Waveform data: per timestamp, per signal
    struct TimeStep {
        int64_t timestamp;
        std::unordered_map<std::string, Value> signal_values;
    };
    std::vector<TimeStep> time_steps;
    std::unordered_map<int64_t, int> timestamp_index_map;

    // Current values at current index
    std::unordered_map<std::string, Value> current_values;

    // CSV parsing options
    char delimiter = ',';
    bool has_header = true;
    std::string time_column = "time";
    bool is_time_based = true;  // vs signal-based

public:
    /**
     * @brief Constructor
     */
    CSVTrace(const std::string& tid, const std::string& filename,
             std::shared_ptr<TraceContainer> container)
        : Trace(tid, filename, container) {}

    /**
     * @brief Set CSV delimiter (default: ',')
     */
    void set_delimiter(char delim) { delimiter = delim; }

    /**
     * @brief Set whether CSV has header row (default: true)
     */
    void set_has_header(bool has_h) { has_header = has_h; }

    /**
     * @brief Set time column name (default: "time")
     */
    void set_time_column(const std::string& col) { time_column = col; }

    /**
     * @brief Parse and load CSV file
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
     * @brief Parse CSV header to extract signal names
     */
    void parse_header(const std::vector<std::string>& lines);

    /**
     * @brief Parse CSV data rows
     */
    void parse_data(const std::vector<std::string>& lines);

    /**
     * @brief Split CSV line by delimiter, handling quoted fields
     */
    static std::vector<std::string> split_csv_line(const std::string& line, char delim);

    /**
     * @brief Convert string to numeric value
     */
    static Value parse_value(const std::string& value);

    /**
     * @brief Infer bit width from value
     */
    static int infer_width(int64_t max_value);

    /**
     * @brief Read file into lines
     */
    static std::vector<std::string> read_file_lines(const std::string& filename);
};

} // namespace wal
