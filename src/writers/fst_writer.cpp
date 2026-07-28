// fst_writer.cpp - FST format writer implementation
//
// Built on the bundled GTKWave fstapi -- the same library src/fast/vcd_parser.cpp reads FST
// with. The previous implementation hand-wrote bytes through an ofstream and produced files
// that were not FST at all (~80 bytes, no header block), so nothing could read them back,
// WAL itself included.

#include "wal/writers/fst_writer.h"

#include "fstapi.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace wal::writers {

namespace {

/// fstapi identifies variables by fstHandle (a uint32_t); 0 is never a valid handle.
constexpr uint32_t kInvalidHandle = 0;

} // namespace

FSTWriter::FSTWriter(const std::string& filename)
    : filename_(filename),
      time_scale_("ns"),
      time_base_(1),
      version_major_(1),
      version_minor_(0),
      compression_level_(6),
      compression_mode_(CompressionMode::ZLIB),
      signal_aliasing_enabled_(true),
      time_index_mode_(TimeIndexMode::DELTA_ENCODING) {
    ctx_ = fstWriterCreate(filename.c_str(), /*use_compressed_hier=*/1);
    if (ctx_ == nullptr) {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }
    fstWriterSetTimescaleFromString(ctx_, "1ns");
    fstWriterSetPackType(ctx_, FST_WR_PT_ZLIB);
}

FSTWriter::~FSTWriter() {
    if (!finalized_) {
        try {
            finalize();
        } catch (const std::exception& e) {
            std::cerr << "Error in ~FSTWriter: " << e.what() << std::endl;
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
    static const std::vector<std::string> valid_scales = {"s", "ms", "us", "ns", "ps", "fs"};
    if (std::find(valid_scales.begin(), valid_scales.end(), scale) == valid_scales.end()) {
        throw std::runtime_error("Invalid time scale: " + scale);
    }
    time_scale_ = scale;
    fstWriterSetTimescaleFromString(ctx_, ("1" + scale).c_str());
}

void FSTWriter::set_time_base(uint64_t base) {
    if (header_written_) {
        throw std::runtime_error("Cannot set time base after header is written");
    }
    time_base_ = base;
}

void FSTWriter::set_compression_level(int level) {
    if (level < 0 || level > 9) {
        throw std::runtime_error("Compression level must be 0-9");
    }
    compression_level_ = level;
}

void FSTWriter::set_compression_mode(CompressionMode mode) {
    compression_mode_ = mode;
    if (ctx_ != nullptr) {
        fstWriterSetPackType(ctx_, mode == CompressionMode::LZ4 ? FST_WR_PT_LZ4 : FST_WR_PT_ZLIB);
    }
}

void FSTWriter::enable_signal_aliasing(bool enable) {
    signal_aliasing_enabled_ = enable;
}

void FSTWriter::set_time_index_mode(TimeIndexMode mode) {
    time_index_mode_ = mode;
}

void FSTWriter::add_signal(const std::string& name, size_t width, const std::string& type) {
    add_hierarchical_signal("", name, width, type);
}

void FSTWriter::add_hierarchical_signal(const std::string& hierarchy,
                                        const std::string& name,
                                        size_t width,
                                        const std::string& type) {
    if (header_written_) {
        throw std::runtime_error("Cannot add signals after the header is written");
    }
    if (width == 0) {
        throw std::runtime_error("Signal width must be at least 1: " + name);
    }
    const std::string full = hierarchy.empty() ? name : hierarchy + "." + name;
    if (signals_.count(full) != 0) {
        throw std::runtime_error("Signal already exists: " + full);
    }
    signals_[full] = SignalInfo{kInvalidHandle, width, type, hierarchy};
    declaration_order_.push_back(full);
    if (!hierarchy.empty()) {
        scopes_.insert(hierarchy);
    }
}

void FSTWriter::ensure_header_written() {
    if (header_written_) {
        return;
    }
    // fstapi wants the entire hierarchy declared before any value change, with variables
    // created inside the scope they belong to.
    if (scopes_.empty()) {
        fstWriterSetScope(ctx_, FST_ST_VCD_MODULE, "top", nullptr);
        for (const auto& full : declaration_order_) {
            SignalInfo& info = signals_[full];
            info.handle = fstWriterCreateVar(ctx_, FST_VT_VCD_WIRE, FST_VD_IMPLICIT,
                                             static_cast<uint32_t>(info.width),
                                             leaf_name(full).c_str(), 0);
        }
        fstWriterSetUpscope(ctx_);
    } else {
        for (const auto& scope : scopes_) {
            fstWriterSetScope(ctx_, FST_ST_VCD_MODULE, scope.c_str(), nullptr);
            for (const auto& full : declaration_order_) {
                SignalInfo& info = signals_[full];
                if (info.hierarchy == scope) {
                    info.handle = fstWriterCreateVar(ctx_, FST_VT_VCD_WIRE, FST_VD_IMPLICIT,
                                                     static_cast<uint32_t>(info.width),
                                                     leaf_name(full).c_str(), 0);
                }
            }
            fstWriterSetUpscope(ctx_);
        }
    }
    header_written_ = true;
}

void FSTWriter::write_value(const std::string& signal_name,
                            uint64_t time_index,
                            const std::string& value) {
    ensure_header_written();

    auto it = signals_.find(signal_name);
    if (it == signals_.end()) {
        throw std::runtime_error("Signal not found: " + signal_name);
    }

    // The time must be emitted before the changes at that time, and must not go backwards:
    // fstapi has no way to represent an out-of-order change and would drop it silently.
    if (!time_emitted_ || time_index != current_time_) {
        if (time_emitted_ && time_index < current_time_) {
            throw std::runtime_error("FST timestamps must be non-decreasing");
        }
        fstWriterEmitTimeChange(ctx_, time_index);
        current_time_ = time_index;
        time_emitted_ = true;
    }

    const std::string bits = to_bit_string(value, it->second.width);
    fstWriterEmitValueChange(ctx_, it->second.handle, bits.c_str());
    ++value_changes_;
}

void FSTWriter::write_values(uint64_t time_index,
                             const std::map<std::string, std::string>& values) {
    for (const auto& [name, value] : values) {
        write_value(name, time_index, value);
    }
}

void FSTWriter::begin_transaction() {
    if (in_transaction_) {
        throw std::runtime_error("Transaction already in progress");
    }
    in_transaction_ = true;
}

void FSTWriter::commit_transaction() {
    if (!in_transaction_) {
        throw std::runtime_error("No transaction in progress");
    }
    in_transaction_ = false;
}

void FSTWriter::rollback_transaction() {
    if (!in_transaction_) {
        throw std::runtime_error("No transaction in progress");
    }
    in_transaction_ = false;
}

void FSTWriter::finalize() {
    if (finalized_) {
        return;
    }
    // Declare the hierarchy even for an empty trace, so the result is still a valid FST
    // rather than a zero-length file.
    ensure_header_written();
    if (ctx_ != nullptr) {
        fstWriterClose(ctx_);
        ctx_ = nullptr;
    }
    finalized_ = true;
}

void FSTWriter::flush() {
    if (ctx_ != nullptr) {
        fstWriterFlushContext(ctx_);
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
    return declaration_order_;
}

std::map<std::string, int64_t> FSTWriter::get_statistics() const {
    return {
        {"signals", static_cast<int64_t>(signals_.size())},
        {"value_changes", value_changes_},
    };
}

double FSTWriter::get_compression_ratio() const {
    // fstapi does not report this; 0.0 means "unknown" rather than an invented figure.
    return 0.0;
}

int64_t FSTWriter::get_value_change_count() const {
    return value_changes_;
}

std::string FSTWriter::leaf_name(const std::string& full_name) {
    const auto dot = full_name.rfind('.');
    return dot == std::string::npos ? full_name : full_name.substr(dot + 1);
}

std::string FSTWriter::to_bit_string(const std::string& value, size_t width) {
    std::string bits;

    const bool is_bits =
        !value.empty() && value.find_first_not_of("01xXzZ") == std::string::npos;
    if (value.empty()) {
        return std::string(width, 'x');
    }
    if (is_bits) {
        bits = value;
    } else {
        uint64_t numeric = 0;
        try {
            const bool hex =
                value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X');
            numeric = std::stoull(value, nullptr, hex ? 16 : 10);
        } catch (const std::exception&) {
            // Unparseable: 'x' throughout beats writing the string verbatim, which would
            // decode as nonsense while looking like a successful write.
            return std::string(width, 'x');
        }
        for (size_t i = width; i-- > 0;) {
            bits.push_back(((numeric >> (i % 64)) & 1ULL) != 0ULL ? '1' : '0');
        }
        return bits;
    }

    // fstapi reads exactly `width` bytes for a vector, so pad short values on the left and
    // truncate over-long ones from the left (keeping the low bits).
    if (bits.size() < width) {
        const char c = bits[0];
        const char pad = (c == 'x' || c == 'X' || c == 'z' || c == 'Z') ? c : '0';
        bits.insert(bits.begin(), width - bits.size(), pad);
    } else if (bits.size() > width) {
        bits = bits.substr(bits.size() - width);
    }
    return bits;
}

} // namespace wal::writers
