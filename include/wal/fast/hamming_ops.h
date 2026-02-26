#pragma once

#include "vcd_parser.h"
#include <vector>
#include <utility>
#include <cstdint>

namespace fast_vcd {

/**
 * @brief Result type: (timestamp, distance/weight)
 */
using Result = std::vector<std::pair<int64_t, int64_t>>;

/**
 * @brief Hamming operations on parsed VCD data
 */
class HammingOps {
public:
    explicit HammingOps(const VCDParser& parser) : parser_(parser) {}
    
    /**
     * @brief Calculate Hamming Distance across all signals at each timestamp
     * @return Vector of (timestamp, hamming_distance) pairs
     */
    Result hamming_distance_all() const;
    
    /**
     * @brief Calculate Hamming Distance for signals in a specific scope
     * @param scope_prefix Hierarchical scope prefix (e.g., "top.cpu.core")
     * @return Vector of (timestamp, hamming_distance) pairs
     */
    Result hamming_distance_scope(const std::string& scope_prefix) const;
    
    /**
     * @brief Calculate Hamming Weight across all signals at each timestamp
     * @return Vector of (timestamp, hamming_weight) pairs
     */
    Result hamming_weight_all() const;
    
    /**
     * @brief Calculate Hamming Weight for signals in a specific scope
     * @param scope_prefix Hierarchical scope prefix (e.g., "top.cpu.core")
     * @return Vector of (timestamp, hamming_weight) pairs
     */
    Result hamming_weight_scope(const std::string& scope_prefix) const;
    
    /**
     * @brief Calculate Hamming Distance for a specific list of signals
     * @param signal_names Vector of full hierarchical signal names
     * @return Vector of (timestamp, hamming_distance) pairs
     */
    Result hamming_distance_signals(const std::vector<std::string>& signal_names) const;
    
    /**
     * @brief Calculate Hamming Weight for a specific list of signals
     * @param signal_names Vector of full hierarchical signal names
     * @return Vector of (timestamp, hamming_weight) pairs
     */
    Result hamming_weight_signals(const std::vector<std::string>& signal_names) const;
    
private:
    const VCDParser& parser_;
    
    /**
     * @brief Reconstruct full state at a timestamp from deltas
     */
    void reconstruct_state(
        size_t delta_idx,
        const std::vector<uint32_t>& signal_ids,
        std::vector<int64_t>& state
    ) const;
    
    /**
     * @brief Calculate HD between two states (optimized with popcount)
     */
    int64_t calculate_hd(
        const std::vector<int64_t>& prev_state,
        const std::vector<int64_t>& curr_state
    ) const;
    
    /**
     * @brief Calculate HW of a state (count 1-bits)
     */
    int64_t calculate_hw(const std::vector<int64_t>& state) const;
};

} // namespace fast_vcd
