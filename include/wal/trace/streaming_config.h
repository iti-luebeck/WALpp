#pragma once

#include <cstddef>
#include <string>

namespace wal {

/**
 * @brief Configuration for streaming trace loading
 * 
 * Controls how large trace files are loaded into the system.
 * Can use either full-loading or memory-efficient streaming.
 */
struct StreamingConfig {
    /**
     * Enable streaming mode for large files
     * - true: Use StreamingVCDTrace/StreamingCSVTrace/StreamingFSTTrace
     * - false: Use regular VCDTrace/CSVTrace/FSTTrace (full load)
     */
    bool enabled = true;
    
    /**
     * Threshold in bytes to automatically enable streaming
     * Files larger than this will use streaming regardless of enabled flag
     * Set to 0 to disable auto-detection
     */
    size_t auto_threshold_bytes = 100 * 1024 * 1024;  // 100 MB default
    
    /**
     * Chunk size for streaming I/O operations
     * Larger chunks = fewer I/O ops, more memory
     * Smaller chunks = more I/O ops, less memory
     */
    size_t chunk_size_bytes = 100 * 1024 * 1024;  // 100 MB
    
    /**
     * Per-signal cache capacity for streaming
     * Number of values cached per signal
     * Each value: ~48 bytes, so 1000 entries ~= 48 KB per signal
     */
    size_t cache_capacity_per_signal = 1000;
    
    /**
     * Sparse index sample rate
     * Lower values = smaller index but slower lookups
     * Higher values = larger index but faster lookups
     */
    size_t index_sample_rate = 10000;  // Sample every 10,000 timestamps
    
    /**
     * Warm cache on load with frequently accessed signals
     */
    std::vector<std::string> warm_cache_signals;
    
    /**
     * Enable cache statistics reporting
     */
    bool report_cache_stats = false;
    
    /**
     * Check if streaming should be used for a file
     */
    bool should_stream(size_t file_size_bytes) const {
        if (auto_threshold_bytes > 0 && file_size_bytes > auto_threshold_bytes) {
            return true;  // Auto-enable for large files
        }
        return enabled;
    }
};

} // namespace wal
