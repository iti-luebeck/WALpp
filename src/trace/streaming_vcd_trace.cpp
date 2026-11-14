#include "wal/trace/streaming_vcd_trace.h"
#include "wal/fast/vcd_parser.h"
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
        // Through the shared constant: this reader hardcoded "^", so it kept
        // naming signals the old way after the separator changed to "." and
        // nothing else could resolve one of its names.
        if (!path.empty()) path += Trace::SCOPE_SEPARATOR;
        path += scope;
    }
    path += Trace::SCOPE_SEPARATOR;
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
    // One forward pass through the fast parser, which reads the header and then
    // streams the deltas without keeping any of them.
    //
    // This used to go through the chunk reader instead, appending 100 MB chunks
    // into a buffer and copying out of it, which cost several hundred megabytes
    // before a single signal had been read -- on the reader whose whole purpose is
    // to avoid that. The chunk reader is still what the CSV and FST streaming
    // readers use.
    fast_vcd::VCDParser parser;
    index_timestamps.clear();
    parser.set_delta_sink([&](const fast_vcd::DeltaTimestamp& delta) {
        index_timestamps.push_back(delta.timestamp);
    });
    parser.parse(filename);

    signal_info.clear();
    signal_order.clear();
    for (uint32_t id : parser.get_signal_ids()) {
        const auto& info = parser.get_signal_info(id);
        SignalInfo mine;
        mine.name = info.name;
        mine.identifier = info.vcd_id;
        mine.width = info.width;
        mine.scope = info.scope;
        if (!signal_info.count(info.name)) {
            signal_order.push_back(info.name);
        }
        signal_info[info.name] = mine;
    }

    max_index = index_timestamps.empty()
        ? 0 : static_cast<int>(index_timestamps.size()) - 1;
    current_index = 0;
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
                if (!signal_info.count(full_name)) {
                    // Declaration order, so SIGNALS reads the same from either
                    // reader. signal_info is an unordered_map, so iterating it
                    // gave an arbitrary order that differed from the full-load
                    // reader's and from run to run.
                    signal_order.push_back(full_name);
                }
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
            // Count timestamps so MAX-INDEX is known. The sparse index samples
            // every 10,000 entries, so it cannot answer how many there are, and
            // max_index stayed 0: a streaming trace reported itself as a single
            // sample, which made set-index and every sweep fail immediately.
            max_index = timestamp_count++;
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

/**
 * @brief One forward pass recording the changes of the requested signals.
 *
 * Uses the fast VCD parser in streaming mode, so the pass itself holds only the
 * current timestamp's changes. What is kept afterwards is one change list per
 * requested signal, which for the handful of signals an analysis actually watches
 * is a tiny fraction of the trace.
 */
void StreamingVCDTrace::scan_for(const std::vector<std::string>& wanted) {
    fast_vcd::VCDParser parser;

    // Resolve the requested names to parser signal ids. The parser must read the
    // header before the ids exist, so resolution happens on the first delta.
    std::unordered_map<uint32_t, std::string> id_to_name;
    bool resolved = false;
    int current = -1;

    for (const auto& name : wanted) {
        histories[name].clear();
    }
    std::vector<int64_t> timestamps;

    parser.set_delta_sink([&](const fast_vcd::DeltaTimestamp& delta) {
        if (!resolved) {
            // One name at a time: get_signal_ids_by_names SKIPS what it cannot
            // find, so zipping its result against `wanted` by position silently
            // mislabels every signal after the first missing one.
            for (const auto& name : wanted) {
                auto ids = parser.get_signal_ids_by_names({name});
                if (!ids.empty()) {
                    id_to_name[ids[0]] = name;
                }
            }
            // Seed from the values at time 0, so a signal that never changes still
            // has a value.
            for (const auto& [sid, val] : parser.get_initial_values()) {
                auto it = id_to_name.find(sid);
                if (it != id_to_name.end()) {
                    histories[it->second].push_back({0, Value(static_cast<int64_t>(val))});
                }
            }
            resolved = true;
        }

        ++current;
        timestamps.push_back(delta.timestamp);

        for (const auto& [sig_id, value] : delta.changes) {
            auto it = id_to_name.find(sig_id);
            if (it != id_to_name.end()) {
                auto& history = histories[it->second];
                if (!history.empty() && history.back().index == current) {
                    history.back().value = Value(static_cast<int64_t>(value));
                } else {
                    history.push_back({current, Value(static_cast<int64_t>(value))});
                }
            }
        }
        for (const auto& [sig_id, words] : delta.wide_changes) {
            auto it = id_to_name.find(sig_id);
            if (it != id_to_name.end()) {
                // Wider than 64 bits: keep the exact bit pattern as a string, the
                // same representation the non-streaming reader uses.
                std::string bits;
                for (size_t w = words.size(); w-- > 0;) {
                    for (int bit = 63; bit >= 0; --bit) {
                        bits += ((words[w] >> bit) & 1ULL) ? '1' : '0';
                    }
                }
                histories[it->second].push_back({current, Value(bits)});
            }
        }
    });

    parser.parse(filename);

    if (index_timestamps.empty() && !timestamps.empty()) {
        index_timestamps = std::move(timestamps);
        max_index = static_cast<int>(index_timestamps.size()) - 1;
    }
    scanned_once = true;
}

void StreamingVCDTrace::ensure_history(const std::string& name) {
    if (histories.count(name)) {
        return;
    }
    // Re-scan for everything wanted so far plus the new name, so a later read of an
    // already-loaded signal never triggers another pass.
    std::vector<std::string> wanted;
    wanted.reserve(histories.size() + 1);
    for (const auto& entry : histories) {
        wanted.push_back(entry.first);
    }
    wanted.push_back(name);
    scan_for(wanted);
    // A name the trace does not contain gets an empty history, so it is not
    // rescanned on every read.
    histories.emplace(name, std::vector<Change>{});
}

Value StreamingVCDTrace::signal_value(const std::string& name, int offset,
                                     const std::string& scope) {
    std::string lookup = name;
    if (!scope.empty()) {
        lookup = scope + Trace::SCOPE_SEPARATOR + name;
    }
    if (signal_info.find(lookup) == signal_info.end()) {
        throw std::runtime_error("Signal not found: " + lookup);
    }

    ensure_history(lookup);

    int target = current_index + offset;
    if (target < 0 || target > max_index) {
        throw std::out_of_range("Index out of range: " + std::to_string(target));
    }

    const auto& history = histories[lookup];
    if (history.empty()) {
        return Value(static_cast<int64_t>(0));
    }

    // The value in force at `target` is the last change at or before it.
    size_t lo = 0, hi = history.size();
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (history[mid].index <= target) lo = mid + 1;
        else hi = mid;
    }
    if (lo == 0) {
        return Value(static_cast<int64_t>(0));   // before the first recorded change
    }
    return history[lo - 1].value;
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
    return signal_order;
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
