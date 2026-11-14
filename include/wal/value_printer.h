#pragma once

#include "ast.h"
#include <iostream>
#include <ostream>
#include <cmath>

namespace wal {

/**
 * @brief Render a Value the way the REPL and `-e` both display results.
 *
 * This is the single implementation behind REPL::print_output and the CLI's
 * `-e` result printing. It used to be duplicated: the REPL copy handled lists
 * and symbols, the `-e` copy stopped at strings, so `wal -e '(signal-names)'`
 * printed nothing at all while the same expression printed fine in the REPL.
 *
 * @param result  the value to render
 * @param os      stream to write to
 * @param quote_strings  wrap a *top-level* string in double quotes (REPL style).
 *        Strings nested inside a list are always quoted, so that a printed list
 *        reads back as the list it came from. The CLI passes false to keep
 *        `wal -e '(concat "a" "b")'` emitting bare text that a shell can consume.
 *
 * nil is NOT handled here, because the two callers disagree on purpose: the REPL
 * stays silent (nil is what `print` returns, and echoing it would double every
 * line), while `-e` emits "nil" so a script can tell "evaluated to nothing" from
 * "produced no output". Each caller checks is_nil() before calling this.
 */
inline void print_value(const Value& result, std::ostream& os, bool quote_strings) {
    if (result.is_bool()) {
        os << (result.to_bool() ? "true" : "false");
    } else if (result.is_int()) {
        os << result.to_int();
    } else if (result.is_float()) {
        // A whole float prints with its ".0" so a float is distinguishable from an
        // int, as upstream's output is: `(average (list 1 2 3))` is 2.0, not 2.
        double value = result.to_float();
        os << value;
        if (std::isfinite(value) && value == static_cast<int64_t>(value)) {
            os << ".0";
        }
    } else if (result.is_string()) {
        if (quote_strings) {
            os << "\"" << result.to_string() << "\"";
        } else {
            os << result.to_string();
        }
    } else if (result.is_symbol()) {
        os << "'" << result.to_symbol()->get_name();
    } else if (result.is_list()) {
        auto list = result.to_list();
        os << "(";
        for (size_t i = 0; i < list->size(); ++i) {
            if (i > 0) os << " ";
            // Elements are always quoted/prefixed, so the output re-reads as a list.
            print_value((*list)[i], os, true);
        }
        os << ")";
    } else if (result.is_bigint()) {
        os << result.to_bigint()->to_string();
    } else if (result.is_array()) {
        os << result.to_string_repr();
    } else if (result.is_nil()) {
        os << "nil";
    } else {
        os << "<#value>";
    }
}

} // namespace wal
