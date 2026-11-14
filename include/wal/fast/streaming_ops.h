#pragma once

#include "vcd_parser.h"
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fast_vcd {

/**
 * @brief Single-pass versions of the extraction kernels.
 *
 * The kernels in HammingOps are already strictly forward: each walks the deltas in
 * order carrying a running state, and never looks backward or ahead. The only thing
 * that forced the whole trace into memory was the PARSER, which built the complete
 * `std::vector<DeltaTimestamp>` before any kernel ran. Peak memory was then roughly
 * three times the file: 137 MB for a 50 MB VCD, and proportionally worse from there.
 *
 * These consume one delta at a time from VCDParser's sink and emit one output row at
 * a time, so memory is O(signals) and independent of how long the trace is. The
 * arithmetic is copied from HammingOps rather than re-derived, so the output is
 * identical; a test asserts that on the same trace.
 *
 * A kernel initialises itself on the first delta rather than in its constructor,
 * because the signal table is only complete once the parser has read the header,
 * which happens after the sink is installed.
 */

/// Where a kernel sends its output: one row of (timestamp, value).
using RowSink = std::function<void(int64_t, int64_t)>;

/// Which signals a kernel follows.
struct TrackedSignals {
    enum class Kind { All, Scope, Names };
    Kind kind = Kind::All;
    std::string scope;                 ///< for Kind::Scope
    std::vector<std::string> names;    ///< for Kind::Names

    static TrackedSignals all() { return {}; }
    static TrackedSignals in_scope(std::string prefix) {
        TrackedSignals t; t.kind = Kind::Scope; t.scope = std::move(prefix); return t;
    }
    static TrackedSignals named(std::vector<std::string> list) {
        TrackedSignals t; t.kind = Kind::Names; t.names = std::move(list); return t;
    }
};

/**
 * @brief Hamming weight or Hamming distance, one timestamp at a time.
 *
 * Weight is the running popcount of the tracked state, emitted at every timestamp.
 * Distance is the popcount of what changed since the previous timestamp, emitted
 * from the SECOND timestamp onward, because the first one only establishes a
 * complete state to compare against.
 */
class StreamingHamming {
public:
    StreamingHamming(const VCDParser& parser, TrackedSignals tracked, bool distance,
                     RowSink sink)
        : parser_(parser), tracked_(std::move(tracked)), distance_(distance),
          sink_(std::move(sink)) {}

    void on_delta(const DeltaTimestamp& delta);

    /// Nothing is buffered, so this exists only to make the lifecycle explicit.
    void finish() {}

    /// How many rows were emitted, for the progress line the CLI prints.
    size_t rows() const { return rows_; }

private:
    void initialise();

    const VCDParser& parser_;
    TrackedSignals tracked_;
    bool distance_;
    RowSink sink_;

    bool ready_ = false;
    bool seen_first_ = false;            ///< distance mode skips the first timestamp
    size_t rows_ = 0;

    std::unordered_map<uint32_t, size_t> index_of_;   ///< signal id -> slot in state_
    std::vector<int64_t> state_;
    std::unordered_map<uint32_t, std::vector<uint64_t>> wide_state_;
    int64_t running_ = 0;                ///< weight mode: popcount of the whole state
};

/**
 * @brief Raw value changes for a named list of signals, one row at a time.
 *
 * Emits (timestamp, position in the requested list, value), and only when a tracked
 * signal actually changes, plus its value at the first timestamp. This is what
 * latency and marker analysis reads, where a popcount is not the quantity wanted.
 */
class StreamingValues {
public:
    using ValueSink = std::function<void(int64_t, uint32_t, int64_t)>;

    StreamingValues(const VCDParser& parser, std::vector<std::string> names,
                    ValueSink sink)
        : parser_(parser), names_(std::move(names)), sink_(std::move(sink)) {}

    void on_delta(const DeltaTimestamp& delta);
    void finish() {}
    size_t rows() const { return rows_; }

private:
    void initialise();

    const VCDParser& parser_;
    std::vector<std::string> names_;
    ValueSink sink_;

    bool ready_ = false;
    bool emitted_initial_ = false;
    size_t rows_ = 0;
    std::unordered_map<uint32_t, uint32_t> position_of_;   ///< signal id -> list position
    std::unordered_map<uint32_t, int64_t> last_;           ///< last emitted value
};

} // namespace fast_vcd
