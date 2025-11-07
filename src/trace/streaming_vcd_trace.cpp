#include "wal/trace/streaming_vcd_trace.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace wal {

void StreamingVCDTrace::enter_scope(const std::string& scope_name) {
    current_scope_stack.push_back(scope_name);
}

void StreamingVCDTrace::exit_scope() {
    if (!current_scope_stack.empty()) {
        current_scope_stack.pop_back();
    }
}

std::string StreamingVCDTrace::get_full_signal_path(const std::string& signal_name) const {
    if (current_scope_stack.empty()) {
        return signal_name;
    }
    
    std::string path;
    for (const auto& scope : current_scope_stack) {
        if (!path.empty()) path += "^";
        path += scope;
    }
    path += "^";
    path += signal_name;
    return path;
}

void StreamingVCDTrace::validate_vcd_format(const std::string& line) {
    // Check for valid VCD line formats
    if (line.empty()) return;
    
    if (line[0] == '$') {
        // VCD keywords must be recognized
        static const std::vector<std::string> valid_keywords = {
            "$scope", "$var", "$upscope", "$enddefinitions", 
            "$end", "$date", "$version", "$timescale", "$comment"
        };
        
        bool valid = false;
        for (const auto& keyword : valid_keywords) {
            if (line.find(keyword) == 0) {
                valid = true;
                break;
            }
        }
        
        if (!valid && line.find('$') == 0) {
            // Unknown keyword - log but don't fail
            // This allows for forward compatibility with extended VCD formats
        }
    }
}

void StreamingVCDTrace::load() {
    // Initialize chunk reader
    chunk_reader = std::make_unique<StreamingChunkReader>(
        create_chunk_reader(filename, chunk_size_bytes)
    );
    
    // Phase 1: Parse header and build initial index
    parse_header_from_chunks();
    
    // Phase 2: Parse data and build sparse timestamp index
    parse_data_from_chunks();
}

bool StreamingVCDTrace::parse_header_from_chunks() {
    std::string buffer;
    
    // Collect chunks until we find the end of header ($enddefinitions)
    while (chunk_reader && chunk_reader->has_chunk()) {
        const auto& chunk = chunk_reader->get_chunk();
        
        if (!chunk.data.empty()) {
            buffer.append(chunk.data.begin(), chunk.data.end());
        }
        
        // Check if we've reached end of header
        if (buffer.find("$enddefinitions") != std::string::npos) {
            break;
        }
        
        // Check if this is final chunk and we haven't found header end
        if (chunk.is_final_chunk) {
            break;
        }
        
        chunk_reader->next();
    }
    
    // Parse header line by line
    std::istringstream iss(buffer);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (line.empty()) continue;
        
        // Validate VCD format
        validate_vcd_format(line);
        
        if (line == "$enddefinitions $end") {
            return true;
        } else if (line.find("$scope") == 0) {
            // Parse scope: $scope <type> <name> $end
            std::istringstream scope_stream(line);
            std::string scope_key, scope_type, scope_name;
            scope_stream >> scope_key >> scope_type >> scope_name;
            
            enter_scope(scope_name);
        } else if (line == "$upscope $end") {
            // Exit current scope
            exit_scope();
        } else if (line.find("$var") == 0) {
            // Parse variable: $var <type> <width> <id> <name> [bit] $end
            std::istringstream var_stream(line);
            std::string var_key, type, width_str, id, name;
            
            var_stream >> var_key >> type >> width_str >> id >> name;
            
            try {
                SignalInfo info;
                info.name = name;
                info.identifier = id;
                info.width = std::stoi(width_str);
                info.scope_hierarchy = current_scope_stack;
                
                // Build full hierarchical name
                std::string full_name = get_full_signal_path(name);
                signal_info[full_name] = info;
                identifier_map[id] = 0;  // placeholder for index
            } catch (const std::exception& e) {
                // Log parse error but continue
                continue;
            }
        }
    }
    
    return false;
}

bool StreamingVCDTrace::parse_data_from_chunks() {
    int64_t current_timestamp = 0;
    std::string buffer;
    int chunk_count = 0;
    
    // Continue consuming chunks from where we left off
    while (chunk_reader && chunk_reader->has_chunk()) {
        const auto& chunk = chunk_reader->get_chunk();
        
        if (!chunk.data.empty()) {
            buffer.append(chunk.data.begin(), chunk.data.end());
        }
        
        chunk_count++;
        
        // Parse buffer in lines, but keep incomplete line in buffer
        size_t last_newline = buffer.rfind('\n');
        std::string to_parse;
        
        if (last_newline != std::string::npos) {
            to_parse = buffer.substr(0, last_newline + 1);
            buffer = buffer.substr(last_newline + 1);
        } else if (chunk.is_final_chunk) {
            // Final chunk - parse everything
            to_parse = buffer;
            buffer.clear();
        } else {
            // Incomplete line, keep for next chunk
            if (chunk.is_final_chunk) {
                break;
            }
            chunk_reader->next();
            continue;
        }
        
        // Parse buffer in lines
        std::istringstream iss(to_parse);
        std::string line;
        
        while (std::getline(iss, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            if (line.empty()) continue;
            
            try {
                parse_vcd_line(line, current_timestamp);
            } catch (const std::exception& e) {
                // Skip malformed lines
                continue;
            }
        }
        
        if (chunk.is_final_chunk) {
            break;
        }
        
        chunk_reader->next();
    }
    
    return true;
}

void StreamingVCDTrace::parse_vcd_line(const std::string& line, int64_t& current_timestamp) {
    if (line.empty()) return;
    
    if (line[0] == '#') {
        // Timestamp line
        try {
            current_timestamp = std::stoll(line.substr(1));
            index.add_entry(current_timestamp, 0);  // 0 = approximate offset
        } catch (...) {
            // Invalid timestamp, skip
        }
    } else if (line[0] == 'b' || line[0] == 'B') {
        // Binary value change
        // Format: b<value> <identifier>
        try {
            size_t space_pos = line.find(' ');
            if (space_pos != std::string::npos && space_pos > 1) {
                std::string value_str = line.substr(1, space_pos - 1);
                std::string id = line.substr(space_pos + 1);
                
                if (identifier_map.find(id) != identifier_map.end()) {
                    // Find signal by identifier
                    for (auto& [name, info] : signal_info) {
                        if (info.identifier == id) {
                            current_values[name] = Value(value_str);
                            value_cache.add(name, static_cast<int>(current_timestamp), current_values[name]);
                            break;
                        }
                    }
                }
            }
        } catch (...) {
            // Malformed line, skip
        }
    } else if (std::isdigit(line[0]) || line[0] == 'x' || line[0] == 'X' ||
               line[0] == 'z' || line[0] == 'Z') {
        // Single-bit value change
        // Format: <value><identifier>
        try {
            if (line.length() > 1) {
                std::string value_str(1, line[0]);
                std::string id = line.substr(1);
                
                if (identifier_map.find(id) != identifier_map.end()) {
                    for (auto& [name, info] : signal_info) {
                        if (info.identifier == id) {
                            current_values[name] = Value(value_str);
                            value_cache.add(name, static_cast<int>(current_timestamp), current_values[name]);
                            break;
                        }
                    }
                }
            }
        } catch (...) {
            // Malformed line, skip
        }
    }
}

Value StreamingVCDTrace::signal_value(const std::string& name, int offset,
                                     const std::string& scope) {
    // Try exact match first (with scope if provided)
    std::string lookup_name = name;
    if (!scope.empty()) {
        lookup_name = scope + "^" + name;
    }
    
    // Check cache first
    if (auto cached = value_cache.get(lookup_name, offset)) {
        return *cached;
    }
    
    // Try without scope (flatten search)
    for (const auto& [signal_name, info] : signal_info) {
        if (signal_name.find(name) != std::string::npos) {
            if (current_values.find(signal_name) != current_values.end()) {
                auto val = current_values[signal_name];
                value_cache.add(lookup_name, offset, val);
                return val;
            }
        }
    }
    
    // Return default (X) if not found
    return Value("x");
}

int StreamingVCDTrace::signal_width(const std::string& name) {
    // Try exact match
    if (signal_info.find(name) != signal_info.end()) {
        return signal_info[name].width;
    }
    
    // Try to find by suffix (for hierarchical names)
    for (const auto& [full_name, info] : signal_info) {
        if (full_name.find(name) != std::string::npos) {
            return info.width;
        }
    }
    
    return 0;
}

bool StreamingVCDTrace::contains_signal(const std::string& name) const {
    return signal_info.find(name) != signal_info.end();
}

std::vector<std::string> StreamingVCDTrace::get_all_signals() const {
    std::vector<std::string> signals;
    for (const auto& [name, info] : signal_info) {
        signals.push_back(name);
    }
    return signals;
}

// Cache enhancement methods

void StreamingVCDTrace::warm_cache_for_signals(const std::vector<std::string>& signal_names) {
    // Pre-initialize cache for high-frequency signals
    for (const auto& signal : signal_names) {
        if (signal_info.find(signal) != signal_info.end()) {
            value_cache.warm_cache({signal});
        }
    }
}

void StreamingVCDTrace::profile_cache_performance() {
    // Log cache statistics periodically for analysis
    if (!current_values.empty()) {
        std::string stats = value_cache.get_statistics();
        // In production: write to logger
        // For now: available via get_memory_stats()
    }
}

} // namespace wal
