// trace_writer.h - Base interface for writing trace files
// Phase 3 Task 1 - Trace File Writing API

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wal::writers {

/// Base interface for all trace format writers.
/// Provides common operations for writing signals and trace data.
class TraceWriter {
public:
    TraceWriter() = default;
    virtual ~TraceWriter() = default;

    // Prevent copying
    TraceWriter(const TraceWriter&) = delete;
    TraceWriter& operator=(const TraceWriter&) = delete;

    // Allow moving
    TraceWriter(TraceWriter&&) = default;
    TraceWriter& operator=(TraceWriter&&) = default;

    // ========== Configuration Methods ==========

    /// Set the format version for the output file.
    ///
    /// @param major Major version number
    /// @param minor Minor version number
    virtual void set_format_version(int major, int minor) = 0;

    /// Set the time scale for timestamp values.
    ///
    /// @param scale Time scale ("ps", "ns", "us", "ms", "s")
    virtual void set_time_scale(const std::string& scale) = 0;

    /// Set the time base for calculations.
    ///
    /// @param base Time base (typically 1)
    virtual void set_time_base(uint64_t base) = 0;

    // ========== Signal Definition Methods ==========

    /// Add a signal to the trace.
    ///
    /// @param name Signal name
    /// @param width Bit width of the signal
    /// @param type Signal type (default: "wire")
    virtual void add_signal(
        const std::string& name,
        size_t width,
        const std::string& type = "wire"
    ) = 0;

    /// Add a hierarchical signal (with scope path).
    ///
    /// @param hierarchy Dot-separated scope path (e.g., "top.cpu.alu")
    /// @param name Signal name within the hierarchy
    /// @param width Bit width of the signal
    /// @param type Signal type (default: "wire")
    virtual void add_hierarchical_signal(
        const std::string& hierarchy,
        const std::string& name,
        size_t width,
        const std::string& type = "wire"
    ) = 0;

    // ========== Data Writing Methods ==========

    /// Write a signal value at a specific time index.
    ///
    /// @param signal_name Name of the signal
    /// @param time_index Time index for this value
    /// @param value Signal value as string (binary, hex, or decimal)
    /// @throws std::runtime_error if signal not found
    virtual void write_value(
        const std::string& signal_name,
        uint64_t time_index,
        const std::string& value
    ) = 0;

    /// Write multiple signal values at the same time index.
    ///
    /// @param time_index Time index for all values
    /// @param values Map of signal_name → value
    /// @throws std::runtime_error if any signal not found
    virtual void write_values(
        uint64_t time_index,
        const std::map<std::string, std::string>& values
    ) = 0;

    // ========== Transaction Methods ==========

    /// Begin a transaction for batch operations.
    /// Useful for grouping multiple writes for efficiency.
    ///
    /// All writes after this are buffered until commit or rollback.
    virtual void begin_transaction() = 0;

    /// Commit the current transaction.
    /// Flushes all buffered writes to the file.
    virtual void commit_transaction() = 0;

    /// Rollback the current transaction.
    /// Discards all buffered writes since begin_transaction().
    virtual void rollback_transaction() = 0;

    // ========== File Operations ==========

    /// Finalize and close the trace file.
    /// Must be called before the writer is destroyed.
    /// Subsequent writes will fail.
    virtual void finalize() = 0;

    /// Flush buffered data to disk without finalizing.
    /// Useful for periodic checkpoints in long-running operations.
    virtual void flush() = 0;

    // ========== Query Methods ==========

    /// Get the number of signals currently defined.
    ///
    /// @return Number of signals
    virtual size_t signal_count() const = 0;

    /// Check if a signal has been defined.
    ///
    /// @param signal_name Name of the signal
    /// @return true if signal exists, false otherwise
    virtual bool has_signal(const std::string& signal_name) const = 0;

    /// Get the width of a defined signal.
    ///
    /// @param signal_name Name of the signal
    /// @return Signal width in bits
    /// @throws std::runtime_error if signal not found
    virtual size_t signal_width(const std::string& signal_name) const = 0;

    /// Get all defined signal names.
    ///
    /// @return Vector of signal names
    virtual std::vector<std::string> get_all_signals() const = 0;

    // ========== Statistics ==========

    /// Get write statistics.
    ///
    /// @return Map with keys:
    ///   - "signals_written": Number of signal values written
    ///   - "transactions": Number of committed transactions
    ///   - "bytes_written": Bytes written to file
    virtual std::map<std::string, int64_t> get_statistics() const = 0;
};

/// Factory function to create a writer for a specific format.
///
/// @param filename Output file path
/// @param format File format ("vcd", "fst", "csv", or "auto")
/// @return Unique pointer to TraceWriter instance
/// @throws std::runtime_error if format is not supported
std::unique_ptr<TraceWriter> create_writer(
    const std::string& filename,
    const std::string& format = "auto"
);

/// Detect file format from extension.
///
/// @param filename File path to analyze
/// @return Format name ("vcd", "fst", "csv", or "unknown")
std::string detect_format(const std::string& filename);

}  // namespace wal::writers
