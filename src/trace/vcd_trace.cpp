#include "wal/trace/vcd_trace.h"
#include "wal/trace/trace_container.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

namespace wal {

const std::string& VCDTrace::intern_identifier(const std::string& id) {
    auto it = identifier_intern_pool.find(id);
    if (it != identifier_intern_pool.end()) {
        return it->second;
    }
    // Insert and return reference to the stored string
    auto result = identifier_intern_pool.emplace(id, id);
    return result.first->second;
}

std::vector<std::string> VCDTrace::read_file_lines(const std::string& filename) {
    std::vector<std::string> lines;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open VCD file: " + filename);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Remove leading whitespace
        size_t start = 0;
        while (start < line.length() && std::isspace(line[start])) {
            start++;
        }
        if (start > 0) {
            line = line.substr(start);
        }
        
        // Remove trailing whitespace and carriage returns
        while (!line.empty() && (line.back() == '\r' || std::isspace(line.back()))) {
            line.pop_back();
        }
        lines.push_back(line);
    }
    
    file.close();
    return lines;
}

// Fast mmap-based line reader for large files
std::vector<std::string> VCDTrace::read_file_lines_mmap(const std::string& filename) {
    std::vector<std::string> lines;
    
    // Open file
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error("Cannot open VCD file: " + filename);
    }
    
    // Get file size
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        throw std::runtime_error("Cannot stat VCD file: " + filename);
    }
    
    size_t file_size = sb.st_size;
    if (file_size == 0) {
        close(fd);
        return lines;
    }
    
    // Memory map the file
    char* data = static_cast<char*>(mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (data == MAP_FAILED) {
        close(fd);
        throw std::runtime_error("Cannot mmap VCD file: " + filename);
    }
    
    // Reserve space for lines (estimate ~30 bytes per line average)
    lines.reserve(file_size / 30);
    
    // Parse lines directly from memory-mapped region
    const char* ptr = data;
    const char* end = data + file_size;
    
    while (ptr < end) {
        // Skip leading whitespace
        while (ptr < end && (*ptr == ' ' || *ptr == '\t')) {
            ++ptr;
        }
        
        // Find end of line
        const char* line_start = ptr;
        while (ptr < end && *ptr != '\n' && *ptr != '\r') {
            ++ptr;
        }
        
        // Extract line, removing trailing whitespace
        const char* line_end = ptr;
        while (line_end > line_start && (*(line_end-1) == ' ' || *(line_end-1) == '\t')) {
            --line_end;
        }
        
        if (line_end > line_start) {
            lines.emplace_back(line_start, line_end - line_start);
        } else {
            lines.emplace_back("");
        }
        
        // Skip newline characters
        while (ptr < end && (*ptr == '\n' || *ptr == '\r')) {
            ++ptr;
        }
    }
    
    // Cleanup
    munmap(data, file_size);
    close(fd);
    
    return lines;
}

Value VCDTrace::parse_bit_value(const std::string& bits) {
    if (bits.empty()) {
        return Value(0);
    }

    if (bits.length() == 1) {
        // Single bit: 0, 1, x, or z - fast path
        char c = bits[0];
        return Value((c == '1') ? 1 : 0);
    }

    // Values up to 64 bits fit in int64_t -- keep the fast integer path.
    // Wider values (e.g. wide testbench/string registers, or real >64-bit
    // hardware buses) must NOT be accumulated into an int64_t: left-shifting
    // past bit 63 silently discards the high bits, corrupting the value
    // (this previously caused e.g. a 1024-bit signal to be truncated to its
    // low 64 bits with no indication anything was wrong -- caught via a
    // WAL++-vs-Python-WAL cross-check that found Hamming Weight results
    // differing by orders of magnitude on the same trace). For these,
    // return a string Value holding the exact bit pattern as parsed;
    // downstream consumers (e.g. HammingOperators::count_ones /
    // value_to_binary_string) already handle string-typed Values, counting
    // '1' characters directly and correctly, including any 'x'/'z' states
    // (treated as non-set, consistent with the single-bit fast path above).
    if (bits.length() <= 64) {
        int64_t value = 0;
        const char* ptr = bits.data();
        const char* end = ptr + bits.length();

        while (ptr < end) {
            value = (value << 1) | ((*ptr == '1') ? 1 : 0);
            ++ptr;
        }

        return Value(value);
    }

    return Value(bits);
}

void VCDTrace::parse_header(const std::vector<std::string>& lines, size_t& line_idx) {
    std::string current_scope;
    std::vector<std::string> scope_stack;  // Track nested scopes
    
    while (line_idx < lines.size()) {
        std::string line = lines[line_idx];
        
        // Skip empty lines
        if (line.empty()) {
            line_idx++;
            continue;
        }
        
        // Check for section end
        if (line == "$enddefinitions" || line.substr(0, 15) == "$enddefinitions") {
            line_idx++;
            break;
        }
        
        // Parse different VCD header commands
        if (line.substr(0, 6) == "$scope") {
            // $scope module scope_name
            std::istringstream iss(line);
            std::string cmd, type, scope_name;
            iss >> cmd >> type >> scope_name;
            
            // Push scope onto stack
            scope_stack.push_back(scope_name);
            
            // Build full hierarchical scope
            current_scope = "";
            for (size_t i = 0; i < scope_stack.size(); ++i) {
                if (i > 0) current_scope += Trace::SCOPE_SEPARATOR;
                current_scope += scope_stack[i];
            }
            
            scope_names.push_back(current_scope);
        }
        else if (line.substr(0, 8) == "$upscope") {
            // Pop scope from stack
            if (!scope_stack.empty()) {
                scope_stack.pop_back();
            }
            
            // Rebuild current scope
            current_scope = "";
            for (size_t i = 0; i < scope_stack.size(); ++i) {
                if (i > 0) current_scope += Trace::SCOPE_SEPARATOR;
                current_scope += scope_stack[i];
            }
        }
        else if (line.substr(0, 4) == "$var") {
            // $var wire|reg|parameter|real 1|N identifier reference
            std::istringstream iss(line);
            std::string cmd, var_type;
            int width;
            std::string identifier, reference;
            
            iss >> cmd >> var_type >> width >> identifier >> reference;
            
            std::string full_name = reference;
            if (!current_scope.empty()) {
                full_name = current_scope + Trace::SCOPE_SEPARATOR + reference;
            }
            
            // If scope_filter is set, only register signals matching the filter
            bool should_register = scope_filter.empty() || 
                                   (full_name.length() >= scope_filter.length() &&
                                    full_name.substr(0, scope_filter.length()) == scope_filter);
            
            if (should_register) {
                SignalInfo info;
                info.name = reference;
                info.identifier = identifier;
                info.width = width;
                info.scope = current_scope;
                
                signal_info[full_name] = info;
                signal_id_map[identifier] = signal_info.size() - 1;
                signal_names.push_back(full_name);
                
                // Intern the identifier string for reuse
                const std::string& interned_id = intern_identifier(identifier);
                
                // Track this identifier as filtered
                filtered_identifiers.insert(interned_id);
                
                // Initialize current value to 0
                current_values[interned_id] = Value(0);
            }
        }
        else if (line.substr(0, 8) == "$timescale") {
            // $timescale 1ps
            // Ignore for now
        }
        else if (line.substr(0, 5) == "$date") {
            // $date ... $end
            // Skip until $end (check if already on same line)
            if (line.find("$end") == std::string::npos) {
                while (line_idx < lines.size()) {
                    line_idx++;
                    if (line_idx >= lines.size()) break;
                    line = lines[line_idx];
                    // Match lines starting with $end (handles both "$end" and "$enddefinitions $end")
                    if (line.length() >= 4 && line.substr(0, 4) == "$end") break;
                }
            }
        }
        else if (line.substr(0, 8) == "$version") {
            // $version ... $end
            // Skip until $end (check if already on same line)
            if (line.find("$end") == std::string::npos) {
                while (line_idx < lines.size()) {
                    line_idx++;
                    if (line_idx >= lines.size()) break;
                    line = lines[line_idx];
                    // Match lines starting with $end (handles both "$end" and "$enddefinitions $end")
                    if (line.length() >= 4 && line.substr(0, 4) == "$end") break;
                }
            }
        }
        
        line_idx++;
    }
}

void VCDTrace::parse_value_change(const std::string& line, int64_t& timestamp,
                                   std::unordered_map<std::string, Value>* changes) {
    if (line.empty()) return;

    if (line[0] == '#') {
        // Timestamp - fast integer parsing
        timestamp = 0;
        const char* ptr = line.data() + 1;
        const char* end = line.data() + line.length();
        while (ptr < end && *ptr >= '0' && *ptr <= '9') {
            timestamp = timestamp * 10 + (*ptr - '0');
            ++ptr;
        }
    } else if (line[0] == 'b') {
        // Binary value change - minimize allocations
        size_t space_pos = line.find(' ');
        if (space_pos != std::string::npos && space_pos + 1 < line.length()) {
            const char* id_ptr = line.data() + space_pos + 1;
            size_t id_len = line.length() - space_pos - 1;
            const char* bit_ptr = line.data() + 1;
            size_t bit_len = space_pos - 1;

            if (scope_filter.empty()) {
                std::string id_str(id_ptr, id_len);
                auto val = parse_bit_value(std::string(bit_ptr, bit_len));
                current_values[id_str] = val;
                if (changes) (*changes)[id_str] = val;
            } else {
                std::string id_str(id_ptr, id_len);
                auto it = filtered_identifiers.find(id_str);
                if (it != filtered_identifiers.end()) {
                    auto val = parse_bit_value(std::string(bit_ptr, bit_len));
                    current_values[*it] = val;
                    if (changes) (*changes)[*it] = val;
                }
            }
        }
    } else if (line[0] == '0' || line[0] == '1' || line[0] == 'x' || line[0] == 'z') {
        // Single bit change - ultra-fast path
        if (line.length() > 1) {
            const char* id_ptr = line.data() + 1;
            size_t id_len = line.length() - 1;
            Value val = (line[0] == '1') ? Value(1) : Value(0);

            if (scope_filter.empty()) {
                std::string id_str(id_ptr, id_len);
                current_values[id_str] = val;
                if (changes) (*changes)[id_str] = val;
            } else {
                std::string id_str(id_ptr, id_len);
                auto it = filtered_identifiers.find(id_str);
                if (it != filtered_identifiers.end()) {
                    current_values[*it] = val;
                    if (changes) (*changes)[*it] = val;
                }
            }
        }
    }
}

void VCDTrace::parse_data(const std::vector<std::string>& lines, size_t& line_idx) {
    int64_t current_timestamp = 0;
    int timestamp_count = 0;

    // Save the initial state (from $dumpvars) so signal_value can scan back to it
    initial_values = current_values;

    // Sparse: accumulate only the changed signals between consecutive timestamps
    std::unordered_map<std::string, Value> changes;

    size_t expected_signals = scope_filter.empty() ? 30000 : filtered_identifiers.size();
    current_values.reserve(expected_signals);
    time_steps.reserve(65536);

    while (line_idx < lines.size()) {
        const std::string& line = lines[line_idx];

        if (line.empty()) {
            line_idx++;
            continue;
        }

        if (line[0] == '#') {
            // Flush the accumulated changes for the current timestamp block
            if (timestamp_count > 0) {
                time_steps.emplace_back();
                time_steps.back().timestamp = current_timestamp;
                time_steps.back().signal_values = changes;  // sparse: only deltas
                timestamp_index_map[current_timestamp] = timestamp_count - 1;
                changes.clear();
            }

            parse_value_change(line, current_timestamp, nullptr);
            timestamp_count++;
        } else {
            // Pass &changes so the value is recorded in both current_values and changes
            parse_value_change(line, current_timestamp, &changes);
        }

        line_idx++;
    }

    // Flush the final timestamp
    if (timestamp_count > 0) {
        time_steps.emplace_back();
        time_steps.back().timestamp = current_timestamp;
        time_steps.back().signal_values = std::move(changes);
        timestamp_index_map[current_timestamp] = timestamp_count - 1;
    }

    max_index = static_cast<int>(time_steps.size()) - 1;
}

void VCDTrace::load() {
    try {
        // Use mmap for better performance on large files
        auto lines = read_file_lines_mmap(filename);
        
        if (lines.empty()) {
            throw std::runtime_error("VCD file is empty");
        }
        
        size_t line_idx = 0;
        
        // Parse header
        parse_header(lines, line_idx);
        
        // Look for $dumpvars (optional) or start of data (#)
        int64_t current_timestamp = 0;
        while (line_idx < lines.size()) {
            const std::string& line = lines[line_idx];
            if (line == "$dumpvars") {
                // Found $dumpvars - parse initial values until $end
                line_idx++;
                while (line_idx < lines.size() && lines[line_idx] != "$end") {
                    parse_value_change(lines[line_idx], current_timestamp);
                    line_idx++;
                }
                if (line_idx < lines.size()) line_idx++;  // skip $end
                break;
            } else if (!line.empty() && line[0] == '#') {
                // No $dumpvars section - data starts immediately
                break;
            }
            line_idx++;
        }
        
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

    auto sig_it = signal_info.find(full_name);
    if (sig_it == signal_info.end()) {
        // Lenient fallback so get-signal accepts every name signal?/signal-width
        // accept. The interpreter splits a name on '.' as well as '^', so a
        // dotted record-field leaf (a GRLIB bus's `ahbmo.haddr` / `apbo.pirq`)
        // arrives reconstructed with '^' and no longer equals the stored '.'
        // key — normalize '.' and '^' to compare, and also match the leaf name.
        auto norm = [](std::string s) {
            for (auto& c : s) if (c == '.') c = '^';
            return s;
        };
        const std::string fn = norm(full_name);
        for (auto it = signal_info.begin(); it != signal_info.end(); ++it) {
            if (it->second.name == full_name || norm(it->first) == fn) {
                sig_it = it;
                break;
            }
        }
    }
    if (sig_it == signal_info.end()) {
        throw std::runtime_error("Signal not found: " + full_name);
    }

    int target_index = current_index + offset;
    if (target_index < 0 || target_index > max_index) {
        throw std::runtime_error("Index out of range");
    }

    const std::string& identifier = sig_it->second.identifier;

    // Sparse storage: scan backward from target_index to find the most recent change
    for (int i = target_index; i >= 0; --i) {
        auto it = time_steps[i].signal_values.find(identifier);
        if (it != time_steps[i].signal_values.end()) {
            return it->second;
        }
    }

    // Fall back to initial values (from $dumpvars)
    auto init_it = initial_values.find(identifier);
    if (init_it != initial_values.end()) return init_it->second;

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

int64_t VCDTrace::get_timestamp() const {
    if (current_index >= 0 && current_index < static_cast<int>(time_steps.size())) {
        return time_steps[current_index].timestamp;
    }
    return 0;
}

void VCDTrace::parse_data_chunk(const std::vector<std::string>& lines,
                                 size_t start_idx,
                                 size_t end_idx,
                                 std::vector<TimeStep>& chunk_timesteps) {
    int current_timestamp = 0;
    std::unordered_map<std::string, Value> local_values = current_values;
    
    for (size_t i = start_idx; i < end_idx && i < lines.size(); ++i) {
        const std::string& line = lines[i];
        
        if (line.empty()) continue;
        
        if (line[0] == '#') {
            // New timestamp - save current values
            if (current_timestamp > 0 || !local_values.empty()) {
                TimeStep ts;
                ts.timestamp = current_timestamp;
                ts.signal_values = local_values;
                chunk_timesteps.push_back(ts);
            }
            
            // Parse new timestamp
            try {
                current_timestamp = std::stoi(line.substr(1));
            } catch (...) {
                current_timestamp = 0;
            }
        } else {
            // Parse value change into local_values
            int dummy_ts = current_timestamp;
            
            if (line[0] == 'b') {
                size_t space_pos = line.find(' ');
                if (space_pos != std::string::npos && space_pos + 1 < line.length()) {
                    const char* id_start = line.data() + space_pos + 1;
                    size_t id_len = line.length() - space_pos - 1;
                    std::string id_str(id_start, id_len);
                    
                    if (scope_filter.empty() || filtered_identifiers.count(id_str)) {
                        const char* bit_start = line.data() + 1;
                        size_t bit_len = space_pos - 1;
                        local_values[id_str] = parse_bit_value(std::string(bit_start, bit_len));
                    }
                }
            } else if (line[0] == '0' || line[0] == '1' || line[0] == 'x' || line[0] == 'z') {
                if (line.length() > 1) {
                    const char* id_start = line.data() + 1;
                    size_t id_len = line.length() - 1;
                    std::string id_str(id_start, id_len);
                    
                    if (scope_filter.empty() || filtered_identifiers.count(id_str)) {
                        Value val = (line[0] == '1') ? Value(1) : Value(0);
                        local_values[id_str] = val;
                    }
                }
            }
        }
    }
    
    // Save final timestamp for this chunk
    if (current_timestamp > 0 || !local_values.empty()) {
        TimeStep ts;
        ts.timestamp = current_timestamp;
        ts.signal_values = local_values;
        chunk_timesteps.push_back(ts);
    }
}

void VCDTrace::parse_data_parallel(const std::vector<std::string>& lines, size_t start_line_idx) {
    // Determine number of threads to use
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;  // Fallback
    
    // For small files, don't bother with parallelization
    size_t total_lines = lines.size() - start_line_idx;
    if (total_lines < 10000 || num_threads == 1) {
        // Use sequential parsing for small files
        size_t idx = start_line_idx;
        parse_data(lines, idx);
        return;
    }
    
    // Find timestamp boundaries for chunk splitting
    std::vector<size_t> timestamp_indices;
    timestamp_indices.push_back(start_line_idx);
    
    for (size_t i = start_line_idx; i < lines.size(); ++i) {
        if (!lines[i].empty() && lines[i][0] == '#') {
            timestamp_indices.push_back(i);
        }
    }
    
    if (timestamp_indices.size() < num_threads * 2) {
        // Not enough timestamps for effective parallelization
        size_t idx = start_line_idx;
        parse_data(lines, idx);
        return;
    }
    
    // Distribute timestamps across threads
    size_t timestamps_per_thread = timestamp_indices.size() / num_threads;
    std::vector<std::thread> threads;
    std::vector<std::vector<TimeStep>> thread_results(num_threads);
    
    for (unsigned int t = 0; t < num_threads; ++t) {
        size_t start_ts_idx = t * timestamps_per_thread;
        size_t end_ts_idx = (t == num_threads - 1) ? timestamp_indices.size() - 1 
                                                    : (t + 1) * timestamps_per_thread;
        
        size_t chunk_start = timestamp_indices[start_ts_idx];
        size_t chunk_end = (end_ts_idx < timestamp_indices.size()) 
                          ? timestamp_indices[end_ts_idx] 
                          : lines.size();
        
        threads.emplace_back([this, &lines, chunk_start, chunk_end, &thread_results, t]() {
            parse_data_chunk(lines, chunk_start, chunk_end, thread_results[t]);
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Merge results in order
    time_steps.reserve(timestamp_indices.size());
    for (unsigned int t = 0; t < num_threads; ++t) {
        time_steps.insert(time_steps.end(), 
                         thread_results[t].begin(), 
                         thread_results[t].end());
    }
    
    // Build timestamp index map
    for (size_t i = 0; i < time_steps.size(); ++i) {
        timestamp_index_map[time_steps[i].timestamp] = static_cast<int>(i);
    }
    
    max_index = static_cast<int>(time_steps.size()) - 1;
}

} // namespace wal
