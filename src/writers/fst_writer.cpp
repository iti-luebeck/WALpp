// fst_writer.cpp - FST format writer implementation
// Phase 3 Task 1, Phase 2 - Trace File Writing API

#include "wal/writers/fst_writer.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <iomanip>
#include <sstream>

namespace wal::writers {

FSTWriter::FSTWriter(const std::string& filename)
    : filename_(filename),
      file_(filename, std::ios::out | std::ios::binary),
      time_scale_("ns"),
      time_base_(1),
      version_major_(1),
      version_minor_(0),
      compression_level_(6),
      compression_mode_(CompressionMode::ZLIB),
      signal_aliasing_enabled_(true),
      time_index_mode_(TimeIndexMode::DELTA_ENCODING),
      finalized_(false),
      next_signal_id_(0),
      signals_written_(0),
      value_changes_(0),
      bytes_written_(0),
      bytes_uncompressed_(0),
      in_transaction_(false) {
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }
}

FSTWriter::~FSTWriter() {
    if (!finalized_) {
        try {
            finalize();
        } catch (const std::exception& e) {
            // Silently handle finalization errors in destructor
        }
    }
}

void FSTWriter::set_format_version(int major, int minor) {
    if (header_written_) {
        throw std::runtime_error("Cannot set format version after header is written");
    }
    version_major_ = major;
    version_minor_ = minor;
}

void FSTWriter::set_time_scale(const std::string& scale) {
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

void FSTWriter::set_time_base(uint64_t base) {
    if (header_written_) {
        throw std::runtime_error("Cannot set time base after header is written");
    }
    if (base == 0) {
        throw std::runtime_error("Time base cannot be zero");
    }
    time_base_ = base;
}

void FSTWriter::set_compression_level(int level) {
    if (header_written_) {
        throw std::runtime_error("Cannot set compression after header is written");
    }
    if (level < 0 || level > 9) {
        throw std::runtime_error("Compression level must be 0-9");
    }
    compression_level_ = level;
}

void FSTWriter::set_compression_mode(CompressionMode mode) {
    if (header_written_) {
        throw std::runtime_error("Cannot set compression mode after header is written");
    }
    compression_mode_ = mode;
}

void FSTWriter::enable_signal_aliasing(bool enable) {
    if (header_written_) {
        throw std::runtime_error("Cannot modify settings after header is written");
    }
    signal_aliasing_enabled_ = enable;
}

void FSTWriter::set_time_index_mode(TimeIndexMode mode) {
    if (header_written_) {
        throw std::runtime_error("Cannot set time index mode after header is written");
    }
    time_index_mode_ = mode;
}

void FSTWriter::add_signal(
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

    signals_[name] = {
        next_signal_id_++,
        width,
        type,
        "",
        ""
    };
}

void FSTWriter::add_hierarchical_signal(
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

    signals_[full_name] = {
        next_signal_id_++,
        width,
        type,
        hierarchy,
        ""
    };
}

void FSTWriter::write_value(
    const std::string& signal_name,
    uint64_t time_index,
    const std::string& value) {
    ensure_header_written();

    auto it = signals_.find(signal_name);
    if (it == signals_.end()) {
        throw std::runtime_error("Signal not found: " + signal_name);
    }

    if (in_transaction_) {
        // Buffer in transaction
        if (transaction_buffer_.empty() || transaction_buffer_.back().time != time_index) {
            transaction_buffer_.push_back({time_index, {}});
        }
        transaction_buffer_.back().changes.push_back({it->second.fst_signal_id, value});
    } else {
        // Direct write
        if (time_index != current_time_) {
            current_time_ = time_index;
        }
        write_value_changes({time_index, {{it->second.fst_signal_id, value}}});
    }

    signals_written_++;
    value_changes_++;
}

void FSTWriter::write_values(
    uint64_t time_index,
    const std::map<std::string, std::string>& values) {
    ensure_header_written();

    std::vector<std::pair<uint32_t, std::string>> changes;

    for (const auto& [signal_name, value] : values) {
        auto it = signals_.find(signal_name);
        if (it == signals_.end()) {
            throw std::runtime_error("Signal not found: " + signal_name);
        }
        changes.push_back({it->second.fst_signal_id, value});
        signals_written_++;
        value_changes_++;
    }

    if (in_transaction_) {
        if (transaction_buffer_.empty() || transaction_buffer_.back().time != time_index) {
            transaction_buffer_.push_back({time_index, changes});
        } else {
            transaction_buffer_.back().changes.insert(
                transaction_buffer_.back().changes.end(),
                changes.begin(),
                changes.end()
            );
        }
    } else {
        if (time_index != current_time_) {
            current_time_ = time_index;
        }
        write_value_changes({time_index, changes});
    }
}

void FSTWriter::begin_transaction() {
    if (in_transaction_) {
        throw std::runtime_error("Transaction already in progress");
    }
    in_transaction_ = true;
    transaction_buffer_.clear();
}

void FSTWriter::commit_transaction() {
    if (!in_transaction_) {
        throw std::runtime_error("No transaction in progress");
    }

    for (const auto& tp : transaction_buffer_) {
        write_value_changes(tp);
    }

    in_transaction_ = false;
    transaction_buffer_.clear();
}

void FSTWriter::rollback_transaction() {
    if (!in_transaction_) {
        throw std::runtime_error("No transaction in progress");
    }
    in_transaction_ = false;
    transaction_buffer_.clear();
}

void FSTWriter::finalize() {
    if (finalized_) {
        return;
    }

    if (in_transaction_) {
        rollback_transaction();
    }

    // Write final index and close
    file_.flush();
    finalized_ = true;
}

void FSTWriter::flush() {
    if (file_.is_open()) {
        file_.flush();
    }
}

size_t FSTWriter::signal_count() const {
    return signals_.size();
}

bool FSTWriter::has_signal(const std::string& signal_name) const {
    return signals_.find(signal_name) != signals_.end();
}

size_t FSTWriter::signal_width(const std::string& signal_name) const {
    auto it = signals_.find(signal_name);
    if (it == signals_.end()) {
        throw std::runtime_error("Signal not found: " + signal_name);
    }
    return it->second.width;
}

std::vector<std::string> FSTWriter::get_all_signals() const {
    std::vector<std::string> result;
    for (const auto& [name, _] : signals_) {
        result.push_back(name);
    }
    return result;
}

std::map<std::string, int64_t> FSTWriter::get_statistics() const {
    return {
        {"signals_written", signals_written_},
        {"value_changes", value_changes_},
        {"bytes_written", bytes_written_},
        {"bytes_uncompressed", bytes_uncompressed_},
        {"signals_defined", static_cast<int64_t>(signals_.size())}
    };
}

double FSTWriter::get_compression_ratio() const {
    if (bytes_written_ == 0) {
        return 1.0;
    }
    return static_cast<double>(bytes_uncompressed_) / bytes_written_;
}

int64_t FSTWriter::get_value_change_count() const {
    return value_changes_;
}

// ========== Private Implementation Methods ==========

void FSTWriter::ensure_header_written() {
    if (header_written_) {
        return;
    }

    write_header();
    write_signal_definitions();

    header_written_ = true;
    current_time_ = 0;

    if (signal_aliasing_enabled_) {
        build_signal_aliases();
    }
}

void FSTWriter::write_header() {
    // FST file magic and version
    const char magic[] = "FST";
    file_.write(magic, 3);

    // Version
    uint8_t version = (version_major_ << 4) | version_minor_;
    file_.write(reinterpret_cast<const char*>(&version), 1);

    // Compression mode and level
    uint8_t compression_byte = (static_cast<uint8_t>(compression_mode_) << 4) 
                                | compression_level_;
    file_.write(reinterpret_cast<const char*>(&compression_byte), 1);

    // Time scale (encoded)
    uint8_t time_scale_code = 0;
    if (time_scale_ == "s") time_scale_code = 0;
    else if (time_scale_ == "ms") time_scale_code = 1;
    else if (time_scale_ == "us") time_scale_code = 2;
    else if (time_scale_ == "ns") time_scale_code = 3;
    else if (time_scale_ == "ps") time_scale_code = 4;
    else if (time_scale_ == "fs") time_scale_code = 5;

    file_.write(reinterpret_cast<const char*>(&time_scale_code), 1);

    // Time base
    file_.write(reinterpret_cast<const char*>(&time_base_), 8);

    // Number of signals
    uint32_t num_signals = signals_.size();
    file_.write(reinterpret_cast<const char*>(&num_signals), 4);

    bytes_written_ += 20;  // Header size
}

void FSTWriter::write_signal_definitions() {
    for (const auto& [name, sig_info] : signals_) {
        // Signal ID
        file_.write(reinterpret_cast<const char*>(&sig_info.fst_signal_id), 4);

        // Signal width
        uint16_t width = sig_info.width;
        file_.write(reinterpret_cast<const char*>(&width), 2);

        // Signal name length and data
        uint16_t name_len = name.length();
        file_.write(reinterpret_cast<const char*>(&name_len), 2);
        file_.write(name.c_str(), name_len);

        bytes_written_ += 8 + name_len;
    }
}

void FSTWriter::write_scope_hierarchy() {
    // Write scope stack for hierarchical organization
    for (const auto& scope : scope_stack_) {
        uint16_t scope_len = scope.length();
        file_.write(reinterpret_cast<const char*>(&scope_len), 2);
        file_.write(scope.c_str(), scope_len);
        bytes_written_ += 2 + scope_len;
    }
}

void FSTWriter::write_value_changes(const TimePoint& tp) {
    // Write time delta or absolute
    if (time_index_mode_ == TimeIndexMode::DELTA_ENCODING) {
        uint64_t time_delta = tp.time - current_time_;
        file_.write(reinterpret_cast<const char*>(&time_delta), 8);
    } else {
        file_.write(reinterpret_cast<const char*>(&tp.time), 8);
    }

    // Write value changes
    uint16_t num_changes = tp.changes.size();
    file_.write(reinterpret_cast<const char*>(&num_changes), 2);

    for (const auto& [signal_id, value] : tp.changes) {
        file_.write(reinterpret_cast<const char*>(&signal_id), 4);

        uint16_t value_len = value.length();
        file_.write(reinterpret_cast<const char*>(&value_len), 2);
        file_.write(value.c_str(), value_len);
    }

    bytes_written_ += 10 + (8 * tp.changes.size());
    bytes_uncompressed_ = bytes_written_;  // For now, no actual compression
}

void FSTWriter::write_compressed_data() {
    // Placeholder for compression implementation
    // Would use zlib or lz4 based on compression_mode_
}

void FSTWriter::build_signal_aliases() {
    // Build hash map for signal value aliasing
    for (auto& [name, sig_info] : signals_) {
        sig_info.value_hash = "";  // Would compute hash of typical values
    }
}

std::string FSTWriter::compute_value_hash(const std::string& value) {
    // Simple hash for value aliasing detection
    std::hash<std::string> hasher;
    size_t hash = hasher(value);
    std::ostringstream oss;
    oss << std::hex << hash;
    return oss.str();
}

}  // namespace wal::writers
