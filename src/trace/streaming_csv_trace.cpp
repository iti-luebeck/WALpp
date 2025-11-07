#include "wal/trace/streaming_csv_trace.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace wal {

StreamingCSVTrace::StreamingCSVTrace(const std::string& tid, 
                                     const std::string& filename,
                                     std::shared_ptr<TraceContainer> container,
                                     size_t chunk_size,
                                     size_t cache_entries)
    : Trace(tid, filename, container),
      chunk_size_bytes(chunk_size),
      cache_capacity_per_signal(cache_entries) {
    
    // Note: chunk_reader will be created in load()
    sparse_index = std::make_shared<SparseIndex>();
    
    // Initialize signal caches
    // Will be populated during header parsing
}

void StreamingCSVTrace::load() {
    try {
        // Create chunk reader now that we have the filename
        chunk_reader = std::make_shared<StreamingChunkReader>(create_chunk_reader(filename, chunk_size_bytes));
        
        // Parse CSV header to get signal names
        parse_csv_header();
        
        // Initialize sparse index from file
        // This would build timestamp -> offset mapping
        
    } catch (const std::exception& e) {
        throw WalError("Failed to load streaming CSV trace: " + std::string(e.what()));
    }
}

void StreamingCSVTrace::parse_csv_header() {
    std::string buffer;
    
    // Read first chunk to get header
    if (chunk_reader && chunk_reader->has_chunk()) {
        const auto& chunk = chunk_reader->get_chunk();
        buffer.append(chunk.data.begin(), chunk.data.end());
    }
    
    // Find first line (header)
    size_t newline_pos = buffer.find('\n');
    if (newline_pos == std::string::npos) {
        newline_pos = buffer.length();
    }
    
    std::string header_line = buffer.substr(0, newline_pos);
    
    // Trim whitespace
    header_line.erase(0, header_line.find_first_not_of(" \t\r\n"));
    header_line.erase(header_line.find_last_not_of(" \t\r\n") + 1);
    
    // Split header by delimiter
    auto columns = split_csv_line(header_line, delimiter);
    
    // Extract signal names
    size_t col_idx = 0;
    for (const auto& col : columns) {
        std::string signal_name = col;
        signal_name.erase(0, signal_name.find_first_not_of(" \t"));
        signal_name.erase(signal_name.find_last_not_of(" \t") + 1);
        
        if (signal_name.empty()) continue;
        
        if (col_idx == 0 && signal_name == time_column) {
            // Time column - skip
            column_to_signal[col_idx] = ""; // Empty = time column
        } else {
            // Signal column
            signal_info[signal_name] = SignalInfo{signal_name, 1, col_idx};
            column_to_signal[col_idx] = signal_name;
        }
        col_idx++;
    }
}

Value StreamingCSVTrace::read_signal_value(const std::string& name, int64_t timestamp) {
    // Look up value from CSV data at specific timestamp
    // In a full implementation, this would:
    // 1. Use sparse_index to find file offset for timestamp
    // 2. Read appropriate chunk from streaming reader
    // 3. Parse CSV line
    // 4. Extract value for this signal
    
    // For now, return default cached value or 0
    if (signal_info.find(name) != signal_info.end()) {
        return Value(0);  // Placeholder - would parse from file
    }
    
    return Value(0);
}

Value StreamingCSVTrace::lookup_or_load(const std::string& name, int64_t timestamp) {
    if (signal_caches.find(name) == signal_caches.end()) {
        signal_caches[name] = SignalCache();
    }
    
    auto& cache = signal_caches[name];
    
    // Check cache for value
    for (auto& entry : cache.entries) {
        if (entry.index == current_index) {
            cache.hits++;
            entry.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
            return entry.value;
        }
    }
    
    // Cache miss - load from file
    cache.misses++;
    Value value = read_signal_value(name, timestamp);
    
    // Add to cache
    if (cache.entries.size() >= cache.max_entries) {
        evict_lru(cache);
    }
    
    CacheEntry entry{static_cast<int>(current_index), value, 
                    std::chrono::system_clock::now().time_since_epoch().count()};
    cache.entries.push_back(entry);
    
    return value;
}

void StreamingCSVTrace::evict_lru(SignalCache& cache) {
    // Find entry with oldest timestamp
    size_t oldest_idx = 0;
    int64_t oldest_time = cache.entries[0].timestamp;
    
    for (size_t i = 1; i < cache.entries.size(); i++) {
        if (cache.entries[i].timestamp < oldest_time) {
            oldest_time = cache.entries[i].timestamp;
            oldest_idx = i;
        }
    }
    
    // Evict oldest entry
    cache.entries.erase(cache.entries.begin() + oldest_idx);
    cache.evictions++;
}

Value StreamingCSVTrace::signal_value(const std::string& name, int offset,
                                      const std::string& scope) {
    if (!contains_signal(name)) {
        throw WalError("Signal not found: " + name);
    }
    
    int64_t target_timestamp = current_timestamp + offset;
    return lookup_or_load(name, target_timestamp);
}

int StreamingCSVTrace::signal_width(const std::string& name) {
    auto it = signal_info.find(name);
    if (it == signal_info.end()) {
        throw WalError("Signal not found: " + name);
    }
    return it->second.width;
}

bool StreamingCSVTrace::contains_signal(const std::string& name) const {
    return signal_info.find(name) != signal_info.end();
}

std::vector<std::string> StreamingCSVTrace::get_all_signals() const {
    std::vector<std::string> signals;
    for (const auto& [name, _] : signal_info) {
        signals.push_back(name);
    }
    return signals;
}

bool StreamingCSVTrace::step(int steps) {
    // Prevent stepping backwards before first index
    if (steps < 0 && static_cast<int>(current_index) + steps < 0) {
        return false;
    }
    
    current_index += steps;
    return true;
}

void StreamingCSVTrace::warm_cache(const std::vector<std::string>& signal_names) {
    // Pre-load frequently accessed signals into cache
    for (const auto& name : signal_names) {
        if (contains_signal(name)) {
            // Load multiple values for the signal
            for (int offset = 0; offset < 10; offset++) {
                try {
                    lookup_or_load(name, current_timestamp + offset);
                } catch (...) {
                    // Ignore if can't load
                }
            }
        }
    }
}

std::string StreamingCSVTrace::get_cache_statistics() const {
    std::string stats = "CSV Cache Statistics:\n";
    int64_t total_hits = 0, total_misses = 0, total_evictions = 0;
    
    for (const auto& [name, cache] : signal_caches) {
        total_hits += cache.hits;
        total_misses += cache.misses;
        total_evictions += cache.evictions;
        
        double hit_rate = cache.hit_rate();
        stats += "  Signal: " + name + 
                " Hits: " + std::to_string(cache.hits) +
                " Misses: " + std::to_string(cache.misses) +
                " Hit rate: " + std::to_string(hit_rate) + "%\n";
    }
    
    if (total_hits + total_misses > 0) {
        double overall_hit_rate = (100.0 * total_hits) / (total_hits + total_misses);
        stats += "Overall: Hits=" + std::to_string(total_hits) +
                " Misses=" + std::to_string(total_misses) +
                " Evictions=" + std::to_string(total_evictions) +
                " Hit rate=" + std::to_string(overall_hit_rate) + "%\n";
    }
    
    return stats;
}

std::vector<std::string> StreamingCSVTrace::split_csv_line(
    const std::string& line, char delim) {
    
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    
    bool in_quotes = false;
    std::string current;
    
    for (char c : line) {
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == delim && !in_quotes) {
            fields.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    
    fields.push_back(current);
    return fields;
}

Value StreamingCSVTrace::parse_value(const std::string& value) {
    // Convert string to numeric value
    if (value.empty()) return Value(0);
    
    try {
        // Try parsing as integer first
        int64_t ival = std::stoll(value);
        return Value(ival);
    } catch (...) {
        // Try parsing as floating point
        try {
            double dval = std::stod(value);
            return Value(static_cast<int64_t>(dval));
        } catch (...) {
            return Value(0);
        }
    }
}

int StreamingCSVTrace::infer_width(int64_t max_value) {
    if (max_value == 0) return 1;
    
    int width = 0;
    while (max_value > 0) {
        width++;
        max_value >>= 1;
    }
    return width;
}

} // namespace wal

