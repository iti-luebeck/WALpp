// fst_writer.h - FST (Fast Signal Trace) format writer
// Phase 3 Task 1, Phase 2 - Trace File Writing API

#pragma once

#include "wal/writers/trace_writer.h"

#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wal::writers {

/// FST (Fast Signal Trace) format writer for efficient binary trace storage.
///
/// Implements fast, compressed binary format for waveform traces.
/// Supports signal aliasing, compression, and random-access indexing.
class FSTWriter : public TraceWriter {
public:
    /// Compression modes for FST files.
    enum class CompressionMode {
        UNCOMPRESSED = 0,
        ZLIB = 1,
        LZ4 = 2
    };

    /// Time indexing mode for trace generation.
    enum class TimeIndexMode {
        DELTA_ENCODING = 0,  // Store time deltas (more compact)
        ABSOLUTE = 1         // Store absolute timestamps
    };

    /// Construct an FST writer for the specified file.
    ///
    /// @param filename Output file path (e.g., "trace.fst")
    /// @throws std::runtime_error if file cannot be opened
    explicit FSTWriter(const std::string& filename);

    ~FSTWriter() override;

    // Configuration methods
    void set_format_version(int major, int minor) override;
    void set_time_scale(const std::string& scale) override;
    void set_time_base(uint64_t base) override;

    // FST-specific configuration
    /// Set compression level (0-9, default: 6).
    /// @param level Compression level (0=none, 9=maximum)
    void set_compression_level(int level);

    /// Set compression mode (ZLIB, LZ4, or UNCOMPRESSED).
    /// @param mode Compression mode to use
    void set_compression_mode(CompressionMode mode);

    /// Enable or disable signal aliasing (default: enabled).
    /// Signal aliasing reuses identical signals to save space.
    /// @param enable true to enable aliasing
    void enable_signal_aliasing(bool enable);

    /// Set time indexing mode (delta or absolute).
    /// @param mode Time encoding mode
    void set_time_index_mode(TimeIndexMode mode);

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

    // FST-specific queries
    /// Get compression ratio achieved.
    /// @return Compression ratio (uncompressed / compressed size)
    double get_compression_ratio() const;

    /// Get number of value changes recorded.
    /// @return Total value changes
    int64_t get_value_change_count() const;

private:
    struct SignalInfo {
        uint32_t fst_signal_id;
        size_t width;
        std::string type;
        std::string hierarchy;
        std::string value_hash;  // For aliasing detection
    };

    struct TimePoint {
        uint64_t time;
        std::vector<std::pair<uint32_t, std::string>> changes;  // signal_id, value
    };

    // File and metadata
    std::string filename_;
    std::ofstream file_;

    // Configuration
    std::string time_scale_;
    uint64_t time_base_;
    int version_major_;
    int version_minor_;
    int compression_level_;
    CompressionMode compression_mode_;
    bool signal_aliasing_enabled_;
    TimeIndexMode time_index_mode_;

    // Signal management
    std::map<std::string, SignalInfo> signals_;
    std::map<std::string, uint32_t> signal_aliases_;  // For aliasing
    std::vector<std::string> scope_stack_;
    uint32_t next_signal_id_;

    // State tracking
    bool header_written_ = false;
    bool finalized_;
    uint64_t current_time_ = 0;
    bool in_transaction_;
    std::vector<TimePoint> transaction_buffer_;

    // Compression
    std::vector<uint8_t> uncompressed_data_;
    std::vector<uint8_t> compressed_data_;

    // Statistics
    int64_t signals_written_;
    int64_t value_changes_;
    int64_t bytes_written_;
    int64_t bytes_uncompressed_;

    // Private helper methods
    void ensure_header_written();
    void write_header();
    void write_scope_hierarchy();
    void write_signal_definitions();
    void write_value_changes(const TimePoint& tp);
    void write_compressed_data();
    void build_signal_aliases();
    std::string compute_value_hash(const std::string& value);
};

}  // namespace wal::writers
