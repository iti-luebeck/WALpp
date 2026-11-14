#pragma once

#include "ast.h"
#include <iostream>
#include <ostream>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace wal {

/**
 * @brief The shortest decimal that reads back as the same double.
 *
 * Which is what Python's repr produces and therefore what upstream WAL prints.
 * Three renderers in this binary each had their own precision: `print` truncated
 * at 10 significant digits, `-e` at the stream default of 6, so `(/ 10 3)` printed
 * 3.333333333 one way and 3.33333 the other, both short of the value held. They all
 * come here now.
 */
inline std::string format_double(double value) {
    std::ostringstream oss;
    for (int digits = 15; digits <= 17; ++digits) {
        oss.str("");
        oss.clear();
        oss << std::setprecision(digits) << value;
        double back = 0.0;
        std::istringstream in(oss.str());
        if ((in >> back) && back == value) {
            return oss.str();
        }
    }
    return oss.str();
}

/**
 * @brief Render a Value the way `print` does: for a person to read.
 *
 * Upstream's `print` is `print(*[x if isinstance(x, str) else wal_str(x) ...], sep='')`,
 * so a string contributes its own characters with no quotes at any depth, and the
 * arguments run together with no separator. WAL++ used to quote strings and insert a
 * space, which made the ordinary labelled line `(print "Total: " n)` come out as
 * `"Total: " 5`.
 *
 * `-e` and the REPL still use print_value below, which quotes, because there the
 * output is a value being shown rather than text being emitted.
 */
inline void display_value(const Value& result, std::ostream& os,
                          bool top_level = true) {
    if (result.is_string()) {
        // Only an argument of `print` itself contributes its characters bare. A
        // string reached through a list or an array is quoted, so that a printed
        // collection still shows which of its elements are strings. That is the
        // split in upstream's op_print, which special-cases the top level and sends
        // everything else through wal_str.
        if (top_level) {
            os << result.to_string();
        } else {
            os << "\"" << result.to_string() << "\"";
        }
    } else if (result.is_symbol()) {
        // Bare at every level: upstream's wal_str renders a Symbol as its name.
        os << result.to_symbol()->get_name();
    } else if (result.is_list()) {
        auto list = result.to_list();
        os << "(";
        for (size_t i = 0; i < list->size(); ++i) {
            if (i > 0) os << " ";
            display_value((*list)[i], os, /*top_level=*/false);
        }
        os << ")";
    } else if (result.is_array()) {
        auto array = result.to_array();
        os << "{";
        bool first = true;
        for (const auto& entry : array->entries()) {
            if (!first) os << " ";
            first = false;
            // A key is always a string, and is always shown as one.
            os << "(\"" << entry.first << "\" ";
            display_value(entry.second, os, /*top_level=*/false);
            os << ")";
        }
        os << "}";
    } else {
        os << result.to_string_repr();
    }
}

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
        os << format_double(value);
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
