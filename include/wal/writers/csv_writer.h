// csv_writer.h - CSV (Comma-Separated Values) format writer
// Phase 3 Task 1, Phase 3 - Trace File Writing API

#pragma once

#include "wal/writers/trace_writer.h"

#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wal::writers {

/// CSV (Comma-Separated Values) format writer for human-readable trace files.
///
/// Supports flexible delimiter configuration, headers, and time/event indexing.
/// Good for data analysis and import into spreadsheet applications.
class CSVWriter : public TraceWriter {
public:
    /// Output mode for CSV generation.
    enum class OutputMode {
        TIME_INDEXED = 0,    // One row per time point
        EVENT_INDEXED = 1    // One row per signal change
    };

    /// Construct a CSV writer for the specified file.
    ///
    /// @param filename Output file path (e.g., "trace.csv")
    /// @throws std::runtime_error if file cannot be opened
    explicit CSVWriter(const std::string& filename);

    ~CSVWriter() override;

    // Configuration methods
    void set_format_version(int major, int minor) override;
    void set_time_scale(const std::string& scale) override;
    void set_time_base(uint64_t base) override;

    // CSV-specific configuration
    /// Set field delimiter (default: ',').
    /// @param delimiter Character to use as field separator
    void set_delimiter(char delimiter);

    /// Set quote character for fields (default: '"').
    /// @param quote Character to use for quoting
    void set_quote_char(char quote);

    /// Enable or disable header row (default: true).
    /// @param enable true to write header
    void enable_header(bool enable);

    /// Set output mode (time-indexed or event-indexed).
    /// @param mode Output mode
    void set_output_mode(OutputMode mode);

    /// Set number of decimal places for timestamps (default: 6).
    /// @param places Number of decimal places
    void set_timestamp_precision(int places);

    // Signal definition methods
    void add_signal(
        const std::string& name,
        size_t width,
        const std::string& type = "wire"
    ) override;

    void add_hierarchical_signal(
        const std::string& hierarchy,
        const std::string& name,
        size_t width,
        const std::string& type = "wire"
    ) override;

    // Data writing methods
    void write_value(
        const std::string& signal_name,
        uint64_t time_index,
        const std::string& value
    ) override;

    void write_values(
        uint64_t time_index,
        const std::map<std::string, std::string>& values
    ) override;

    // Transaction methods
    void begin_transaction() override;
    void commit_transaction() override;
    void rollback_transaction() override;

    // File operations
    void finalize() override;
    void flush() override;

    // Query methods
    size_t signal_count() const override;
    bool has_signal(const std::string& signal_name) const override;
    size_t signal_width(const std::string& signal_name) const override;
    std::vector<std::string> get_all_signals() const override;

    // Statistics
    std::map<std::string, int64_t> get_statistics() const override;

private:
    struct SignalInfo {
        size_t width;
        std::string type;
        std::string hierarchy;
    };

    struct TimeValue {
        uint64_t time;
        std::map<std::string, std::string> values;
    };

    // File and metadata
    std::string filename_;
    std::ofstream file_;

    // Configuration
    std::string time_scale_;
    uint64_t time_base_;
    int version_major_;
    int version_minor_;
    char delimiter_;
    char quote_char_;
    bool write_header_;
    OutputMode output_mode_;
    int timestamp_precision_;

    // Signal management
    std::map<std::string, SignalInfo> signals_;
    std::vector<std::string> signal_order_;  // For consistent column ordering

    // State tracking
    bool header_written_ = false;
    bool finalized_;
    uint64_t current_time_ = 0;
    bool in_transaction_;
    std::vector<TimeValue> transaction_buffer_;

    // Statistics
    int64_t signals_written_;
    int64_t rows_written_;
    int64_t bytes_written_;

    // Private helper methods
    void ensure_header_written();
    void write_csv_header();
    void write_time_indexed_row(uint64_t time, 
                                const std::map<std::string, std::string>& values);
    void write_event_indexed_rows(uint64_t time,
                                   const std::map<std::string, std::string>& values);
    std::string escape_csv_field(const std::string& field);
    std::string format_timestamp(uint64_t time) const;
};

}  // namespace wal::writers
