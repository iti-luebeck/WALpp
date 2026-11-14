#include "wal/bigint.h"

namespace wal {

/**
 * The single definition of the bigint-mode flag.
 *
 * It lives here rather than as an inline function with a local static in the header,
 * because the Python extension module and libwalpp are separate shared libraries. An
 * inline static gave each one its own copy, so `set_bigint_mode(True)` from Python
 * flipped the extension's copy while the parser, compiled into libwalpp, kept reading
 * its own and still refused oversized literals.
 */
std::atomic<bool>& bigint_mode() {
    // Atomic because it is read from the evaluator on every arithmetic operation
    // and may be set from another thread. A relaxed load compiles to an ordinary
    // load on the platforms WAL++ targets, so the hot path does not pay for it;
    // what it buys is that setting the mode is not a data race.
    static std::atomic<bool> enabled{false};
    return enabled;
}

void set_bigint_mode(bool enabled) {
    bigint_mode().store(enabled, std::memory_order_relaxed);
}

/// Single definition, for the same shared-library reason as bigint_mode().
std::atomic<bool>& compat_types_mode() {
    static std::atomic<bool> enabled{false};
    return enabled;
}

void set_compat_types_mode(bool enabled) {
    compat_types_mode().store(enabled, std::memory_order_relaxed);
}

} // namespace wal
