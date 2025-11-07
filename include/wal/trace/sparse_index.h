#pragma once

#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include <stdexcept>

namespace wal {

/**
 * @brief Timestamp to file offset mapping for fast seeking
 * 
 * Maintains a sparse index of (timestamp, file_offset) pairs to enable
 * efficient random-access seeking in large trace files without loading
 * all data into memory.
 * 
 * **Index Strategy:**
 * - Stores every N-th timestamp (e.g., every 10,000th)
 * - Uses binary search for O(log n) lookups
 * - Memory: ~1 MB for 1 billion timestamps (with sampling factor ~10,000)
 * 
 * **Use Case:**
 * When processing a streaming chunk, you encounter timestamps.
 * Record key timestamps in the index. Later, when jumping to a specific
 * timestamp, use binary_search() to find the appropriate file offset
 * and resume from there.
 * 
 * **Example:**
 * @code
 * SparseIndex index;
 * 
 * // While streaming, periodically record positions:
 * for (auto& chunk : streaming_reader) {
 *     // ... parse chunk ...
 *     if (timestamp % 10000 == 0) {
 *         index.add_entry(timestamp, file_offset);
 *     }
 * }
 * 
 * // Later, jump to timestamp 5,000,000:
 * auto [target_offset, lower_timestamp] = index.binary_search(5000000);
 * // Seek to file offset, then parse until exact timestamp
 * @endcode
 * 
 * **Thread Safety:**
 * NOT thread-safe. Synchronize externally if accessed from multiple threads.
 * 
 * @see StreamingChunkReader for reading chunks
 * @see LazyTraceQuery for using index in queries
 */
class SparseIndex {
private:
    /// (timestamp -> file_offset) mapping, sorted by timestamp
    std::map<int64_t, int64_t> entries;
    
    /// Sampling factor: store every N-th entry encountered
    /// Configurable for memory/performance tradeoffs
    size_t sampling_factor;
    
    /// Counter for sampling - incremented for each add_entry() call
    size_t entry_count;

public:
    /**
     * @brief Construct index with optional sampling
     * 
     * @param sampling_factor Store every N-th entry (default: 10,000)
     *                        Use 1 for complete index (more memory)
     *                        Use larger values for sparser index (less memory)
     */
    explicit SparseIndex(size_t sampling_factor = 10000)
        : sampling_factor(sampling_factor), entry_count(0) {
        if (sampling_factor == 0) {
            throw std::invalid_argument("sampling_factor must be > 0");
        }
    }

    /**
     * @brief Add a timestamp -> file_offset mapping
     * 
     * Sampling is applied: entry is only added if entry_count % sampling_factor == 0.
     * Call this for each timestamp encountered during streaming.
     * 
     * **Typical Usage in Streaming Reader:**
     * @code
     * while (reader) {
     *     chunk = reader.get_chunk();
     *     for (auto timestamp : parse_timestamps(chunk)) {
     *         index.add_entry(timestamp, current_file_offset);
     *     }
     *     reader.next();
     * }
     * @endcode
     * 
     * @param timestamp Simulation timestamp
     * @param file_offset Byte offset in trace file where this timestamp begins
     */
    void add_entry(int64_t timestamp, int64_t file_offset) {
        ++entry_count;
        
        // Only store every sampling_factor-th entry
        if (entry_count % sampling_factor == 0) {
            entries[timestamp] = file_offset;
        }
    }

    /**
     * @brief Binary search for file offset near target timestamp
     * 
     * Finds the largest timestamp <= target_timestamp and returns its offset.
     * Useful for seeking to a specific time in the trace file.
     * 
     * **Example:**
     * @code
     * auto [offset, ts] = index.binary_search(5000000);
     * // offset = file position to start reading from
     * // ts = actual timestamp at that position
     * // Now seek to offset and parse forward to find exact timestamp
     * @endcode
     * 
     * **Return Semantics:**
     * - If target_timestamp is before all entries: returns {0, -1}
     * - Otherwise: returns {offset_of_matching_timestamp, matching_timestamp}
     * 
     * **Complexity:** O(log m) where m = number of index entries
     * (typically much smaller than actual number of timestamps due to sampling)
     * 
     * @param target_timestamp The timestamp to search for
     * @return Pair of (file_offset, found_timestamp)
     *         file_offset: Where to seek in the file
     *         found_timestamp: Actual timestamp at that position (-1 if not found)
     */
    std::pair<int64_t, int64_t> binary_search(int64_t target_timestamp) const {
        if (entries.empty()) {
            return {0, -1};
        }

        // Find first entry > target_timestamp, then go back one
        auto it = entries.upper_bound(target_timestamp);
        
        if (it == entries.begin()) {
            // Target is before all entries
            return {0, -1};
        }

        // Step back to get largest entry <= target_timestamp
        --it;
        return {it->second, it->first};
    }

    /**
     * @brief Get all index entries
     * 
     * Used for debugging, statistics, or rebuilding indices.
     * 
     * @return Map of (timestamp -> file_offset) pairs
     */
    const std::map<int64_t, int64_t>& get_entries() const {
        return entries;
    }

    /**
     * @brief Get number of stored entries
     * 
     * Due to sampling, this is typically much smaller than the total
     * number of timestamps in the file.
     * 
     * @return Number of index entries currently stored
     */
    size_t entry_count_stored() const {
        return entries.size();
    }

    /**
     * @brief Get total number of add_entry() calls
     * 
     * This is the total number of timestamps encountered,
     * not the number stored (due to sampling).
     * 
     * @return Total entries processed
     */
    size_t entry_count_total() const {
        return entry_count;
    }

    /**
     * @brief Get configured sampling factor
     * 
     * @return Sampling factor (every N-th entry is stored)
     */
    size_t get_sampling_factor() const {
        return sampling_factor;
    }

    /**
     * @brief Check if index is empty
     * 
     * @return true if no entries have been added
     */
    bool empty() const {
        return entries.empty();
    }

    /**
     * @brief Clear all entries
     * 
     * Resets index to empty state. Note: entry_count_total is NOT reset.
     */
    void clear() {
        entries.clear();
        // Don't reset entry_count - it represents cumulative entries
    }

    /**
     * @brief Get memory usage estimate in bytes
     * 
     * Rough estimate of memory used by this index.
     * Each entry is approximately 16 bytes (two 8-byte integers).
     * 
     * @return Approximate memory usage in bytes
     */
    size_t estimated_memory_bytes() const {
        // Each entry: int64_t timestamp + int64_t offset = 16 bytes
        // Plus map overhead: ~48 bytes per entry in std::map
        return entries.size() * (16 + 48);
    }

    /**
     * @brief Get summary statistics
     * 
     * @return String with index statistics for debugging
     */
    std::string get_stats() const {
        if (entries.empty()) {
            return "SparseIndex: empty";
        }
        
        int64_t min_ts = entries.begin()->first;
        int64_t max_ts = entries.rbegin()->first;
        int64_t min_offset = entries.begin()->second;
        int64_t max_offset = entries.rbegin()->second;
        
        return "SparseIndex: " + std::to_string(entries.size()) + " entries, "
               + "timestamp range: [" + std::to_string(min_ts) + ", " 
               + std::to_string(max_ts) + "], "
               + "offset range: [" + std::to_string(min_offset) + ", "
               + std::to_string(max_offset) + "], "
               + "sampling: 1/" + std::to_string(sampling_factor) + ", "
               + "memory: " + std::to_string(estimated_memory_bytes()) + " bytes";
    }
};

} // namespace wal
