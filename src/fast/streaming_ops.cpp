#include "wal/fast/streaming_ops.h"
#include <algorithm>
#include <iostream>

namespace fast_vcd {

namespace {

inline int64_t popcount64(int64_t v) {
    return __builtin_popcountll(static_cast<uint64_t>(v));
}

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

} // namespace

// ---------------------------------------------------------------------------
// StreamingHamming
// ---------------------------------------------------------------------------

void StreamingHamming::initialise() {
    std::vector<uint32_t> ids;
    switch (tracked_.kind) {
        case TrackedSignals::Kind::All:
            ids = parser_.get_signal_ids();
            break;
        case TrackedSignals::Kind::Scope:
            ids = parser_.get_signals_in_scope(tracked_.scope);
            break;
        case TrackedSignals::Kind::Names:
            ids = parser_.get_signal_ids_by_names(tracked_.names);
            break;
    }

    index_of_.reserve(ids.size());
    for (size_t i = 0; i < ids.size(); ++i) {
        index_of_[ids[i]] = i;
    }
    state_.assign(ids.size(), 0);

    // Seed from the $dumpvars values, exactly as the in-memory kernels do.
    for (const auto& [sid, val] : parser_.get_initial_values()) {
        auto it = index_of_.find(sid);
        if (it != index_of_.end()) state_[it->second] = val;
    }
    for (const auto& [sid, words] : parser_.get_initial_wide_values()) {
        if (index_of_.count(sid)) wide_state_[sid] = words;
    }

    if (!distance_) {
        // Weight carries a running total, so it needs one full scan up front.
        running_ = 0;
        for (int64_t v : state_) running_ += popcount64(v);
        for (const auto& [sid, words] : wide_state_) {
            (void)sid;
            running_ += popcount_words(words);
        }
    }
    ready_ = true;
}

void StreamingHamming::on_delta(const DeltaTimestamp& delta) {
    if (!ready_) initialise();
    if (index_of_.empty()) return;

    if (distance_) {
        // The first timestamp only completes the state to compare against, so no
        // row is emitted for it. From the second onward, only the signals this
        // delta touched can contribute: an unchanged signal XORs to zero.
        int64_t local = 0;
        for (const auto& [sig_id, value] : delta.changes) {
            auto it = index_of_.find(sig_id);
            if (it != index_of_.end()) {
                size_t idx = it->second;
                local += popcount64(state_[idx] ^ value);
                state_[idx] = value;
            }
        }
        for (const auto& [sig_id, words] : delta.wide_changes) {
            if (index_of_.count(sig_id)) {
                auto wit = wide_state_.find(sig_id);
                if (wit != wide_state_.end()) {
                    local += popcount_xor_words(wit->second, words);
                    wit->second = words;
                } else {
                    local += popcount_words(words);   // implicit previous value: all-zero
                    wide_state_[sig_id] = words;
                }
            }
        }
        if (!seen_first_) {
            seen_first_ = true;
            return;
        }
        sink_(delta.timestamp, local);
        ++rows_;
        return;
    }

    // Weight: adjust the running total by only what changed.
    for (const auto& [sig_id, value] : delta.changes) {
        auto it = index_of_.find(sig_id);
        if (it != index_of_.end()) {
            size_t idx = it->second;
            running_ -= popcount64(state_[idx]);
            state_[idx] = value;
            running_ += popcount64(value);
        }
    }
    for (const auto& [sig_id, words] : delta.wide_changes) {
        if (index_of_.count(sig_id)) {
            auto wit = wide_state_.find(sig_id);
            if (wit != wide_state_.end()) {
                running_ -= popcount_words(wit->second);
                wit->second = words;
            } else {
                wide_state_[sig_id] = words;
            }
            running_ += popcount_words(wide_state_[sig_id]);
        }
    }
    sink_(delta.timestamp, running_);
    ++rows_;
}

// ---------------------------------------------------------------------------
// StreamingValues
// ---------------------------------------------------------------------------

void StreamingValues::initialise() {
    auto ids = parser_.get_signal_ids_by_names(names_);
    position_of_.reserve(ids.size());
    // Several names may resolve to the same canonical id; the first keeps the slot.
    for (uint32_t i = 0; i < ids.size(); ++i) {
        position_of_.emplace(ids[i], i);
    }

    size_t wide_count = 0;
    for (uint32_t id : ids) {
        if (parser_.is_wide_signal(id)) ++wide_count;
    }
    if (wide_count > 0) {
        std::cerr << "WARNING: " << wide_count << " requested signal(s) wider than 64 bits "
                  << "will be truncated to their low 64 bits in raw value output.\n";
    }
    ready_ = true;
}

void StreamingValues::on_delta(const DeltaTimestamp& delta) {
    if (!ready_) initialise();
    if (position_of_.empty()) return;

    if (!emitted_initial_) {
        emitted_initial_ = true;
        for (const auto& [sid, val] : parser_.get_initial_values()) {
            auto it = position_of_.find(sid);
            if (it != position_of_.end()) {
                sink_(0, it->second, val);
                last_[sid] = val;
                ++rows_;
            }
        }
        for (const auto& [sid, words] : parser_.get_initial_wide_values()) {
            auto it = position_of_.find(sid);
            if (it != position_of_.end()) {
                int64_t low = words.empty() ? 0 : static_cast<int64_t>(words[0]);
                sink_(0, it->second, low);
                last_[sid] = low;
                ++rows_;
            }
        }
    }

    for (const auto& [sig_id, value] : delta.changes) {
        auto it = position_of_.find(sig_id);
        if (it != position_of_.end()) {
            sink_(delta.timestamp, it->second, value);
            last_[sig_id] = value;
            ++rows_;
        }
    }
    for (const auto& [sig_id, words] : delta.wide_changes) {
        auto it = position_of_.find(sig_id);
        if (it != position_of_.end()) {
            int64_t low = words.empty() ? 0 : static_cast<int64_t>(words[0]);
            sink_(delta.timestamp, it->second, low);
            last_[sig_id] = low;
            ++rows_;
        }
    }
}

} // namespace fast_vcd
