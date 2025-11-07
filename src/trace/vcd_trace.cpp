#include "wal/trace/vcd_trace.h"
#include "wal/trace/trace_container.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace wal {

std::vector<std::string> VCDTrace::read_file_lines(const std::string& filename) {
    std::vector<std::string> lines;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open VCD file: " + filename);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Remove trailing whitespace and carriage returns
        while (!line.empty() && (line.back() == '\r' || std::isspace(line.back()))) {
            line.pop_back();
        }
        lines.push_back(line);
    }
    
    file.close();
    return lines;
}

Value VCDTrace::parse_bit_value(const std::string& bits) {
    if (bits.empty()) {
        return Value(0);
    }
    
    if (bits.length() == 1) {
        // Single bit: 0, 1, x, or z
        char c = bits[0];
        if (c == '0') return Value(0);
        if (c == '1') return Value(1);
        // For 'x' and 'z', return 0 for now
        return Value(0);
    }
    
    // Multi-bit value: parse as binary
    int64_t value = 0;
    for (size_t i = 0; i < bits.length(); ++i) {
        char c = bits[i];
        if (c == '0' || c == '1') {
            value = (value << 1) | (c == '1' ? 1 : 0);
        } else {
            // 'x' or 'z' bits - treat as 0
            value = (value << 1);
        }
    }
    
    return Value(value);
}

void VCDTrace::parse_header(const std::vector<std::string>& lines, size_t& line_idx) {
    std::string current_scope;
    
    while (line_idx < lines.size()) {
        std::string line = lines[line_idx];
        
        // Skip empty lines
        if (line.empty()) {
            line_idx++;
            continue;
        }
        
        // Check for section end
        if (line == "$enddefinitions") {
            line_idx++;
            break;
        }
        
        // Parse different VCD header commands
        if (line.substr(0, 6) == "$scope") {
            // $scope module scope_name
            std::istringstream iss(line);
            std::string cmd, type, scope_name;
            iss >> cmd >> type >> scope_name;
            current_scope = scope_name;
            scope_names.push_back(scope_name);
        }
        else if (line.substr(0, 6) == "$upscope") {
            current_scope = "";
        }
        else if (line.substr(0, 4) == "$var") {
            // $var wire|reg|parameter|real 1|N identifier reference
            std::istringstream iss(line);
            std::string cmd, var_type;
            int width;
            std::string identifier, reference;
            
            iss >> cmd >> var_type >> width >> identifier >> reference;
            
            SignalInfo info;
            info.name = reference;
            info.identifier = identifier;
            info.width = width;
            info.scope = current_scope;
            
            std::string full_name = reference;
            if (!current_scope.empty()) {
                full_name = current_scope + Trace::SCOPE_SEPARATOR + reference;
            }
            
            signal_info[full_name] = info;
            signal_id_map[identifier] = signal_info.size() - 1;
            signal_names.push_back(full_name);
            
            // Initialize current value to 0
            current_values[identifier] = Value(0);
        }
        else if (line.substr(0, 8) == "$timescale") {
            // $timescale 1ps
            // Ignore for now
        }
        else if (line.substr(0, 5) == "$date") {
            // $date ... $end
            // Skip until $end
            while (line_idx < lines.size() && line != "$end") {
                line_idx++;
                line = lines[line_idx];
            }
        }
        else if (line.substr(0, 8) == "$version") {
            // $version ... $end
            // Skip until $end
            while (line_idx < lines.size() && line != "$end") {
                line_idx++;
                line = lines[line_idx];
            }
        }
        
        line_idx++;
    }
}

void VCDTrace::parse_value_change(const std::string& line, int& timestamp) {
    if (line.empty()) return;
    
    // Value change format:
    // b<value> <identifier>   - multi-bit change
    // <bit> <identifier>      - single bit change (0, 1, x, z)
    // #<timestamp>            - timestamp marker
    
    if (line[0] == '#') {
        // Timestamp
        try {
            timestamp = std::stoi(line.substr(1));
        } catch (...) {
            timestamp = 0;
        }
    } else if (line[0] == 'b') {
        // Binary value change
        size_t space_pos = line.find(' ');
        if (space_pos != std::string::npos) {
            std::string bit_value = line.substr(1, space_pos - 1);
            std::string identifier = line.substr(space_pos + 1);
            
            current_values[identifier] = parse_bit_value(bit_value);
        }
    } else if (line[0] == '0' || line[0] == '1' || line[0] == 'x' || line[0] == 'z') {
        // Single bit change
        char bit = line[0];
        std::string identifier = line.substr(1);
        
        Value val;
        if (bit == '0') val = Value(0);
        else if (bit == '1') val = Value(1);
        else val = Value(0);  // 'x' or 'z'
        
        current_values[identifier] = val;
    }
}

void VCDTrace::parse_data(const std::vector<std::string>& lines, size_t& line_idx) {
    int current_timestamp = 0;
    int timestamp_count = 0;
    
    while (line_idx < lines.size()) {
        std::string line = lines[line_idx];
        
        if (line.empty()) {
            line_idx++;
            continue;
        }
        
        if (line[0] == '#') {
            // New timestamp - save current values
            if (timestamp_count > 0) {
                TimeStep ts;
                ts.timestamp = current_timestamp;
                ts.signal_values = current_values;
                time_steps.push_back(ts);
                timestamp_index_map[current_timestamp] = timestamp_count - 1;
            }
            
            parse_value_change(line, current_timestamp);
            timestamp_count++;
        } else {
            parse_value_change(line, current_timestamp);
        }
        
        line_idx++;
    }
    
    // Save final timestamp
    if (timestamp_count > 0) {
        TimeStep ts;
        ts.timestamp = current_timestamp;
        ts.signal_values = current_values;
        time_steps.push_back(ts);
        timestamp_index_map[current_timestamp] = timestamp_count - 1;
    }
    
    max_index = static_cast<int>(time_steps.size()) - 1;
}

void VCDTrace::load() {
    try {
        auto lines = read_file_lines(filename);
        
        if (lines.empty()) {
            throw std::runtime_error("VCD file is empty");
        }
        
        size_t line_idx = 0;
        
        // Parse header
        parse_header(lines, line_idx);
        
        // Find $dumpvars or data start
        int current_timestamp = 0;
        while (line_idx < lines.size()) {
            if (lines[line_idx] == "$dumpvars") {
                line_idx++;
                break;
            }
            line_idx++;
        }
        
        // Parse initial values
        while (line_idx < lines.size() && lines[line_idx] != "$end") {
            parse_value_change(lines[line_idx], current_timestamp);
            line_idx++;
        }
        if (line_idx < lines.size()) line_idx++;  // skip $end
        
        // Parse value changes
        parse_data(lines, line_idx);
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load VCD file: " + std::string(e.what()));
    }
}

Value VCDTrace::signal_value(const std::string& name, int offset,
                            const std::string& scope) {
    std::string full_name = name;
    if (!scope.empty()) {
        full_name = scope + Trace::SCOPE_SEPARATOR + name;
    }
    
    // Check if signal exists
    if (signal_info.find(full_name) == signal_info.end()) {
        throw std::runtime_error("Signal not found: " + full_name);
    }
    
    int target_index = current_index + offset;
    if (target_index < 0 || target_index > max_index) {
        throw std::runtime_error("Index out of range");
    }
    
    if (target_index < static_cast<int>(time_steps.size())) {
        auto& ts = time_steps[target_index];
        auto& signal_info_ref = signal_info[full_name];
        
        auto it = ts.signal_values.find(signal_info_ref.identifier);
        if (it != ts.signal_values.end()) {
            return it->second;
        }
    }
    
    return Value(0);
}

int VCDTrace::signal_width(const std::string& name) {
    // Try exact match first
    auto it = signal_info.find(name);
    if (it != signal_info.end()) {
        return it->second.width;
    }
    
    // Try to find by base name (without scope prefix)
    for (const auto& [full_name, info] : signal_info) {
        if (info.name == name) {
            return info.width;
        }
    }
    
    throw std::runtime_error("Signal not found: " + name);
}

bool VCDTrace::contains_signal(const std::string& name) const {
    // Try exact match first
    if (signal_info.find(name) != signal_info.end()) {
        return true;
    }
    
    // Try to find by base name (without scope prefix)
    for (const auto& [full_name, info] : signal_info) {
        if (info.name == name) {
            return true;
        }
    }
    
    return false;
}

std::vector<std::string> VCDTrace::get_all_signals() const {
    return signal_names;
}

} // namespace wal
