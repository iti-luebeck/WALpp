// vcd_writer.h - VCD format trace writer
// Phase 3 Task 1 - Trace File Writing API

#pragma once

#include "wal/writers/trace_writer.h"

#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace wal::writers {

/// VCD (Value Change Dump) format trace file writer.
///
/// Implements IEEE Std 1364-2001 (Verilog) standard for trace files.
/// Supports hierarchical signal organization and efficient multi-signal updates.
class VCDWriter : public TraceWriter {
public:
    /// Construct a VCD writer for the specified file.
    ///
    /// @param filename Output file path (e.g., "trace.vcd")
    /// @throws std::runtime_error if file cannot be opened
    explicit VCDWriter(const std::string& filename);

    ~VCDWriter() override;

    // Configuration methods
    void set_format_version(int major, int minor) override;
    void set_time_scale(const std::string& scale) override;
    void set_time_base(uint64_t base) override;

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
        std::string signal_id;
        size_t width;
        std::string type;
        std::string hierarchy;  // Empty for flat signals
    };

    // File and metadata
    std::string filename_;
    std::ofstream file_;

    // Configuration
    std::string time_scale_;
    uint64_t time_base_;
    int version_major_;
    int version_minor_;

    // Signal management
    std::map<std::string, SignalInfo> signals_;
    std::set<std::string> scopes_;
    size_t next_signal_id_;

    // State tracking
    bool header_written_ = false;
    bool finalized_;
    uint64_t current_time_ = 0;
    /// False until the first `#<time>` marker has been emitted. Without this, values
    /// written at time 0 would be emitted with no timestamp at all, because
    /// current_time_ already reads 0.
    bool time_emitted_ = false;
    bool in_transaction_;

    // Statistics
    int64_t signals_written_;
    int64_t bytes_written_;

    // Private helper methods
    static std::string generate_signal_id(size_t index);
    void ensure_header_written();
    void write_header();
    void write_flat_signals(std::ostringstream& oss);
    void write_hierarchical_signals(std::ostringstream& oss);
    void write_timestamp_if_needed(uint64_t time_index);
    /// Leaf name of a (possibly dotted) hierarchical signal name: VCD `$var` declarations
    /// carry the name *within* the enclosing scope, not the full path.
    static std::string leaf_name(const std::string& full_name);
    /// Normalise a caller-supplied value to the VCD bit representation for `width` bits.
    /// Accepts binary (with x/z), `0x`-prefixed hex, or decimal.
    static std::string to_bit_string(const std::string& value, size_t width);
    void write_value_line(
        const std::string& signal_id,
        const std::string& value,
        size_t width
    );
};

}  // namespace wal::writers
