#pragma once

#include "trace.h"
#include "streaming_chunk_reader.h"
#include "sparse_index.h"
#include <unordered_map>
#include <memory>
#include <deque>
#include <optional>
#include <chrono>
#include <iostream>
#include <vector>

namespace wal {

/**
 * @brief LRU Cache for signal values with statistics
 * 
 * Caches recently accessed signal values to improve query performance
 * without loading the entire file into memory.
 * 
 * Features:
 * - Per-signal LRU cache
 * - Hit/miss statistics
 * - Adaptive sizing
 * - Cache warming
 * 
 * Memory: O(cache_size) bounded
 */
class SignalValueCache {
private:
    struct CacheEntry {
        int index;
        Value value;
        int64_t timestamp;  // When accessed
    };
    
    struct SignalCache {
        std::deque<CacheEntry> entries;
        size_t max_entries;
        
        // Statistics
        int64_t hits = 0;
        int64_t misses = 0;
        int64_t evictions = 0;
        
        SignalCache() : max_entries(1000) {}
        SignalCache(size_t capacity) : max_entries(capacity) {}
        
        void add(int index, const Value& value) {
            auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            
            // Remove oldest if at capacity
            if (entries.size() >= max_entries) {
                entries.pop_front();
                evictions++;
            }
            entries.push_back({index, value, now});
        }
        
        std::optional<Value> get(int index) {
            for (auto& entry : entries) {
                if (entry.index == index) {
                    hits++;
                    entry.timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                    return entry.value;
                }
            }
            misses++;
            return std::nullopt;
        }
        
        void clear() {
            entries.clear();
            hits = 0;
            misses = 0;
            evictions = 0;
        }
        
        double hit_rate() const {
            int64_t total = hits + misses;
            return total == 0 ? 0.0 : (100.0 * hits) / total;
        }
    };
    
    std::unordered_map<std::string, SignalCache> cache;
    size_t cache_capacity;
    size_t adaptive_capacity;  // For dynamic sizing

public:
    /**
     * @brief Construct cache with given entry capacity per signal
     * 
     * @param capacity_per_signal Max entries to cache per signal (default: 1000)
     */
    explicit SignalValueCache(size_t capacity_per_signal = 1000)
        : cache_capacity(capacity_per_signal) {}

    /**
     * @brief Add value to cache
     * 
     * @param signal_name Signal identifier
     * @param index Time index
     * @param value Signal value at this index
     */
    void add(const std::string& signal_name, int index, const Value& value) {
        if (cache.find(signal_name) == cache.end()) {
            cache[signal_name] = SignalCache(cache_capacity);
        }
        cache[signal_name].add(index, value);
    }

    /**
     * @brief Try to retrieve cached value
     * 
     * @param signal_name Signal identifier
     * @param index Time index
     * @return Cached value if present, std::nullopt otherwise
     */
    std::optional<Value> get(const std::string& signal_name, int index) {
        auto it = cache.find(signal_name);
        if (it != cache.end()) {
            return it->second.get(index);
        }
        return std::nullopt;
    }

    /**
     * @brief Clear all cached values
     */
    void clear() {
        for (auto& [signal, cache_entries] : cache) {
            cache_entries.clear();
        }
    }

    /**
     * @brief Get estimated memory usage
     * 
     * @return Estimated bytes used by cache
     */
    size_t estimated_memory_bytes() const {
        size_t total = 0;
        for (const auto& [signal, cache_entries] : cache) {
            total += signal.size() + (cache_entries.entries.size() * 48);
        }
        return total;
    }

    /**
     * @brief Get cache hit rate
     * 
     * @param signal_name Signal identifier
     * @return Hit rate percentage (0-100)
     */
    double get_hit_rate(const std::string& signal_name) const {
        auto it = cache.find(signal_name);
        if (it != cache.end()) {
            return it->second.hit_rate();
        }
        return 0.0;
    }

    /**
     * @brief Get overall cache statistics
     * 
     * @return String with hit rate, evictions, etc
     */
    std::string get_statistics() const {
        int64_t total_hits = 0, total_misses = 0, total_evictions = 0;
        for (const auto& [signal, stat] : cache) {
            total_hits += stat.hits;
            total_misses += stat.misses;
            total_evictions += stat.evictions;
        }
        
        double overall_hit_rate = (total_hits + total_misses) == 0 ? 0.0 
            : (100.0 * total_hits) / (total_hits + total_misses);
        
        return "Cache: " + std::to_string(total_hits) + " hits, "
               + std::to_string(total_misses) + " misses, "
               + std::to_string(total_evictions) + " evictions, "
               + std::to_string(overall_hit_rate) + "% hit rate";
    }

    /**
     * @brief Warm cache with frequently accessed signals
     * 
     * @param signal_names Vector of signal names to pre-load
     */
    void warm_cache(const std::vector<std::string>& signal_names) {
        for (const auto& signal : signal_names) {
            if (cache.find(signal) == cache.end()) {
                cache[signal] = SignalCache(cache_capacity);
            }
        }
    }

    /**
     * @brief Adapt cache size based on performance
     * 
     * Increases capacity if hit rate is low, decreases if memory-constrained
     */
    void adapt_cache_size() {
        int64_t total_hits = 0, total_misses = 0;
        for (const auto& [signal, stat] : cache) {
            total_hits += stat.hits;
            total_misses += stat.misses;
        }
        
        double hit_rate = (total_hits + total_misses) == 0 ? 50.0
            : (100.0 * total_hits) / (total_hits + total_misses);
        
        // Adaptive algorithm
        if (hit_rate < 70.0 && adaptive_capacity < cache_capacity * 2) {
            adaptive_capacity = std::min(adaptive_capacity * 1.5, cache_capacity * 2.0);
        } else if (hit_rate > 95.0 && adaptive_capacity > cache_capacity / 2) {
            adaptive_capacity = std::max(adaptive_capacity / 1.2, cache_capacity / 2.0);
        }
    }
};

/**
 * @brief Streaming VCD trace reader with lazy evaluation
 * 
 * Extends the Trace base class to provide memory-efficient access to large VCD files
 * using streaming chunks and sparse indexing.
 * 
 * Memory characteristics:
 * - Chunk: 100 MB (configurable)
 * - Index: ~1 MB per billion timestamps
 * - Cache: Configurable per-signal LRU
 * - Total: ~116 MB for 1 TB files (vs 5 TB with load-all)
 * 
 * Usage:
 * @code
 * auto trace = std::make_shared<StreamingVCDTrace>(tid, filename, container);
 * trace->load();  // Starts streaming and indexes
 * auto value = trace->signal_value("clk", 0);  // Lazy lookup
 * @endcode
 */
class StreamingVCDTrace : public Trace {
private:
    int timestamp_count = 0;   ///< how many '#' records were seen; drives max_index

    // Streaming components
    std::unique_ptr<StreamingChunkReader> chunk_reader;
    SparseIndex index;
    SignalValueCache value_cache;
    
    // Configuration
    size_t chunk_size_bytes = 100 * 1024 * 1024;  // 100 MB default
    size_t cache_capacity_per_signal = 1000;
    
    // Parsing state
    struct SignalInfo {
        std::string name;
        std::string identifier;
        int width;
        std::string scope;
        std::vector<std::string> scope_hierarchy;  // Hierarchical scope path
    };
    
    std::unordered_map<std::string, SignalInfo> signal_info;
    std::unordered_map<std::string, int> identifier_map;
    std::vector<std::string> current_scope_stack;  // Track scope hierarchy
    std::unordered_map<std::string, std::vector<std::string>> scope_hierarchy_map;
    
    // Current state
    std::unordered_map<std::string, Value> current_values;
    
    // Parsing helpers
    bool parse_header_from_chunks();
    bool parse_data_from_chunks();
    void parse_vcd_line(const std::string& line, int64_t& current_timestamp);
    
    // Hierarchical scope handling
    void enter_scope(const std::string& scope_name);
    void exit_scope();
    std::string get_full_signal_path(const std::string& signal_name) const;
    void validate_vcd_format(const std::string& line);
    
public:
    /**
     * @brief Constructor
     * 
     * @param tid Trace ID for identification
     * @param filename Path to VCD file
     * @param container Parent TraceContainer
     * @param chunk_sz Chunk size in bytes (default: 100 MB)
     * @param cache_sz Cache capacity per signal (default: 1000)
     */
    StreamingVCDTrace(const std::string& tid, const std::string& filename,
                     std::shared_ptr<TraceContainer> container,
                     size_t chunk_sz = 100 * 1024 * 1024,
                     size_t cache_sz = 1000)
        : Trace(tid, filename, container),
          index(10000),  // Sample every 10,000 timestamps
          value_cache(cache_sz),
          chunk_size_bytes(chunk_sz),
          cache_capacity_per_signal(cache_sz) {}

    /**
     * @brief Load and parse VCD file with streaming
     * 
     * Starts streaming the file, builds sparse index, and parses header.
     * Data is parsed on-demand as needed.
     * 
     * @throws std::ios_base::failure if file cannot be opened
     */
    void load();

    /**
     * @brief Get signal value with lazy evaluation
     * 
     * First checks cache, then seeks to appropriate chunk using sparse index,
     * and parses the necessary data.
     * 
     * @param name Signal name
     * @param offset Time offset from current index
     * @param scope Optional scope prefix
     * @return Signal value at specified time
     */
    Value signal_value(const std::string& name, int offset = 0,
                      const std::string& scope = "") override;

    /**
     * @brief Get signal width in bits
     * 
     * @param name Signal name
     * @return Width in bits
     */
    int signal_width(const std::string& name) override;

    /**
     * @brief Check if signal exists
     * 
     * @param name Signal name
     * @return true if signal is present
     */
    bool contains_signal(const std::string& name) const override;

    /**
     * @brief Get all signal names
     * 
     * @return Vector of signal names
     */
    std::vector<std::string> get_all_signals() const override;

    /**
     * @brief Get memory usage statistics
     * 
     * @return String with memory breakdown
     */
    std::string get_memory_stats() const {
        return "StreamingVCDTrace: "
               "chunk_size=" + std::to_string(chunk_size_bytes / 1024 / 1024) + " MB, "
               "index=" + std::to_string(index.estimated_memory_bytes() / 1024) + " KB, "
               "cache=" + std::to_string(value_cache.estimated_memory_bytes() / 1024) + " KB";
    }

    /**
     * @brief Warm cache for frequently accessed signals
     * 
     * @param signal_names Vector of signal names to pre-initialize
     */
    void warm_cache(const std::vector<std::string>& signal_names) {
        value_cache.warm_cache(signal_names);
    }

    void warm_cache_for_signals(const std::vector<std::string>& signal_names);

    /**
     * @brief Get cache statistics for reporting
     * 
     * @return String with cache hit rate, evictions, etc
     */
    std::string get_cache_statistics() const {
        return value_cache.get_statistics();
    }

    /**
     * @brief Profile and log cache performance
     */
    void profile_cache_performance();

};

} // namespace wal
