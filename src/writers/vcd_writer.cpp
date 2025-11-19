// vcd_writer.cpp - VCD trace file writer implementation
// Phase 3 Task 1 - Trace File Writing API

#include "wal/writers/vcd_writer.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace wal::writers {

VCDWriter::VCDWriter(const std::string& filename)
    : filename_(filename),
      file_(filename, std::ios::out | std::ios::binary),
      time_scale_("ns"),
      time_base_(1),
      version_major_(2),
      version_minor_(0),
      finalized_(false),
      next_signal_id_(0),
      signals_written_(0),
      bytes_written_(0),
      in_transaction_(false) {
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }
}

VCDWriter::~VCDWriter() {
    if (!finalized_) {
        try {
            finalize();
        } catch (const std::exception& e) {
            std::cerr << "Error in ~VCDWriter: " << e.what() << std::endl;
        }
    }
}

void VCDWriter::set_format_version(int major, int minor) {
    if (header_written_) {
        throw std::runtime_error("Cannot set format version after header is written");
    }
    version_major_ = major;
    version_minor_ = minor;
}

void VCDWriter::set_time_scale(const std::string& scale) {
    if (header_written_) {
        throw std::runtime_error("Cannot set time scale after header is written");
    }

    // Validate time scale
    static const std::vector<std::string> valid_scales = {
        "s", "ms", "us", "ns", "ps", "fs"
    };
    if (std::find(valid_scales.begin(), valid_scales.end(), scale) == valid_scales.end()) {
        throw std::runtime_error("Invalid time scale: " + scale);
    }

    time_scale_ = scale;
}

void VCDWriter::set_time_base(uint64_t base) {
    if (header_written_) {
        throw std::runtime_error("Cannot set time base after header is written");
    }
    if (base == 0) {
        throw std::runtime_error("Time base cannot be zero");
    }
    time_base_ = base;
}

void VCDWriter::add_signal(
    const std::string& name,
    size_t width,
    const std::string& type) {
    if (header_written_) {
        throw std::runtime_error("Cannot add signals after header is written");
    }

    if (name.empty()) {
        throw std::runtime_error("Signal name cannot be empty");
    }

    if (width == 0) {
        throw std::runtime_error("Signal width must be > 0");
    }

    if (signals_.find(name) != signals_.end()) {
        throw std::runtime_error("Signal already exists: " + name);
    }

    std::string signal_id = generate_signal_id(next_signal_id_++);
    signals_[name] = {signal_id, width, type, ""};
}

void VCDWriter::add_hierarchical_signal(
    const std::string& hierarchy,
    const std::string& name,
    size_t width,
    const std::string& type) {
    if (header_written_) {
        throw std::runtime_error("Cannot add signals after header is written");
    }

    if (hierarchy.empty() || name.empty()) {
        throw std::runtime_error("Hierarchy or signal name cannot be empty");
    }

    if (width == 0) {
        throw std::runtime_error("Signal width must be > 0");
    }

    std::string full_name = hierarchy + "." + name;
    if (signals_.find(full_name) != signals_.end()) {
        throw std::runtime_error("Signal already exists: " + full_name);
    }

    std::string signal_id = generate_signal_id(next_signal_id_++);
    signals_[full_name] = {signal_id, width, type, hierarchy};

    // Track hierarchy scopes
    scopes_.insert(hierarchy);
}

void VCDWriter::write_value(
    const std::string& signal_name,
    uint64_t time_index,
    const std::string& value) {
    ensure_header_written();

    auto it = signals_.find(signal_name);
    if (it == signals_.end()) {
        throw std::runtime_error("Signal not found: " + signal_name);
    }

    if (!in_transaction_) {
        write_timestamp_if_needed(time_index);
    }

    write_value_line(it->second.signal_id, value, it->second.width);
    signals_written_++;

    if (!in_transaction_) {
        current_time_ = time_index;
    }
}

void VCDWriter::write_values(
    uint64_t time_index,
    const std::map<std::string, std::string>& values) {
    ensure_header_written();

    if (!in_transaction_) {
        write_timestamp_if_needed(time_index);
    }

    for (const auto& [signal_name, value] : values) {
        auto it = signals_.find(signal_name);
        if (it == signals_.end()) {
            throw std::runtime_error("Signal not found: " + signal_name);
        }

        write_value_line(it->second.signal_id, value, it->second.width);
        signals_written_++;
    }

    if (!in_transaction_) {
        current_time_ = time_index;
    }
}

void VCDWriter::begin_transaction() {
    if (in_transaction_) {
        throw std::runtime_error("Transaction already in progress");
    }
    in_transaction_ = true;
}

void VCDWriter::commit_transaction() {
    if (!in_transaction_) {
        throw std::runtime_error("No transaction in progress");
    }
    in_transaction_ = false;
    // Buffer is automatically flushed in write_value operations
}

void VCDWriter::rollback_transaction() {
    if (!in_transaction_) {
        throw std::runtime_error("No transaction in progress");
    }
    in_transaction_ = false;
    // In a full implementation, we'd need to track the buffer start
    // For now, this is a no-op as we don't buffer
}

void VCDWriter::finalize() {
    if (finalized_) {
        return;
    }

    if (in_transaction_) {
        rollback_transaction();
    }

    file_ << "$end" << std::endl;
    file_.flush();

    finalized_ = true;
}

void VCDWriter::flush() {
    if (file_.is_open()) {
        file_.flush();
    }
}

size_t VCDWriter::signal_count() const {
    return signals_.size();
}

bool VCDWriter::has_signal(const std::string& signal_name) const {
    return signals_.find(signal_name) != signals_.end();
}

size_t VCDWriter::signal_width(const std::string& signal_name) const {
    auto it = signals_.find(signal_name);
    if (it == signals_.end()) {
        throw std::runtime_error("Signal not found: " + signal_name);
    }
    return it->second.width;
}

std::vector<std::string> VCDWriter::get_all_signals() const {
    std::vector<std::string> result;
    for (const auto& [name, _] : signals_) {
        result.push_back(name);
    }
    return result;
}

std::map<std::string, int64_t> VCDWriter::get_statistics() const {
    return {
        {"signals_written", static_cast<int64_t>(signals_written_)},
        {"transactions", 0},  // Would track in full implementation
        {"bytes_written", bytes_written_}
    };
}

// ========== Private Implementation Methods ==========

std::string VCDWriter::generate_signal_id(size_t index) {
    // Generate short identifiers for signals (a, b, c, ... aa, ab, ...)
    std::string id;
    do {
        id = char('a' + (index % 26)) + id;
        index /= 26;
    } while (index > 0);
    return id;
}

void VCDWriter::ensure_header_written() {
    if (header_written_) {
        return;
    }

    write_header();
    header_written_ = true;
    current_time_ = 0;
}

void VCDWriter::write_header() {
    std::ostringstream oss;

    // $version section
    oss << "$version" << std::endl;
    oss << "WAL Trace Writer v" << version_major_ << "." << version_minor_ << std::endl;
    oss << "$end" << std::endl;

    // $date section
    oss << "$date" << std::endl;
    oss << "Generated by WAL VCDWriter" << std::endl;
    oss << "$end" << std::endl;

    // $timescale section
    oss << "$timescale" << std::endl;
    oss << "1" << time_scale_ << std::endl;
    oss << "$end" << std::endl;

    // $scope and $var sections
    if (!scopes_.empty()) {
        write_hierarchical_signals(oss);
    } else {
        write_flat_signals(oss);
    }

    // $upscope to close all scopes
    for (size_t i = 0; i < scopes_.size(); ++i) {
        oss << "$upscope" << std::endl;
        oss << "$end" << std::endl;
    }

    // Initial values
    oss << "$dumpvars" << std::endl;
    for (const auto& [_, sig_info] : signals_) {
        oss << "x" << sig_info.signal_id << std::endl;  // Initial value is 'x' (unknown)
    }
    oss << "$end" << std::endl;

    std::string header = oss.str();
    file_ << header;
    bytes_written_ += header.length();
}

void VCDWriter::write_flat_signals(std::ostringstream& oss) {
    oss << "$scope" << std::endl;
    oss << "module" << std::endl;
    oss << "top" << std::endl;
    oss << "$end" << std::endl;

    for (const auto& [name, sig_info] : signals_) {
        oss << "$var" << std::endl;
        oss << sig_info.type << " " << sig_info.width << " "
            << sig_info.signal_id << " " << name << std::endl;
        oss << "$end" << std::endl;
    }
}

void VCDWriter::write_hierarchical_signals(std::ostringstream& oss) {
    for (const auto& scope : scopes_) {
        oss << "$scope" << std::endl;
        oss << "module" << std::endl;
        oss << scope << std::endl;
        oss << "$end" << std::endl;

        // Write signals in this scope
        for (const auto& [name, sig_info] : signals_) {
            if (sig_info.hierarchy == scope) {
                oss << "$var" << std::endl;
                oss << sig_info.type << " " << sig_info.width << " "
                    << sig_info.signal_id << " " << name << std::endl;
                oss << "$end" << std::endl;
            }
        }
    }
}

void VCDWriter::write_timestamp_if_needed(uint64_t time_index) {
    if (time_index != current_time_) {
        file_ << "#" << time_index << std::endl;
        bytes_written_ += 20;  // Rough estimate
        current_time_ = time_index;
    }
}

void VCDWriter::write_value_line(
    const std::string& signal_id,
    const std::string& value,
    size_t width) {
    std::ostringstream oss;

    if (width == 1) {
        // Single-bit value
        oss << value.back() << signal_id << std::endl;
    } else {
        // Multi-bit value
        oss << "b" << value << " " << signal_id << std::endl;
    }

    std::string line = oss.str();
    file_ << line;
    bytes_written_ += line.length();
}

}  // namespace wal::writers
