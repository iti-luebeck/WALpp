#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <functional>

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
    std::vector<std::pair<uint32_t, int64_t>> changes;  // (signal_id, new_value), signals <=64 bits wide

    // Signals >64 bits wide, stored as fixed-size word vectors (low word
    // first) since a single int64_t can't hold their full value. Kept in a
    // separate vector so the hot (>=99% of signals) narrow path above is
    // completely unaffected in layout, size, or iteration cost.
    std::vector<std::pair<uint32_t, std::vector<uint64_t>>> wide_changes;

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
     * @brief Parse FST file using GTKWave fstapi
     * @param filename Path to FST file
     * @param scope_filter Optional scope prefix for filtering (e.g., "top.cpu")
     */
    void parse_fst(const std::string& filename, const std::string& scope_filter = "");
    
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
     *
     * Empty when a sink is installed: see set_delta_sink().
     */
    const std::vector<DeltaTimestamp>& get_deltas() const {
        return deltas_;
    }

    /// Receives each timestamp's changes as they are parsed.
    using DeltaSink = std::function<void(const DeltaTimestamp&)>;

    /**
     * @brief Stream deltas to @p sink instead of accumulating them.
     *
     * Without a sink the parser builds the complete `deltas_` vector before any
     * consumer runs, so peak memory grows with the trace: about three times the
     * file, measured at 137 MB for a 50 MB VCD. Every extraction kernel walks that
     * vector strictly forward, carrying a running state, so none of them needs it
     * to exist. With a sink installed each timestamp is handed over and dropped,
     * and memory becomes O(signals), independent of how long the trace is.
     *
     * Install it before parse(). The header is read first, so by the time the first
     * delta arrives the signal table is complete and the sink can size itself.
     */
    void set_delta_sink(DeltaSink sink) { delta_sink_ = std::move(sink); }

    /// Whether deltas are being streamed rather than stored.
    bool streaming() const { return static_cast<bool>(delta_sink_); }

    /**
     * @brief Get initial signal values from $dumpvars / FST time-0 callback
     * Maps signal_id -> initial value. Signals absent have implicit value 0.
     * Only covers signals <=64 bits wide; see get_initial_wide_values() for wider ones.
     */
    const std::unordered_map<uint32_t, int64_t>& get_initial_values() const {
        return initial_values_;
    }

    /**
     * @brief Get initial values for signals >64 bits wide (see get_initial_values()).
     * Maps signal_id -> word vector (low word first).
     */
    const std::unordered_map<uint32_t, std::vector<uint64_t>>& get_initial_wide_values() const {
        return initial_wide_values_;
    }

    /**
     * @brief Whether a signal's declared width exceeds 64 bits (see DeltaTimestamp::wide_changes).
     */
    bool is_wide_signal(uint32_t signal_id) const {
        return signals_[signal_id].width > 64;
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
    
    /**
     * @brief Get signal IDs from a list of signal names
     * @param signal_names Vector of full hierarchical signal names
     * @return Vector of signal IDs (signals not found are skipped)
     */
    std::vector<uint32_t> get_signal_ids_by_names(const std::vector<std::string>& signal_names) const;

    /** @brief Trace timescale, e.g. "1ps" / "10ns" (default "1ns" if absent). */
    const std::string& get_timescale() const { return timescale_; }

private:
    void parse_header(const char* data, const char* end, size_t& pos);
    void parse_data(const char* data, const char* end, size_t& pos);

    std::string timescale_ = "1ns";             // From $timescale / FST metadata
    std::vector<SignalInfo> signals_;           // All signal metadata
    std::vector<uint32_t> signal_ids_;          // Active signal IDs (after filtering)
    std::unordered_map<std::string, uint32_t> vcd_id_to_signal_;  // VCD ID -> signal ID
    std::vector<DeltaTimestamp> deltas_;        // Delta-encoded timestamps (empty while streaming)
    DeltaSink delta_sink_;                      // see set_delta_sink()
    DeltaTimestamp pending_;                    // the delta being filled while streaming
    bool has_pending_ = false;
    std::unordered_set<std::string> filtered_vcd_ids_;  // VCD IDs to keep (if filtered)
    std::unordered_map<uint32_t, int64_t> initial_values_;  // signal_id -> value at t=0 (<=64 bits)
    std::unordered_map<uint32_t, std::vector<uint64_t>> initial_wide_values_;  // signal_id -> words at t=0 (>64 bits)

    uint32_t next_signal_id_ = 0;
};

} // namespace fast_vcd
