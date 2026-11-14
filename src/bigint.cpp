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
bool& bigint_mode() {
    static bool enabled = false;
    return enabled;
}

void set_bigint_mode(bool enabled) { bigint_mode() = enabled; }

/// Single definition, for the same shared-library reason as bigint_mode().
bool& compat_types_mode() {
    static bool enabled = false;
    return enabled;
}

void set_compat_types_mode(bool enabled) { compat_types_mode() = enabled; }

} // namespace wal
