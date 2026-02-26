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

namespace fast_vcd {

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
    
    // Skip $dumpvars if present
    while (pos < static_cast<size_t>(end - data)) {
        // Skip whitespace
        while (pos < static_cast<size_t>(end - data) && 
               (data[pos] == ' ' || data[pos] == '\t' || 
                data[pos] == '\n' || data[pos] == '\r')) {
            pos++;
        }
        
        if (pos >= static_cast<size_t>(end - data)) break;
        
        if (strncmp(data + pos, "$dumpvars", 9) == 0) {
            // Skip until $end
            while (pos < static_cast<size_t>(end - data)) {
                if (strncmp(data + pos, "$end", 4) == 0) {
                    while (pos < static_cast<size_t>(end - data) && data[pos] != '\n') pos++;
                    pos++;
                    break;
                }
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
    
    // Build name->ID map for efficient lookup
    std::unordered_map<std::string, uint32_t> name_to_id;
    name_to_id.reserve(signals_.size());
    for (const auto& sig : signals_) {
        name_to_id[sig.name] = sig.id;
    }
    
    // Find signal IDs (skip signals not found)
    for (const auto& name : signal_names) {
        auto it = name_to_id.find(name);
        if (it != name_to_id.end()) {
            result.push_back(it->second);
        }
    }
    
    return result;
}

} // namespace fast_vcd
