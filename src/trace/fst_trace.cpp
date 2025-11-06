#include "wal/trace/fst_trace.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace wal {

std::vector<uint8_t> FSTTrace::read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Read file into memory
    std::vector<uint8_t> data(file_size);
    file.read(reinterpret_cast<char*>(data.data()), file_size);
    file.close();
    
    return data;
}

uint64_t FSTTrace::read_vli(const std::vector<uint8_t>& data, size_t& pos) {
    uint64_t value = 0;
    int shift = 0;
    
    while (pos < data.size()) {
        uint8_t byte = data[pos++];
        value |= (static_cast<uint64_t>(byte & 0x7F) << shift);
        
        if ((byte & 0x80) == 0) {
            break;
        }
        shift += 7;
    }
    
    return value;
}

std::string FSTTrace::read_string(const std::vector<uint8_t>& data, size_t& pos) {
    uint64_t len = read_vli(data, pos);
    
    if (pos + len > data.size()) {
        throw std::runtime_error("Invalid string length in FST file");
    }
    
    std::string result(reinterpret_cast<const char*>(data.data() + pos), len);
    pos += len;
    
    return result;
}

std::vector<uint8_t> FSTTrace::decompress_data(const std::vector<uint8_t>& compressed) {
    // FST uses compression, but for this simplified implementation,
    // we'll handle uncompressed FST files or simple RLE-like compression
    // Full FST support would use zlib or similar
    
    // For now, just return the data as-is
    // A full implementation would decompress here
    return compressed;
}

char FSTTrace::parse_bit_value(uint8_t byte) {
    switch (byte) {
        case 0: return '0';
        case 1: return '1';
        case 2: return 'x';
        case 3: return 'z';
        default: return '0';
    }
}

Value FSTTrace::bits_to_value(const std::string& bits) {
    int64_t value = 0;
    
    for (size_t i = 0; i < bits.length(); ++i) {
        char c = bits[i];
        if (c == '0' || c == '1') {
            value = (value << 1) | (c == '1' ? 1 : 0);
        } else {
            // For x or z, treat as 0
            value = (value << 1);
        }
    }
    
    return Value(value);
}

int FSTTrace::infer_width(int64_t max_value) {
    if (max_value == 0) return 1;
    if (max_value < 0) return 64;
    
    int width = 0;
    int64_t val = max_value;
    while (val > 0) {
        width++;
        val >>= 1;
    }
    return width + 1;
}

void FSTTrace::parse_header(const std::vector<uint8_t>& data, size_t& pos) {
    if (pos + 20 > data.size()) {
        throw std::runtime_error("FST file too small for header");
    }
    
    // FST magic number
    if (data[pos] != 0xFF || data[pos + 1] != 0x00 || 
        data[pos + 2] != 0xFF || data[pos + 3] != 0x00) {
        throw std::runtime_error("Invalid FST file magic number");
    }
    pos += 4;
    
    // Version
    fst_header.version = data[pos++];
    
    // Timescale (power of 10)
    fst_header.timescale_power = data[pos++];
    fst_header.timescale_unit = data[pos++];
    
    // Start and end time
    fst_header.start_time = static_cast<int64_t>(read_vli(data, pos));
    fst_header.end_time = static_cast<int64_t>(read_vli(data, pos));
}

void FSTTrace::parse_hierarchy(const std::vector<uint8_t>& data, size_t& pos) {
    std::string current_scope;
    int var_count = 0;
    
    // Parse hierarchy section
    while (pos < data.size()) {
        uint8_t section_type = data[pos++];
        
        if (section_type == 0x00) {  // End of hierarchy
            break;
        }
        
        // Section types:
        // 1 = scope start
        // 2 = scope end
        // 3 = variable
        
        if (section_type == 1) {
            // Scope type: wire, reg, parameter, etc.
            char scope_type = data[pos++];
            std::string scope_name = read_string(data, pos);
            
            if (!current_scope.empty()) {
                current_scope += Trace::SCOPE_SEPARATOR;
            }
            current_scope += scope_name;
            scope_names.push_back(current_scope);
        }
        else if (section_type == 2) {
            // End scope
            size_t sep_pos = current_scope.rfind(Trace::SCOPE_SEPARATOR);
            if (sep_pos != std::string::npos) {
                current_scope = current_scope.substr(0, sep_pos);
            } else {
                current_scope = "";
            }
        }
        else if (section_type == 3) {
            // Variable definition
            char var_type = data[pos++];
            int width = read_vli(data, pos);
            std::string var_name = read_string(data, pos);
            int wire_id = read_vli(data, pos);
            
            std::string full_name = var_name;
            if (!current_scope.empty()) {
                full_name = current_scope + Trace::SCOPE_SEPARATOR + var_name;
            }
            
            SignalInfo info;
            info.name = var_name;
            info.wire_id = wire_id;
            info.width = width;
            info.scope = current_scope;
            info.kind = var_type;
            
            signal_info[full_name] = info;
            wire_id_to_signal[wire_id] = full_name;
            signal_names.push_back(full_name);
            current_values[full_name] = Value(0);
            
            var_count++;
        }
    }
}

void FSTTrace::parse_value_changes(const std::vector<uint8_t>& data, size_t& pos) {
    int64_t current_timestamp = fst_header.start_time;
    int timestamp_count = 0;
    
    // Parse value change data
    std::unordered_map<std::string, int64_t> max_values;
    for (const auto& sig_name : signal_names) {
        max_values[sig_name] = 0;
    }
    
    while (pos < data.size()) {
        uint8_t change_type = data[pos++];
        
        if (change_type == 0x00) {  // End of changes
            break;
        }
        
        if (change_type == 0xFF) {  // Timestamp change
            current_timestamp = static_cast<int64_t>(read_vli(data, pos));
            
            // Save previous timestep if we have value changes
            if (timestamp_count > 0 && !signal_info.empty()) {
                // Create timestep with current values
                TimeStep ts;
                ts.timestamp = current_timestamp - 1;  // Previous timestamp
                
                for (const auto& [sig_name, _] : signal_info) {
                    auto it = current_values.find(sig_name);
                    if (it != current_values.end()) {
                        ts.signal_values[sig_name] = it->second;
                    }
                }
                
                time_steps.push_back(ts);
                timestamp_index_map[ts.timestamp] = timestamp_count - 1;
            }
        }
        else if (change_type >= 0x01 && change_type <= 0x04) {
            // Single bit value change
            // Format: 1 byte type + wire ID (VLI) + bit value
            int wire_id = read_vli(data, pos);
            char bit_value = (change_type == 0x01) ? '0' : '1';
            
            auto it = wire_id_to_signal.find(wire_id);
            if (it != wire_id_to_signal.end()) {
                std::string sig_name = it->second;
                current_values[sig_name] = Value(bit_value == '1' ? 1 : 0);
            }
        }
        else if (change_type == 0x10) {  // Vector value change
            // Format: 0x10 + wire ID (VLI) + bit pattern
            int wire_id = read_vli(data, pos);
            std::string bits = read_string(data, pos);
            
            auto it = wire_id_to_signal.find(wire_id);
            if (it != wire_id_to_signal.end()) {
                std::string sig_name = it->second;
                Value val = bits_to_value(bits);
                current_values[sig_name] = val;
                
                if (val.is_int()) {
                    int64_t iv = val.to_int();
                    if (iv > max_values[sig_name]) {
                        max_values[sig_name] = iv;
                    }
                }
            }
        }
        
        timestamp_count++;
    }
    
    // Save final timestep
    if (timestamp_count > 0) {
        TimeStep ts;
        ts.timestamp = current_timestamp;
        
        for (const auto& [sig_name, _] : signal_info) {
            auto it = current_values.find(sig_name);
            if (it != current_values.end()) {
                ts.signal_values[sig_name] = it->second;
            }
        }
        
        time_steps.push_back(ts);
        timestamp_index_map[ts.timestamp] = timestamp_count - 1;
    }
    
    // Update signal widths based on observed values
    for (const auto& [sig_name, max_val] : max_values) {
        if (signal_info.find(sig_name) != signal_info.end()) {
            signal_info[sig_name].width = infer_width(max_val);
        }
    }
    
    max_index = static_cast<int>(time_steps.size()) - 1;
}

void FSTTrace::load() {
    try {
        auto data = read_file(filename);
        
        if (data.empty()) {
            throw std::runtime_error("FST file is empty");
        }
        
        size_t pos = 0;
        
        // Parse header
        parse_header(data, pos);
        
        // Parse hierarchy
        parse_hierarchy(data, pos);
        
        // Parse value changes
        parse_value_changes(data, pos);
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load FST file: " + std::string(e.what()));
    }
}

Value FSTTrace::signal_value(const std::string& name, int offset,
                            const std::string& scope) {
    std::string full_name = name;
    if (!scope.empty()) {
        full_name = scope + Trace::SCOPE_SEPARATOR + name;
    }
    
    // Check if signal exists
    if (signal_info.find(full_name) == signal_info.end() &&
        signal_info.find(name) == signal_info.end()) {
        throw std::runtime_error("Signal not found: " + full_name);
    }
    
    std::string lookup_name = (signal_info.find(full_name) != signal_info.end()) ? full_name : name;
    
    int target_index = current_index + offset;
    if (target_index < 0 || target_index > max_index) {
        throw std::runtime_error("Index out of range");
    }
    
    if (target_index < static_cast<int>(time_steps.size())) {
        auto& ts = time_steps[target_index];
        auto it = ts.signal_values.find(lookup_name);
        if (it != ts.signal_values.end()) {
            return it->second;
        }
    }
    
    // Return last known value if not found at this timestep
    auto it = current_values.find(lookup_name);
    if (it != current_values.end()) {
        return it->second;
    }
    
    return Value(0);
}

int FSTTrace::signal_width(const std::string& name) {
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

bool FSTTrace::contains_signal(const std::string& name) const {
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

std::vector<std::string> FSTTrace::get_all_signals() const {
    return signal_names;
}

} // namespace wal
