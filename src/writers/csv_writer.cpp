// csv_writer.cpp - CSV format writer implementation
// Phase 3 Task 1, Phase 3 - Trace File Writing API

#include "wal/writers/csv_writer.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace wal::writers {

CSVWriter::CSVWriter(const std::string& filename)
    : filename_(filename),
      file_(filename, std::ios::out),
      time_scale_("ns"),
      time_base_(1),
      version_major_(1),
      version_minor_(0),
      delimiter_(','),
      quote_char_('"'),
      write_header_(true),
      output_mode_(OutputMode::TIME_INDEXED),
      timestamp_precision_(6),
      finalized_(false),
      signals_written_(0),
      rows_written_(0),
      bytes_written_(0),
      in_transaction_(false),
      current_time_(0) {
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }
}

CSVWriter::~CSVWriter() {
    if (!finalized_) {
        try {
            finalize();
        } catch (const std::exception& e) {
            // Silently handle finalization errors in destructor
        }
    }
}

void CSVWriter::set_format_version(int major, int minor) {
    if (header_written_) {
        throw std::runtime_error("Cannot set format version after header is written");
    }
    version_major_ = major;
    version_minor_ = minor;
}

void CSVWriter::set_time_scale(const std::string& scale) {
    if (header_written_) {
        throw std::runtime_error("Cannot set time scale after header is written");
    }

    static const std::vector<std::string> valid_scales = {
        "s", "ms", "us", "ns", "ps", "fs"
    };
    if (std::find(valid_scales.begin(), valid_scales.end(), scale) == valid_scales.end()) {
        throw std::runtime_error("Invalid time scale: " + scale);
    }

    time_scale_ = scale;
}

void CSVWriter::set_time_base(uint64_t base) {
    if (header_written_) {
        throw std::runtime_error("Cannot set time base after header is written");
    }
    if (base == 0) {
        throw std::runtime_error("Time base cannot be zero");
    }
    time_base_ = base;
}

void CSVWriter::set_delimiter(char delimiter) {
    if (header_written_) {
        throw std::runtime_error("Cannot set delimiter after header is written");
    }
    delimiter_ = delimiter;
}

void CSVWriter::set_quote_char(char quote) {
    if (header_written_) {
        throw std::runtime_error("Cannot set quote char after header is written");
    }
    quote_char_ = quote;
}

void CSVWriter::enable_header(bool enable) {
    if (header_written_) {
        throw std::runtime_error("Cannot modify header setting after write");
    }
    write_header_ = enable;
}

void CSVWriter::set_output_mode(OutputMode mode) {
    if (header_written_) {
        throw std::runtime_error("Cannot set output mode after header is written");
    }
    output_mode_ = mode;
}

void CSVWriter::set_timestamp_precision(int places) {
    if (header_written_) {
        throw std::runtime_error("Cannot set precision after header is written");
    }
    if (places < 0 || places > 15) {
        throw std::runtime_error("Precision must be 0-15");
    }
    timestamp_precision_ = places;
}

void CSVWriter::add_signal(
    const std::string& name,
    size_t width,
    const std::string& type) {
    if (header_written_) {
        throw std::runtime_error("Cannot add signals after header is written");
    }

    if (name.empty() || width == 0) {
        throw std::runtime_error("Invalid signal: empty name or zero width");
    }

    if (signals_.find(name) != signals_.end()) {
        throw std::runtime_error("Signal already exists: " + name);
    }

    signals_[name] = {width, type, ""};
    signal_order_.push_back(name);
}

void CSVWriter::add_hierarchical_signal(
    const std::string& hierarchy,
    const std::string& name,
    size_t width,
    const std::string& type) {
    if (header_written_) {
        throw std::runtime_error("Cannot add signals after header is written");
    }

    if (hierarchy.empty() || name.empty() || width == 0) {
        throw std::runtime_error("Invalid signal: empty hierarchy/name or zero width");
    }

    std::string full_name = hierarchy + "." + name;
    if (signals_.find(full_name) != signals_.end()) {
        throw std::runtime_error("Signal already exists: " + full_name);
    }

    signals_[full_name] = {width, type, hierarchy};
    signal_order_.push_back(full_name);
}

void CSVWriter::write_value(
    const std::string& signal_name,
    uint64_t time_index,
    const std::string& value) {
    ensure_header_written();

    auto it = signals_.find(signal_name);
    if (it == signals_.end()) {
        throw std::runtime_error("Signal not found: " + signal_name);
    }

    if (in_transaction_) {
        if (transaction_buffer_.empty() || transaction_buffer_.back().time != time_index) {
            transaction_buffer_.push_back({time_index, {}});
        }
        transaction_buffer_.back().values[signal_name] = value;
    } else {
        std::map<std::string, std::string> values;
        values[signal_name] = value;

        if (output_mode_ == OutputMode::TIME_INDEXED) {
            write_time_indexed_row(time_index, values);
        } else {
            write_event_indexed_rows(time_index, values);
        }
    }

    signals_written_++;
}

void CSVWriter::write_values(
    uint64_t time_index,
    const std::map<std::string, std::string>& values) {
    ensure_header_written();

    for (const auto& [signal_name, _] : values) {
        auto it = signals_.find(signal_name);
        if (it == signals_.end()) {
            throw std::runtime_error("Signal not found: " + signal_name);
        }
        signals_written_++;
    }

    if (in_transaction_) {
        if (transaction_buffer_.empty() || transaction_buffer_.back().time != time_index) {
            transaction_buffer_.push_back({time_index, values});
        } else {
            for (const auto& [name, value] : values) {
                transaction_buffer_.back().values[name] = value;
            }
        }
    } else {
        if (output_mode_ == OutputMode::TIME_INDEXED) {
            write_time_indexed_row(time_index, values);
        } else {
            write_event_indexed_rows(time_index, values);
        }
    }
}

void CSVWriter::begin_transaction() {
    if (in_transaction_) {
        throw std::runtime_error("Transaction already in progress");
    }
    in_transaction_ = true;
    transaction_buffer_.clear();
}

void CSVWriter::commit_transaction() {
    if (!in_transaction_) {
        throw std::runtime_error("No transaction in progress");
    }

    for (const auto& tv : transaction_buffer_) {
        if (output_mode_ == OutputMode::TIME_INDEXED) {
            write_time_indexed_row(tv.time, tv.values);
        } else {
            write_event_indexed_rows(tv.time, tv.values);
        }
    }

    in_transaction_ = false;
    transaction_buffer_.clear();
}

void CSVWriter::rollback_transaction() {
    if (!in_transaction_) {
        throw std::runtime_error("No transaction in progress");
    }
    in_transaction_ = false;
    transaction_buffer_.clear();
}

void CSVWriter::finalize() {
    if (finalized_) {
        return;
    }

    if (in_transaction_) {
        rollback_transaction();
    }

    file_.flush();
    finalized_ = true;
}

void CSVWriter::flush() {
    if (file_.is_open()) {
        file_.flush();
    }
}

size_t CSVWriter::signal_count() const {
    return signals_.size();
}

bool CSVWriter::has_signal(const std::string& signal_name) const {
    return signals_.find(signal_name) != signals_.end();
}

size_t CSVWriter::signal_width(const std::string& signal_name) const {
    auto it = signals_.find(signal_name);
    if (it == signals_.end()) {
        throw std::runtime_error("Signal not found: " + signal_name);
    }
    return it->second.width;
}

std::vector<std::string> CSVWriter::get_all_signals() const {
    return signal_order_;
}

std::map<std::string, int64_t> CSVWriter::get_statistics() const {
    return {
        {"signals_written", signals_written_},
        {"rows_written", rows_written_},
        {"bytes_written", bytes_written_},
        {"signals_defined", static_cast<int64_t>(signals_.size())}
    };
}

// ========== Private Implementation Methods ==========

void CSVWriter::ensure_header_written() {
    if (header_written_) {
        return;
    }

    if (write_header_) {
        write_csv_header();
    }

    header_written_ = true;
}

void CSVWriter::write_csv_header() {
    std::ostringstream oss;

    // Time column
    oss << escape_csv_field("Time") << delimiter_;

    // Signal columns
    for (size_t i = 0; i < signal_order_.size(); ++i) {
        oss << escape_csv_field(signal_order_[i]);
        if (i < signal_order_.size() - 1) {
            oss << delimiter_;
        }
    }

    oss << "\n";
    std::string header_line = oss.str();
    file_ << header_line;
    bytes_written_ += header_line.length();
}

void CSVWriter::write_time_indexed_row(
    uint64_t time,
    const std::map<std::string, std::string>& values) {
    std::ostringstream oss;

    // Time column
    oss << format_timestamp(time) << delimiter_;

    // Signal columns
    for (size_t i = 0; i < signal_order_.size(); ++i) {
        const auto& signal_name = signal_order_[i];
        auto it = values.find(signal_name);

        if (it != values.end()) {
            oss << escape_csv_field(it->second);
        } else {
            oss << "x";  // Unchanged value
        }

        if (i < signal_order_.size() - 1) {
            oss << delimiter_;
        }
    }

    oss << "\n";
    std::string row = oss.str();
    file_ << row;
    bytes_written_ += row.length();
    rows_written_++;
}

void CSVWriter::write_event_indexed_rows(
    uint64_t time,
    const std::map<std::string, std::string>& values) {
    // Event-indexed mode: one row per value change
    for (const auto& [signal_name, value] : values) {
        std::ostringstream oss;

        oss << format_timestamp(time) << delimiter_;
        oss << escape_csv_field(signal_name) << delimiter_;
        oss << escape_csv_field(value) << "\n";

        std::string row = oss.str();
        file_ << row;
        bytes_written_ += row.length();
        rows_written_++;
    }
}

std::string CSVWriter::escape_csv_field(const std::string& field) {
    // Check if field needs quoting
    bool needs_quoting = field.find(delimiter_) != std::string::npos ||
                         field.find(quote_char_) != std::string::npos ||
                         field.find('\n') != std::string::npos;

    if (!needs_quoting) {
        return field;
    }

    std::ostringstream oss;
    oss << quote_char_;

    for (char c : field) {
        if (c == quote_char_) {
            oss << quote_char_ << quote_char_;  // Escape quotes
        } else {
            oss << c;
        }
    }

    oss << quote_char_;
    return oss.str();
}

std::string CSVWriter::format_timestamp(uint64_t time) const {
    // Format timestamp with precision
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(timestamp_precision_);

    double scaled_time = static_cast<double>(time) / time_base_;

    // Apply time scale
    if (time_scale_ == "s") {
        // Already in seconds
    } else if (time_scale_ == "ms") {
        scaled_time /= 1e3;
    } else if (time_scale_ == "us") {
        scaled_time /= 1e6;
    } else if (time_scale_ == "ns") {
        scaled_time /= 1e9;
    } else if (time_scale_ == "ps") {
        scaled_time /= 1e12;
    } else if (time_scale_ == "fs") {
        scaled_time /= 1e15;
    }

    oss << scaled_time;
    return oss.str();
}

}  // namespace wal::writers
