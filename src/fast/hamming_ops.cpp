#include "wal/fast/hamming_ops.h"
#include <algorithm>
#include <iostream>
#include <tuple>
#include <unordered_map>

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

namespace {
inline int64_t popcount64(int64_t v) {
    return __builtin_popcountll(static_cast<uint64_t>(v));
}

// Wide (>64-bit) signal support: values are stored as fixed-size word vectors
// (see DeltaTimestamp::wide_changes), so weight/distance are just a popcount
// (XOR-popcount for distance) summed word by word.
int64_t popcount_words(const std::vector<uint64_t>& words) {
    int64_t total = 0;
    for (uint64_t w : words) total += __builtin_popcountll(w);
    return total;
}

int64_t popcount_xor_words(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b) {
    size_t n = std::max(a.size(), b.size());
    int64_t total = 0;
    for (size_t i = 0; i < n; ++i) {
        uint64_t wa = i < a.size() ? a[i] : 0;
        uint64_t wb = i < b.size() ? b[i] : 0;
        total += __builtin_popcountll(wa ^ wb);
    }
    return total;
}
}  // namespace

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

    // Wide (>64-bit) signals, tracked separately by signal id (see hamming_ops.h).
    std::unordered_map<uint32_t, std::vector<uint64_t>> wide_state;
    for (const auto& [sid, words] : parser_.get_initial_wide_values()) {
        if (sig_id_to_idx.count(sid)) wide_state[sid] = words;
    }
    for (const auto& [sig_id, words] : deltas[0].wide_changes) {
        if (sig_id_to_idx.count(sig_id)) wide_state[sig_id] = words;
    }

    // Emit HD from delta[1] onward. Unchanged signals contribute popcount(0)=0
    // to the XOR-based distance regardless, so only the signals actually
    // touched by this timestamp's delta need to be diffed -- no need to scan
    // (or copy) the full state vector per timestamp, which is what made this
    // O(timestamps * scope_size) before instead of O(timestamps * changes).
    for (size_t i = 1; i < deltas.size(); i++) {
        int64_t local_hd = 0;
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) {
                size_t idx = it->second;
                local_hd += popcount64(curr_state[idx] ^ value);
                curr_state[idx] = value;
            }
        }
        for (const auto& [sig_id, words] : deltas[i].wide_changes) {
            if (sig_id_to_idx.count(sig_id)) {
                auto wit = wide_state.find(sig_id);
                if (wit != wide_state.end()) {
                    local_hd += popcount_xor_words(wit->second, words);
                    wit->second = words;
                } else {
                    local_hd += popcount_words(words);  // implicit previous value: all-zero
                    wide_state[sig_id] = words;
                }
            }
        }
        results.emplace_back(deltas[i].timestamp, local_hd);
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

    // See hamming_distance_all() for the wide-signal (>64-bit) handling too.
    std::unordered_map<uint32_t, std::vector<uint64_t>> wide_state;
    for (const auto& [sid, words] : parser_.get_initial_wide_values()) {
        if (sig_id_to_idx.count(sid)) wide_state[sid] = words;
    }
    for (const auto& [sig_id, words] : deltas[0].wide_changes) {
        if (sig_id_to_idx.count(sig_id)) wide_state[sig_id] = words;
    }

    // See hamming_distance_all() for why this only touches changed signals.
    for (size_t i = 1; i < deltas.size(); i++) {
        int64_t local_hd = 0;
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) {
                size_t idx = it->second;
                local_hd += popcount64(curr_state[idx] ^ value);
                curr_state[idx] = value;
            }
        }
        for (const auto& [sig_id, words] : deltas[i].wide_changes) {
            if (sig_id_to_idx.count(sig_id)) {
                auto wit = wide_state.find(sig_id);
                if (wit != wide_state.end()) {
                    local_hd += popcount_xor_words(wit->second, words);
                    wit->second = words;
                } else {
                    local_hd += popcount_words(words);
                    wide_state[sig_id] = words;
                }
            }
        }
        results.emplace_back(deltas[i].timestamp, local_hd);
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
    int64_t running_weight = calculate_hw(state);  // one full scan, up front only

    // Wide (>64-bit) signals, tracked separately by signal id (see hamming_ops.h).
    std::unordered_map<uint32_t, std::vector<uint64_t>> wide_state;
    for (const auto& [sid, words] : parser_.get_initial_wide_values()) {
        if (sig_id_to_idx.count(sid)) {
            running_weight += popcount_words(words);
            wide_state[sid] = words;
        }
    }

    // Process each timestamp incrementally: update only the running weight's
    // delta for the specific signals that changed, instead of recomputing
    // popcount over the entire (potentially huge) state vector every
    // timestamp -- this was the O(timestamps * scope_size) hot spot.
    for (size_t i = 0; i < deltas.size(); i++) {
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) {
                size_t idx = it->second;
                running_weight -= popcount64(state[idx]);
                state[idx] = value;
                running_weight += popcount64(value);
            }
        }
        for (const auto& [sig_id, words] : deltas[i].wide_changes) {
            if (sig_id_to_idx.count(sig_id)) {
                auto wit = wide_state.find(sig_id);
                if (wit != wide_state.end()) {
                    running_weight -= popcount_words(wit->second);
                    wit->second = words;
                } else {
                    wide_state[sig_id] = words;
                }
                running_weight += popcount_words(wide_state[sig_id]);
            }
        }

        results.emplace_back(deltas[i].timestamp, running_weight);
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
    int64_t running_weight = calculate_hw(state);

    std::unordered_map<uint32_t, std::vector<uint64_t>> wide_state;
    for (const auto& [sid, words] : parser_.get_initial_wide_values()) {
        if (sig_id_to_idx.count(sid)) {
            running_weight += popcount_words(words);
            wide_state[sid] = words;
        }
    }

    for (size_t i = 0; i < deltas.size(); i++) {
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) {
                size_t idx = it->second;
                running_weight -= popcount64(state[idx]);
                state[idx] = value;
                running_weight += popcount64(value);
            }
        }
        for (const auto& [sig_id, words] : deltas[i].wide_changes) {
            if (sig_id_to_idx.count(sig_id)) {
                auto wit = wide_state.find(sig_id);
                if (wit != wide_state.end()) {
                    running_weight -= popcount_words(wit->second);
                    wit->second = words;
                } else {
                    wide_state[sig_id] = words;
                }
                running_weight += popcount_words(wide_state[sig_id]);
            }
        }
        results.emplace_back(deltas[i].timestamp, running_weight);
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

    // See hamming_distance_all() for the wide-signal (>64-bit) handling too.
    std::unordered_map<uint32_t, std::vector<uint64_t>> wide_state;
    for (const auto& [sid, words] : parser_.get_initial_wide_values()) {
        if (sig_id_to_idx.count(sid)) wide_state[sid] = words;
    }
    for (const auto& [sig_id, words] : deltas[0].wide_changes) {
        if (sig_id_to_idx.count(sig_id)) wide_state[sig_id] = words;
    }

    // See hamming_distance_all() for why this only touches changed signals.
    for (size_t i = 1; i < deltas.size(); i++) {
        int64_t local_hd = 0;
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) {
                size_t idx = it->second;
                local_hd += popcount64(curr_state[idx] ^ value);
                curr_state[idx] = value;
            }
        }
        for (const auto& [sig_id, words] : deltas[i].wide_changes) {
            if (sig_id_to_idx.count(sig_id)) {
                auto wit = wide_state.find(sig_id);
                if (wit != wide_state.end()) {
                    local_hd += popcount_xor_words(wit->second, words);
                    wit->second = words;
                } else {
                    local_hd += popcount_words(words);
                    wide_state[sig_id] = words;
                }
            }
        }
        results.emplace_back(deltas[i].timestamp, local_hd);
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
    int64_t running_weight = calculate_hw(state);

    std::unordered_map<uint32_t, std::vector<uint64_t>> wide_state;
    for (const auto& [sid, words] : parser_.get_initial_wide_values()) {
        if (sig_id_to_idx.count(sid)) {
            running_weight += popcount_words(words);
            wide_state[sid] = words;
        }
    }

    for (size_t i = 0; i < deltas.size(); i++) {
        for (const auto& [sig_id, value] : deltas[i].changes) {
            auto it = sig_id_to_idx.find(sig_id);
            if (it != sig_id_to_idx.end()) {
                size_t idx = it->second;
                running_weight -= popcount64(state[idx]);
                state[idx] = value;
                running_weight += popcount64(value);
            }
        }
        for (const auto& [sig_id, words] : deltas[i].wide_changes) {
            if (sig_id_to_idx.count(sig_id)) {
                auto wit = wide_state.find(sig_id);
                if (wit != wide_state.end()) {
                    running_weight -= popcount_words(wit->second);
                    wit->second = words;
                } else {
                    wide_state[sig_id] = words;
                }
                running_weight += popcount_words(wide_state[sig_id]);
            }
        }
        results.emplace_back(deltas[i].timestamp, running_weight);
    }

    return results;
}

ValueRows HammingOps::values_signals(const std::vector<std::string>& signal_names) const {
    std::vector<uint32_t> signal_ids = parser_.get_signal_ids_by_names(signal_names);
    ValueRows rows;
    if (signal_ids.empty()) return rows;

    // Canonical signal id -> list index (the requested order). Several names may
    // resolve to the same canonical id; first occurrence keeps the index.
    std::unordered_map<uint32_t, uint32_t> id_to_idx;
    id_to_idx.reserve(signal_ids.size());
    for (uint32_t i = 0; i < signal_ids.size(); ++i) {
        id_to_idx.emplace(signal_ids[i], i);
    }

    // Warn about signals >64 bits wide: ValueRows' int64_t column can't hold
    // their full value, so (unlike Hamming Weight/Distance, which use the
    // exact multi-word DeltaTimestamp::wide_changes path) rows for these
    // signals below are truncated to the low 64 bits.
    size_t wide_count = 0;
    for (uint32_t id : signal_ids) {
        if (parser_.is_wide_signal(id)) ++wide_count;
    }
    if (wide_count > 0) {
        std::cerr << "WARNING: " << wide_count << " requested signal(s) wider than 64 bits "
                  << "will be truncated to their low 64 bits in raw value output.\n";
    }

    // Initial values at t=0 for tracked signals.
    const auto& init = parser_.get_initial_values();
    for (const auto& [sid, val] : init) {
        auto it = id_to_idx.find(sid);
        if (it != id_to_idx.end()) rows.emplace_back(0, it->second, val);
    }
    for (const auto& [sid, words] : parser_.get_initial_wide_values()) {
        auto it = id_to_idx.find(sid);
        if (it != id_to_idx.end()) rows.emplace_back(0, it->second, words.empty() ? 0 : static_cast<int64_t>(words[0]));
    }

    // One row per change of a tracked signal (the actual decoded value).
    const auto& deltas = parser_.get_deltas();
    for (const auto& d : deltas) {
        for (const auto& [sig_id, value] : d.changes) {
            auto it = id_to_idx.find(sig_id);
            if (it != id_to_idx.end()) rows.emplace_back(d.timestamp, it->second, value);
        }
        for (const auto& [sig_id, words] : d.wide_changes) {
            auto it = id_to_idx.find(sig_id);
            if (it != id_to_idx.end())
                rows.emplace_back(d.timestamp, it->second, words.empty() ? 0 : static_cast<int64_t>(words[0]));
        }
    }
    return rows;
}

} // namespace fast_vcd
