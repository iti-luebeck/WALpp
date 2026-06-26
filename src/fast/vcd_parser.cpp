#include "wal/fast/vcd_parser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

extern "C" {
#include "fstapi.h"
}

namespace fast_vcd {

// ---------------------------------------------------------------------------
// FST fast-path: callback state and value-change function
// ---------------------------------------------------------------------------
namespace {

struct FSTFastState {
    std::unordered_map<fstHandle, uint32_t>* handle_to_sig_id;
    std::vector<DeltaTimestamp>*             deltas;
    uint64_t prev_time  = 0;
    bool     has_prev   = false;
};

static void fst_fast_change_cb(void* user_data,
                                uint64_t time,
                                fstHandle handle,
                                const unsigned char* value)
{
    auto* s = static_cast<FSTFastState*>(user_data);

    // Start a new delta when the timestamp advances
    if (!s->has_prev || time != s->prev_time) {
        s->deltas->emplace_back(static_cast<int64_t>(time));
        s->deltas->back().changes.reserve(64);
        s->has_prev  = true;
        s->prev_time = time;
    }

    // Parse binary value string: "0","1","x","z","0110..." → int64_t
    int64_t val = 0;
    for (const unsigned char* p = value; *p; ++p) {
        val <<= 1;
        if (*p == '1') val |= 1;
    }

    auto it = s->handle_to_sig_id->find(handle);
    if (it != s->handle_to_sig_id->end()) {
        s->deltas->back().changes.emplace_back(it->second, val);
    }
}

} // anonymous namespace

void VCDParser::parse(const std::string& filename, const std::string& scope_filter) {
    // Open and mmap file
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error("Cannot open VCD file: " + filename);
    }
    
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        throw std::runtime_error("Cannot stat VCD file");
    }
    
    size_t file_size = sb.st_size;
    if (file_size == 0) {
        close(fd);
        throw std::runtime_error("VCD file is empty");
    }
    
    const char* data = static_cast<const char*>(
        mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0)
    );
    
    if (data == MAP_FAILED) {
        close(fd);
        throw std::runtime_error("Cannot mmap VCD file");
    }
    
    const char* end = data + file_size;
    size_t pos = 0;
    
    // Parse header to build signal metadata
    parse_header(data, end, pos);
    
    // If scope filter is specified, filter signals
    if (!scope_filter.empty()) {
        filtered_vcd_ids_.clear();
        signal_ids_.clear();
        
        for (const auto& sig : signals_) {
            if (sig.scope.find(scope_filter) == 0 || 
                sig.name.find(scope_filter) == 0) {
                filtered_vcd_ids_.insert(sig.vcd_id);
                signal_ids_.push_back(sig.id);
            }
        }
    } else {
        // No filter: use all signals
        signal_ids_.reserve(signals_.size());
        for (const auto& sig : signals_) {
            signal_ids_.push_back(sig.id);
        }
    }
    
    // Reserve space for deltas (estimate ~64K timestamps)
    deltas_.reserve(65536);
    
    // Parse value changes with delta encoding
    parse_data(data, end, pos);
    
    // Cleanup
    munmap(const_cast<char*>(data), file_size);
    close(fd);
}

void VCDParser::parse_header(const char* data, const char* end, size_t& pos) {
    std::string current_scope;
    std::vector<std::string> scope_stack;
    
    // Reserve space for signals (typical VCD has 10K-100K signals)
    signals_.reserve(50000);
    vcd_id_to_signal_.reserve(50000);
    
    while (pos < static_cast<size_t>(end - data)) {
        // Skip whitespace
        while (pos < static_cast<size_t>(end - data) && 
               (data[pos] == ' ' || data[pos] == '\t' || 
                data[pos] == '\n' || data[pos] == '\r')) {
            pos++;
        }
        
        if (pos >= static_cast<size_t>(end - data)) break;
        
        // Check for end of definitions
        if (strncmp(data + pos, "$enddefinitions", 15) == 0) {
            // Skip to end of line
            while (pos < static_cast<size_t>(end - data) && data[pos] != '\n') pos++;
            pos++;
            break;
        }

        // Parse $timescale (value may be on the same line or the next), e.g.
        // "$timescale 1ps $end" — grab the magnitude+unit token(s) before $end.
        if (strncmp(data + pos, "$timescale", 10) == 0) {
            pos += 10;
            std::string ts;
            while (pos < static_cast<size_t>(end - data) &&
                   strncmp(data + pos, "$end", 4) != 0) {
                const char c = data[pos];
                if (c != ' ' && c != '\t' && c != '\n' && c != '\r') ts += c;
                pos++;
            }
            if (!ts.empty()) timescale_ = ts;
            continue;
        }

        // Parse $scope
        if (strncmp(data + pos, "$scope", 6) == 0) {
            pos += 6;
            
            // Skip whitespace
            while (pos < static_cast<size_t>(end - data) && 
                   (data[pos] == ' ' || data[pos] == '\t')) pos++;
            
            // Skip type (module/begin/etc)
            while (pos < static_cast<size_t>(end - data) && 
                   data[pos] != ' ' && data[pos] != '\t' && data[pos] != '\n') pos++;
            
            // Skip whitespace
            while (pos < static_cast<size_t>(end - data) && 
                   (data[pos] == ' ' || data[pos] == '\t')) pos++;
            
            // Extract scope name
            const char* name_start = data + pos;
            while (pos < static_cast<size_t>(end - data) && 
                   data[pos] != ' ' && data[pos] != '\t' && 
                   data[pos] != '\n' && data[pos] != '\r' && data[pos] != '$') {
                pos++;
            }
            
            std::string scope_name(name_start, data + pos - name_start);
            scope_stack.push_back(scope_name);
            
            // Build hierarchical scope
            current_scope.clear();
            for (size_t i = 0; i < scope_stack.size(); i++) {
                if (i > 0) current_scope += ".";
                current_scope += scope_stack[i];
            }
            
            // Skip to end of line
            while (pos < static_cast<size_t>(end - data) && data[pos] != '\n') pos++;
            pos++;
            continue;
        }
        
        // Parse $upscope
        if (strncmp(data + pos, "$upscope", 8) == 0) {
            if (!scope_stack.empty()) {
                scope_stack.pop_back();
                
                // Rebuild current scope
                current_scope.clear();
                for (size_t i = 0; i < scope_stack.size(); i++) {
                    if (i > 0) current_scope += ".";
                    current_scope += scope_stack[i];
                }
            }
            
            // Skip to end of line
            while (pos < static_cast<size_t>(end - data) && data[pos] != '\n') pos++;
            pos++;
            continue;
        }
        
        // Parse $var
        if (strncmp(data + pos, "$var", 4) == 0) {
            pos += 4;
            
            // Skip whitespace
            while (pos < static_cast<size_t>(end - data) && 
                   (data[pos] == ' ' || data[pos] == '\t')) pos++;
            
            // Skip type (wire/reg/etc)
            while (pos < static_cast<size_t>(end - data) && 
                   data[pos] != ' ' && data[pos] != '\t') pos++;
            
            // Skip whitespace
            while (pos < static_cast<size_t>(end - data) && 
                   (data[pos] == ' ' || data[pos] == '\t')) pos++;
            
            // Parse width
            int width = 0;
            while (pos < static_cast<size_t>(end - data) && 
                   data[pos] >= '0' && data[pos] <= '9') {
                width = width * 10 + (data[pos] - '0');
                pos++;
            }
            
            // Skip whitespace
            while (pos < static_cast<size_t>(end - data) && 
                   (data[pos] == ' ' || data[pos] == '\t')) pos++;
            
            // Parse VCD identifier
            const char* id_start = data + pos;
            while (pos < static_cast<size_t>(end - data) && 
                   data[pos] != ' ' && data[pos] != '\t') pos++;
            std::string vcd_id(id_start, data + pos - id_start);
            
            // Skip whitespace
            while (pos < static_cast<size_t>(end - data) && 
                   (data[pos] == ' ' || data[pos] == '\t')) pos++;
            
            // Parse signal name
            const char* name_start = data + pos;
            while (pos < static_cast<size_t>(end - data) && 
                   data[pos] != ' ' && data[pos] != '\t' && 
                   data[pos] != '\n' && data[pos] != '\r' && data[pos] != '$') {
                pos++;
            }
            std::string sig_name(name_start, data + pos - name_start);
            
            // Build full hierarchical name
            std::string full_name = current_scope.empty() ? 
                sig_name : current_scope + "." + sig_name;
            
            // Create signal info
            SignalInfo sig;
            sig.id = next_signal_id_++;
            sig.name = full_name;
            sig.vcd_id = vcd_id;
            sig.scope = current_scope;
            sig.width = width;
            
            signals_.push_back(sig);
            vcd_id_to_signal_[vcd_id] = sig.id;
            
            // Skip to end of line
            while (pos < static_cast<size_t>(end - data) && data[pos] != '\n') pos++;
            pos++;
            continue;
        }
        
        // Skip other commands
        if (data[pos] == '$') {
            while (pos < static_cast<size_t>(end - data) && data[pos] != '\n') pos++;
            pos++;
            continue;
        }
        
        // Skip line
        while (pos < static_cast<size_t>(end - data) && data[pos] != '\n') pos++;
        pos++;
    }
}

void VCDParser::parse_data(const char* data, const char* end, size_t& pos) {
    int64_t current_timestamp = 0;
    DeltaTimestamp* current_delta = nullptr;
    
    // Parse $dumpvars to capture initial signal values
    while (pos < static_cast<size_t>(end - data)) {
        // Skip whitespace
        while (pos < static_cast<size_t>(end - data) &&
               (data[pos] == ' ' || data[pos] == '\t' ||
                data[pos] == '\n' || data[pos] == '\r')) {
            pos++;
        }

        if (pos >= static_cast<size_t>(end - data)) break;

        if (strncmp(data + pos, "$dumpvars", 9) == 0) {
            pos += 9;
            // Parse value changes until $end
            while (pos < static_cast<size_t>(end - data)) {
                while (pos < static_cast<size_t>(end - data) &&
                       (data[pos] == ' ' || data[pos] == '\t' ||
                        data[pos] == '\n' || data[pos] == '\r')) {
                    pos++;
                }
                if (pos >= static_cast<size_t>(end - data)) break;
                if (strncmp(data + pos, "$end", 4) == 0) {
                    pos += 4;
                    break;
                }

                // Binary value: b<bits> <id>
                if (data[pos] == 'b') {
                    pos++;
                    int64_t value = 0;
                    while (pos < static_cast<size_t>(end - data) &&
                           data[pos] != ' ' && data[pos] != '\t') {
                        value = (value << 1) | (data[pos] == '1' ? 1 : 0);
                        pos++;
                    }
                    while (pos < static_cast<size_t>(end - data) &&
                           (data[pos] == ' ' || data[pos] == '\t')) pos++;
                    const char* id_start = data + pos;
                    while (pos < static_cast<size_t>(end - data) &&
                           data[pos] != ' ' && data[pos] != '\t' &&
                           data[pos] != '\n' && data[pos] != '\r') pos++;
                    std::string vcd_id(id_start, data + pos - id_start);
                    if (filtered_vcd_ids_.empty() || filtered_vcd_ids_.count(vcd_id)) {
                        auto it = vcd_id_to_signal_.find(vcd_id);
                        if (it != vcd_id_to_signal_.end())
                            initial_values_[it->second] = value;
                    }
                    continue;
                }

                // Single-bit value: <0|1|x|z><id>
                if (data[pos] == '0' || data[pos] == '1' ||
                    data[pos] == 'x' || data[pos] == 'z') {
                    int64_t value = (data[pos] == '1') ? 1 : 0;
                    pos++;
                    const char* id_start = data + pos;
                    while (pos < static_cast<size_t>(end - data) &&
                           data[pos] != ' ' && data[pos] != '\t' &&
                           data[pos] != '\n' && data[pos] != '\r') pos++;
                    std::string vcd_id(id_start, data + pos - id_start);
                    if (filtered_vcd_ids_.empty() || filtered_vcd_ids_.count(vcd_id)) {
                        auto it = vcd_id_to_signal_.find(vcd_id);
                        if (it != vcd_id_to_signal_.end())
                            initial_values_[it->second] = value;
                    }
                    continue;
                }

                // Skip unknown line
                while (pos < static_cast<size_t>(end - data) && data[pos] != '\n') pos++;
                pos++;
            }
            continue;
        }

        if (data[pos] == '#') {
            break;  // Start of timestamps
        }

        pos++;
    }
    
    // Parse value changes
    while (pos < static_cast<size_t>(end - data)) {
        // Skip whitespace
        while (pos < static_cast<size_t>(end - data) && 
               (data[pos] == ' ' || data[pos] == '\t' || 
                data[pos] == '\n' || data[pos] == '\r')) {
            pos++;
        }
        
        if (pos >= static_cast<size_t>(end - data)) break;
        
        // Timestamp marker
        if (data[pos] == '#') {
            pos++;
            
            // Parse timestamp (fast integer parsing)
            int64_t ts = 0;
            while (pos < static_cast<size_t>(end - data) && 
                   data[pos] >= '0' && data[pos] <= '9') {
                ts = ts * 10 + (data[pos] - '0');
                pos++;
            }
            
            current_timestamp = ts;
            deltas_.emplace_back(current_timestamp);
            current_delta = &deltas_.back();
            current_delta->changes.reserve(100);  // Estimate ~100 changes per timestamp
            
            continue;
        }
        
        if (!current_delta) {
            // No timestamp yet, skip
            while (pos < static_cast<size_t>(end - data) && data[pos] != '\n') pos++;
            pos++;
            continue;
        }
        
        // Binary value change: b<bits> <id>
        if (data[pos] == 'b') {
            pos++;
            
            // Parse binary value
            int64_t value = 0;
            while (pos < static_cast<size_t>(end - data) && 
                   data[pos] != ' ' && data[pos] != '\t') {
                if (data[pos] == '1') {
                    value = (value << 1) | 1;
                } else {
                    value = value << 1;
                }
                pos++;
            }
            
            // Skip whitespace
            while (pos < static_cast<size_t>(end - data) && 
                   (data[pos] == ' ' || data[pos] == '\t')) pos++;
            
            // Parse VCD ID
            const char* id_start = data + pos;
            while (pos < static_cast<size_t>(end - data) && 
                   data[pos] != ' ' && data[pos] != '\t' && 
                   data[pos] != '\n' && data[pos] != '\r') {
                pos++;
            }
            
            std::string vcd_id(id_start, data + pos - id_start);
            
            // Check if we're tracking this signal
            if (filtered_vcd_ids_.empty() || filtered_vcd_ids_.count(vcd_id)) {
                auto it = vcd_id_to_signal_.find(vcd_id);
                if (it != vcd_id_to_signal_.end()) {
                    current_delta->changes.emplace_back(it->second, value);
                }
            }
            
            continue;
        }
        
        // Single bit change: <bit><id>
        if (data[pos] == '0' || data[pos] == '1' || 
            data[pos] == 'x' || data[pos] == 'z') {
            int64_t value = (data[pos] == '1') ? 1 : 0;
            pos++;
            
            // Parse VCD ID
            const char* id_start = data + pos;
            while (pos < static_cast<size_t>(end - data) && 
                   data[pos] != ' ' && data[pos] != '\t' && 
                   data[pos] != '\n' && data[pos] != '\r') {
                pos++;
            }
            
            std::string vcd_id(id_start, data + pos - id_start);
            
            // Check if we're tracking this signal
            if (filtered_vcd_ids_.empty() || filtered_vcd_ids_.count(vcd_id)) {
                auto it = vcd_id_to_signal_.find(vcd_id);
                if (it != vcd_id_to_signal_.end()) {
                    current_delta->changes.emplace_back(it->second, value);
                }
            }
            
            continue;
        }
        
        // Skip unknown line
        while (pos < static_cast<size_t>(end - data) && data[pos] != '\n') pos++;
        pos++;
    }
}

std::vector<uint32_t> VCDParser::get_signals_in_scope(const std::string& scope_prefix) const {
    std::vector<uint32_t> result;
    result.reserve(signals_.size() / 10);  // Estimate
    
    for (const auto& sig : signals_) {
        if (sig.scope.find(scope_prefix) == 0 || 
            sig.name.find(scope_prefix) == 0) {
            result.push_back(sig.id);
        }
    }
    
    return result;
}

std::vector<uint32_t> VCDParser::get_signal_ids_by_names(const std::vector<std::string>& signal_names) const {
    std::vector<uint32_t> result;
    result.reserve(signal_names.size());
    
    // Build name -> VCD id map (keep every alias name; aliases share a vcd_id).
    std::unordered_map<std::string, std::string> name_to_vcdid;
    name_to_vcdid.reserve(signals_.size());
    for (const auto& sig : signals_) {
        name_to_vcdid[sig.name] = sig.vcd_id;
    }

    // Resolve each name to the CANONICAL signal id that carries the value
    // changes for its vcd_id. A VCD aliases connected nets to one vcd_id, but
    // the deltas only reference one signal id per vcd_id (vcd_id_to_signal_).
    // Returning an alias' own signal id instead yields a constant series — the
    // bug that made --fast-*-signals report all-zero for an aliased port.
    for (const auto& name : signal_names) {
        auto nit = name_to_vcdid.find(name);
        if (nit == name_to_vcdid.end()) continue;
        auto cit = vcd_id_to_signal_.find(nit->second);
        if (cit != vcd_id_to_signal_.end()) {
            result.push_back(cit->second);
        }
    }

    return result;
}

void VCDParser::parse_fst(const std::string& filename, const std::string& scope_filter) {
    void* ctx = fstReaderOpen(filename.c_str());
    if (!ctx) {
        throw std::runtime_error("Cannot open FST file: " + filename);
    }

    try {
        // ----------------------------------------------------------------
        // 1. Walk the signal hierarchy
        // ----------------------------------------------------------------
        std::unordered_map<fstHandle, uint32_t> handle_to_sig_id;
        handle_to_sig_id.reserve(50000);
        signals_.reserve(50000);

        std::string current_scope;
        std::vector<std::string> scope_stack;

        fstReaderIterateHierRewind(ctx);
        struct fstHier* hier;
        while ((hier = fstReaderIterateHier(ctx)) != nullptr) {
            switch (hier->htyp) {
                case FST_HT_SCOPE:
                    scope_stack.push_back(hier->u.scope.name);
                    current_scope.clear();
                    for (size_t i = 0; i < scope_stack.size(); ++i) {
                        if (i > 0) current_scope += '.';
                        current_scope += scope_stack[i];
                    }
                    break;

                case FST_HT_UPSCOPE:
                    if (!scope_stack.empty()) scope_stack.pop_back();
                    current_scope.clear();
                    for (size_t i = 0; i < scope_stack.size(); ++i) {
                        if (i > 0) current_scope += '.';
                        current_scope += scope_stack[i];
                    }
                    break;

                case FST_HT_VAR: {
                    if (hier->u.var.is_alias) break;

                    std::string var_name  = hier->u.var.name;
                    std::string full_name = current_scope.empty()
                        ? var_name
                        : current_scope + '.' + var_name;

                    // Apply scope filter if set
                    if (!scope_filter.empty()) {
                        if (current_scope.find(scope_filter) != 0 &&
                            full_name.find(scope_filter) != 0) {
                            break;
                        }
                    }

                    SignalInfo sig;
                    sig.id     = next_signal_id_++;
                    sig.name   = full_name;
                    sig.vcd_id = "";  // FST uses numeric handles, not text IDs
                    sig.scope  = current_scope;
                    sig.width  = static_cast<int>(hier->u.var.length);

                    handle_to_sig_id[hier->u.var.handle] = sig.id;
                    signals_.push_back(sig);
                    signal_ids_.push_back(sig.id);
                    break;
                }

                default:
                    break;
            }
        }

        // ----------------------------------------------------------------
        // 2. Collect value-change events into delta timestamps
        // ----------------------------------------------------------------
        deltas_.reserve(65536);

        FSTFastState cb_state;
        cb_state.handle_to_sig_id = &handle_to_sig_id;
        cb_state.deltas           = &deltas_;

        fstReaderSetFacProcessMaskAll(ctx);
        fstReaderIterBlocks(ctx, fst_fast_change_cb, &cb_state, nullptr);

    } catch (...) {
        fstReaderClose(ctx);
        throw;
    }

    fstReaderClose(ctx);
}

} // namespace fast_vcd
