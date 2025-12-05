#include "wal/fast/hamming_ops.h"
#include <algorithm>

namespace fast_vcd {

void HammingOps::reconstruct_state(
    size_t delta_idx,
    const std::vector<uint32_t>& signal_ids,
    std::vector<int64_t>& state
) const {
    const auto& deltas = parser_.get_deltas();
    
    // Start with zeros
    std::fill(state.begin(), state.end(), 0);
    
    // Apply all deltas up to and including delta_idx
    for (size_t i = 0; i <= delta_idx; i++) {
        for (const auto& [sig_id, value] : deltas[i].changes) {
            // Find position in signal_ids
            auto it = std::find(signal_ids.begin(), signal_ids.end(), sig_id);
            if (it != signal_ids.end()) {
                size_t idx = it - signal_ids.begin();
                state[idx] = value;
            }
        }
    }
}

int64_t HammingOps::calculate_hd(
    const std::vector<int64_t>& prev_state,
    const std::vector<int64_t>& curr_state
) const {
    int64_t distance = 0;
    
    for (size_t i = 0; i < prev_state.size(); i++) {
        uint64_t diff = static_cast<uint64_t>(prev_state[i]) ^ 
                        static_cast<uint64_t>(curr_state[i]);
        distance += __builtin_popcountll(diff);
    }
    
    return distance;
}

int64_t HammingOps::calculate_hw(const std::vector<int64_t>& state) const {
    int64_t weight = 0;
    
    for (const auto& value : state) {
        if (value > 0) {
            weight += __builtin_popcountll(static_cast<uint64_t>(value));
        } else if (value < 0) {
            weight += __builtin_popcountll(static_cast<uint64_t>(value));
        }
    }
    
    return weight;
}

Result HammingOps::hamming_distance_all() const {
    const auto& signal_ids = parser_.get_signal_ids();
    const auto& deltas = parser_.get_deltas();
    
    if (deltas.empty() || signal_ids.empty()) {
        return {};
    }
    
    Result results;
    results.reserve(deltas.size() - 1);
    
    // Map from signal_id to index in state vector
    std::unordered_map<uint32_t, size_t> sig_id_to_idx;
    sig_id_to_idx.reserve(signal_ids.size());
    for (size_t i = 0; i < signal_ids.size(); i++) {
        sig_id_to_idx[signal_ids[i]] = i;
    }
    
    // Allocate state vectors
    std::vector<int64_t> prev_state(signal_ids.size(), 0);
    std::vector<int64_t> curr_state(signal_ids.size(), 0);
    
    // Apply first delta
    for (const auto& [sig_id, value] : deltas[0].changes) {
        auto it = sig_id_to_idx.find(sig_id);
        if (it != sig_id_to_idx.end()) {
            curr_state[it->second] = value;
        }
    }
    
    prev_state = curr_state;
    
    // Process remaining timestamps incrementally
    for (size_t i = 1; i < deltas.size(); i++) {
        // Apply delta to current state (incremental update!)
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) {
                curr_state[it->second] = value;
            }
        }
        
        int64_t distance = calculate_hd(prev_state, curr_state);
        results.emplace_back(deltas[i].timestamp, distance);
        
        prev_state = curr_state;
    }
    
    return results;
}

Result HammingOps::hamming_distance_scope(const std::string& scope_prefix) const {
    // Get signals in scope
    std::vector<uint32_t> scoped_signal_ids = parser_.get_signals_in_scope(scope_prefix);
    
    if (scoped_signal_ids.empty()) {
        return {};
    }
    
    const auto& deltas = parser_.get_deltas();
    Result results;
    results.reserve(deltas.size() - 1);
    
    // Map from signal_id to index in state vector
    std::unordered_map<uint32_t, size_t> sig_id_to_idx;
    sig_id_to_idx.reserve(scoped_signal_ids.size());
    for (size_t i = 0; i < scoped_signal_ids.size(); i++) {
        sig_id_to_idx[scoped_signal_ids[i]] = i;
    }
    
    // Allocate state vectors
    std::vector<int64_t> prev_state(scoped_signal_ids.size(), 0);
    std::vector<int64_t> curr_state(scoped_signal_ids.size(), 0);
    
    // Apply first delta
    for (const auto& [sig_id, value] : deltas[0].changes) {
        auto it = sig_id_to_idx.find(sig_id);
        if (it != sig_id_to_idx.end()) {
            curr_state[it->second] = value;
        }
    }
    
    prev_state = curr_state;
    
    // Process remaining timestamps incrementally
    for (size_t i = 1; i < deltas.size(); i++) {
        // Apply delta to current state (incremental update!)
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) {
                curr_state[it->second] = value;
            }
        }
        
        int64_t distance = calculate_hd(prev_state, curr_state);
        results.emplace_back(deltas[i].timestamp, distance);
        
        prev_state = curr_state;
    }
    
    return results;
}

Result HammingOps::hamming_weight_all() const {
    const auto& signal_ids = parser_.get_signal_ids();
    const auto& deltas = parser_.get_deltas();
    
    if (deltas.empty() || signal_ids.empty()) {
        return {};
    }
    
    Result results;
    results.reserve(deltas.size());
    
    // Map from signal_id to index in state vector
    std::unordered_map<uint32_t, size_t> sig_id_to_idx;
    sig_id_to_idx.reserve(signal_ids.size());
    for (size_t i = 0; i < signal_ids.size(); i++) {
        sig_id_to_idx[signal_ids[i]] = i;
    }
    
    // Allocate state vector
    std::vector<int64_t> state(signal_ids.size(), 0);
    
    // Process each timestamp incrementally
    for (size_t i = 0; i < deltas.size(); i++) {
        // Apply delta to current state
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) {
                state[it->second] = value;
            }
        }
        
        int64_t weight = calculate_hw(state);
        results.emplace_back(deltas[i].timestamp, weight);
    }
    
    return results;
}

Result HammingOps::hamming_weight_scope(const std::string& scope_prefix) const {
    // Get signals in scope
    std::vector<uint32_t> scoped_signal_ids = parser_.get_signals_in_scope(scope_prefix);
    
    if (scoped_signal_ids.empty()) {
        return {};
    }
    
    const auto& deltas = parser_.get_deltas();
    Result results;
    results.reserve(deltas.size());
    
    // Map from signal_id to index in state vector
    std::unordered_map<uint32_t, size_t> sig_id_to_idx;
    sig_id_to_idx.reserve(scoped_signal_ids.size());
    for (size_t i = 0; i < scoped_signal_ids.size(); i++) {
        sig_id_to_idx[scoped_signal_ids[i]] = i;
    }
    
    // Allocate state vector
    std::vector<int64_t> state(scoped_signal_ids.size(), 0);
    
    // Process each timestamp incrementally
    for (size_t i = 0; i < deltas.size(); i++) {
        // Apply delta to current state
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) {
                state[it->second] = value;
            }
        }
        
        int64_t weight = calculate_hw(state);
        results.emplace_back(deltas[i].timestamp, weight);
    }
    
    return results;
}

} // namespace fast_vcd
