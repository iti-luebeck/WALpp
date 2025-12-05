#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace fast_vcd {

/**
 * @brief Signal metadata
 */
struct SignalInfo {
    uint32_t id;           // Numeric ID for fast indexing
    std::string name;      // Full hierarchical name
    std::string vcd_id;    // VCD identifier (!, ", #, etc.)
    std::string scope;     // Hierarchical scope
    int width;             // Bit width
};

/**
 * @brief Delta-encoded timestamp (stores only changes)
 */
struct DeltaTimestamp {
    int64_t timestamp;
    std::vector<std::pair<uint32_t, int64_t>> changes;  // (signal_id, new_value)
    
    DeltaTimestamp() = default;
    DeltaTimestamp(int64_t ts) : timestamp(ts) {}
};

/**
 * @brief Fast VCD parser with delta compression
 */
class VCDParser {
public:
    VCDParser() = default;
    
    /**
     * @brief Parse VCD file
     * @param filename Path to VCD file
     * @param scope_filter Optional scope prefix for filtering (e.g., "top.cpu")
     */
    void parse(const std::string& filename, const std::string& scope_filter = "");
    
    /**
     * @brief Get signal info by ID
     */
    const SignalInfo& get_signal_info(uint32_t signal_id) const {
        return signals_[signal_id];
    }
    
    /**
     * @brief Get all signal IDs
     */
    const std::vector<uint32_t>& get_signal_ids() const {
        return signal_ids_;
    }
    
    /**
     * @brief Get delta timestamps
     */
    const std::vector<DeltaTimestamp>& get_deltas() const {
        return deltas_;
    }
    
    /**
     * @brief Get number of signals
     */
    size_t num_signals() const {
        return signals_.size();
    }
    
    /**
     * @brief Get number of timestamps
     */
    size_t num_timestamps() const {
        return deltas_.size();
    }
    
    /**
     * @brief Get signals in a specific scope
     */
    std::vector<uint32_t> get_signals_in_scope(const std::string& scope_prefix) const;
    
private:
    void parse_header(const char* data, const char* end, size_t& pos);
    void parse_data(const char* data, const char* end, size_t& pos);
    
    std::vector<SignalInfo> signals_;           // All signal metadata
    std::vector<uint32_t> signal_ids_;          // Active signal IDs (after filtering)
    std::unordered_map<std::string, uint32_t> vcd_id_to_signal_;  // VCD ID -> signal ID
    std::vector<DeltaTimestamp> deltas_;        // Delta-encoded timestamps
    std::unordered_set<std::string> filtered_vcd_ids_;  // VCD IDs to keep (if filtered)
    
    uint32_t next_signal_id_ = 0;
};

} // namespace fast_vcd
