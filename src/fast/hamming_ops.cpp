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
    results.reserve(deltas.size());

    // Map from signal_id to index in state vector
    std::unordered_map<uint32_t, size_t> sig_id_to_idx;
    sig_id_to_idx.reserve(signal_ids.size());
    for (size_t i = 0; i < signal_ids.size(); i++) {
        sig_id_to_idx[signal_ids[i]] = i;
    }

    // Seed state from $dumpvars initial values, then apply delta[0] to form
    // the first complete state.  HD is emitted from delta[1] onward
    // (distance between consecutive complete states), matching interpreter semantics.
    const auto& init = parser_.get_initial_values();
    std::vector<int64_t> curr_state(signal_ids.size(), 0);
    for (const auto& [sid, val] : init) {
        auto it = sig_id_to_idx.find(sid);
        if (it != sig_id_to_idx.end()) curr_state[it->second] = val;
    }

    // Apply first delta → complete state at the first timestamp
    for (const auto& [sig_id, value] : deltas[0].changes) {
        auto it = sig_id_to_idx.find(sig_id);
        if (it != sig_id_to_idx.end()) curr_state[it->second] = value;
    }
    std::vector<int64_t> prev_state = curr_state;

    // Emit HD from delta[1] onward
    for (size_t i = 1; i < deltas.size(); i++) {
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) {
                curr_state[it->second] = value;
            }
        }

        results.emplace_back(deltas[i].timestamp, calculate_hd(prev_state, curr_state));
        prev_state = curr_state;
    }

    return results;
}

Result HammingOps::hamming_distance_scope(const std::string& scope_prefix) const {
    std::vector<uint32_t> scoped_signal_ids = parser_.get_signals_in_scope(scope_prefix);
    if (scoped_signal_ids.empty()) return {};

    const auto& deltas = parser_.get_deltas();
    Result results;
    results.reserve(deltas.size());

    std::unordered_map<uint32_t, size_t> sig_id_to_idx;
    sig_id_to_idx.reserve(scoped_signal_ids.size());
    for (size_t i = 0; i < scoped_signal_ids.size(); i++) {
        sig_id_to_idx[scoped_signal_ids[i]] = i;
    }

    const auto& init = parser_.get_initial_values();
    std::vector<int64_t> curr_state(scoped_signal_ids.size(), 0);
    for (const auto& [sid, val] : init) {
        auto it = sig_id_to_idx.find(sid);
        if (it != sig_id_to_idx.end()) curr_state[it->second] = val;
    }

    // Apply first delta → complete initial state
    for (const auto& [sig_id, value] : deltas[0].changes) {
        auto it = sig_id_to_idx.find(sig_id);
        if (it != sig_id_to_idx.end()) curr_state[it->second] = value;
    }
    std::vector<int64_t> prev_state = curr_state;

    for (size_t i = 1; i < deltas.size(); i++) {
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) curr_state[it->second] = value;
        }
        results.emplace_back(deltas[i].timestamp, calculate_hd(prev_state, curr_state));
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

    // Seed state from $dumpvars initial values
    const auto& init = parser_.get_initial_values();
    std::vector<int64_t> state(signal_ids.size(), 0);
    for (const auto& [sid, val] : init) {
        auto it = sig_id_to_idx.find(sid);
        if (it != sig_id_to_idx.end()) state[it->second] = val;
    }

    // Process each timestamp incrementally
    for (size_t i = 0; i < deltas.size(); i++) {
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) {
                state[it->second] = value;
            }
        }

        results.emplace_back(deltas[i].timestamp, calculate_hw(state));
    }

    return results;
}

Result HammingOps::hamming_weight_scope(const std::string& scope_prefix) const {
    std::vector<uint32_t> scoped_signal_ids = parser_.get_signals_in_scope(scope_prefix);
    if (scoped_signal_ids.empty()) return {};

    const auto& deltas = parser_.get_deltas();
    Result results;
    results.reserve(deltas.size());

    std::unordered_map<uint32_t, size_t> sig_id_to_idx;
    sig_id_to_idx.reserve(scoped_signal_ids.size());
    for (size_t i = 0; i < scoped_signal_ids.size(); i++) {
        sig_id_to_idx[scoped_signal_ids[i]] = i;
    }

    const auto& init = parser_.get_initial_values();
    std::vector<int64_t> state(scoped_signal_ids.size(), 0);
    for (const auto& [sid, val] : init) {
        auto it = sig_id_to_idx.find(sid);
        if (it != sig_id_to_idx.end()) state[it->second] = val;
    }

    for (size_t i = 0; i < deltas.size(); i++) {
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) state[it->second] = value;
        }
        results.emplace_back(deltas[i].timestamp, calculate_hw(state));
    }

    return results;
}

Result HammingOps::hamming_distance_signals(const std::vector<std::string>& signal_names) const {
    std::vector<uint32_t> signal_ids = parser_.get_signal_ids_by_names(signal_names);
    if (signal_ids.empty()) return {};

    const auto& deltas = parser_.get_deltas();
    Result results;
    results.reserve(deltas.size());

    std::unordered_map<uint32_t, size_t> sig_id_to_idx;
    sig_id_to_idx.reserve(signal_ids.size());
    for (size_t i = 0; i < signal_ids.size(); i++) {
        sig_id_to_idx[signal_ids[i]] = i;
    }

    const auto& init = parser_.get_initial_values();
    std::vector<int64_t> curr_state(signal_ids.size(), 0);
    for (const auto& [sid, val] : init) {
        auto it = sig_id_to_idx.find(sid);
        if (it != sig_id_to_idx.end()) curr_state[it->second] = val;
    }

    // Apply first delta → complete initial state
    for (const auto& [sig_id, value] : deltas[0].changes) {
        auto it = sig_id_to_idx.find(sig_id);
        if (it != sig_id_to_idx.end()) curr_state[it->second] = value;
    }
    std::vector<int64_t> prev_state = curr_state;

    for (size_t i = 1; i < deltas.size(); i++) {
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) curr_state[it->second] = value;
        }
        results.emplace_back(deltas[i].timestamp, calculate_hd(prev_state, curr_state));
        prev_state = curr_state;
    }

    return results;
}

Result HammingOps::hamming_weight_signals(const std::vector<std::string>& signal_names) const {
    std::vector<uint32_t> signal_ids = parser_.get_signal_ids_by_names(signal_names);
    if (signal_ids.empty()) return {};

    const auto& deltas = parser_.get_deltas();
    Result results;
    results.reserve(deltas.size());

    std::unordered_map<uint32_t, size_t> sig_id_to_idx;
    sig_id_to_idx.reserve(signal_ids.size());
    for (size_t i = 0; i < signal_ids.size(); i++) {
        sig_id_to_idx[signal_ids[i]] = i;
    }

    const auto& init = parser_.get_initial_values();
    std::vector<int64_t> state(signal_ids.size(), 0);
    for (const auto& [sid, val] : init) {
        auto it = sig_id_to_idx.find(sid);
        if (it != sig_id_to_idx.end()) state[it->second] = val;
    }

    for (size_t i = 0; i < deltas.size(); i++) {
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) state[it->second] = value;
        }
        results.emplace_back(deltas[i].timestamp, calculate_hw(state));
    }

    return results;
}

} // namespace fast_vcd
