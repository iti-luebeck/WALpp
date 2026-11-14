#include "wal/trace/trace.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include "wal/ast.h"

namespace wal {

std::vector<std::string> Trace::get_scopes() const {
    if (!scope_names.empty()) {
        return scope_names;
    }
    // A reader that records signal names but not scope names still has the
    // hierarchy: it is the set of prefixes of the signal names. Deriving it here
    // means every reader reports SCOPES, rather than only the ones that happen to
    // keep a separate list.
    std::vector<std::string> derived;
    for (const auto& signal : get_all_signals()) {
        size_t cut = 0;
        while ((cut = signal.find(SCOPE_SEPARATOR, cut)) != std::string::npos) {
            std::string scope = signal.substr(0, cut);
            if (std::find(derived.begin(), derived.end(), scope) == derived.end()) {
                derived.push_back(scope);
            }
            ++cut;
        }
    }
    std::sort(derived.begin(), derived.end());
    return derived;
}

void Trace::set_index(int idx) {
    if (is_valid_index(idx)) {
        current_index = idx;
    } else {
        throw std::out_of_range("Index out of range: " + std::to_string(idx));
    }
}

bool Trace::step(int steps) {
    int new_index = current_index + steps;
    if (is_valid_index(new_index)) {
        current_index = new_index;
        return true;
    }
    return false;
}

Value Trace::get_special_signal(const std::string& name) {
    if (name == INDEX) {
        return Value(current_index);
    } else if (name == MAX_INDEX) {
        return Value(max_index);
    } else if (name == TIMESTAMP) {
        // The simulation time at the current index, as opposed to the index itself.
        // The base class returns the index; readers that keep real timestamps
        // override get_timestamp().
        return Value(static_cast<int64_t>(get_timestamp()));
    } else if (name == LOCAL_SCOPES || name == LOCAL_SIGNALS) {
        // Both are relative to the evaluator's current scope, which the caller passes
        // in through set_query_scope(). The header has declared these since the
        // beginning; nothing implemented them, so `LOCAL-SCOPES` was an *Undefined
        // variable* even inside an (in-scope ...) body.
        const std::string& scope = query_scope;
        std::string prefix = scope.empty() ? std::string() : scope + SCOPE_SEPARATOR;

        auto directly_under = [&prefix](const std::string& candidate) {
            if (candidate.size() <= prefix.size()) return false;
            if (candidate.compare(0, prefix.size(), prefix) != 0) return false;
            // Nothing further down the hierarchy: "tb.dut.x" is not local to "tb".
            return candidate.find(SCOPE_SEPARATOR, prefix.size()) == std::string::npos;
        };

        auto results = std::make_shared<WList>();
        const auto& pool = (name == LOCAL_SCOPES) ? get_scopes() : get_all_signals();
        for (const auto& candidate : pool) {
            if (directly_under(candidate)) {
                results->push_back(Value(std::make_shared<Symbol>(candidate)));
            }
        }
        return Value(results);
    } else if (name == TRACE_NAME) {
        return Value(std::make_shared<Symbol>(tid));
    } else if (name == TRACE_FILE) {
        return Value(std::make_shared<Symbol>(filename));
    } else if (name == SIGNALS) {
        // Through the VIRTUAL accessor, not the `signal_names` member. Only the
        // full-load readers fill that member; a streaming reader keeps its own
        // index and overrides get_all_signals(), so reading the member directly
        // made SIGNALS report an empty trace whenever streaming was in use.
        auto list = std::make_shared<WList>();
        for (const auto& sig : get_all_signals()) {
            list->push_back(Value(std::make_shared<Symbol>(sig)));
        }
        return Value(list);
    } else if (name == SCOPES) {
        // Virtual accessor, for the same reason as SIGNALS above.
        auto list = std::make_shared<WList>();
        for (const auto& scope : get_scopes()) {
            list->push_back(Value(std::make_shared<Symbol>(scope)));
        }
        return Value(list);
    } else if (name == VIRTUAL_SIGNALS) {
        auto list = std::make_shared<WList>();
        for (const auto& [vsig_name, vsig_value] : virtual_signals) {
            list->push_back(Value(std::make_shared<Symbol>(vsig_name)));
        }
        return Value(list);
    }
    
    throw std::runtime_error("Unknown special signal: " + name);
}


/**
 * @brief Write the trace out as VCD, for `(dump-trace ...)`.
 *
 * Every signal is emitted as a one-value-per-timestamp dump, which is lossless for
 * what a Trace holds: the readers already collapsed a VCD's value-change records
 * into per-index values, so there is no change information left to preserve.
 *
 * Virtual signals are left out: their values are expressions evaluated against a
 * trace, not stored samples, and writing one would mean deciding how to evaluate it
 * here rather than where it was defined.
 */
std::string Trace::dump_vcd(const std::string& path) {
    std::ostringstream out;

    out << "$date\n  written by WAL++\n$end\n";
    out << "$version\n  WAL++ dump-trace\n$end\n";
    out << "$timescale 1ns $end\n";

    // One identifier per signal, in the printable ASCII range VCD uses.
    auto signals = get_all_signals();
    std::unordered_map<std::string, std::string> ids;
    auto identifier = [](size_t n) {
        std::string id;
        do {
            id.insert(id.begin(), static_cast<char>('!' + (n % 94)));
            n /= 94;
        } while (n > 0);
        return id;
    };

    // Rebuild the scope nesting from the dotted names.
    std::vector<std::string> open_scopes;
    auto split = [](const std::string& name) {
        std::vector<std::string> parts;
        size_t start = 0;
        for (size_t i = 0; i <= name.size(); ++i) {
            if (i == name.size() || name[i] == SCOPE_SEPARATOR) {
                parts.push_back(name.substr(start, i - start));
                start = i + 1;
            }
        }
        return parts;
    };

    std::vector<std::string> sorted = signals;
    std::sort(sorted.begin(), sorted.end());

    for (size_t i = 0; i < sorted.size(); ++i) {
        auto parts = split(sorted[i]);
        std::vector<std::string> scopes(parts.begin(), parts.end() - 1);

        size_t shared = 0;
        while (shared < scopes.size() && shared < open_scopes.size() &&
               scopes[shared] == open_scopes[shared]) {
            ++shared;
        }
        for (size_t k = open_scopes.size(); k > shared; --k) {
            out << "$upscope $end\n";
        }
        for (size_t k = shared; k < scopes.size(); ++k) {
            out << "$scope module " << scopes[k] << " $end\n";
        }
        open_scopes = scopes;

        ids[sorted[i]] = identifier(i);
        int width = signal_width(sorted[i]);
        if (width <= 0) width = 1;
        out << "$var wire " << width << " " << ids[sorted[i]] << " "
            << parts.back() << " $end\n";
    }
    for (size_t k = open_scopes.size(); k > 0; --k) {
        out << "$upscope $end\n";
    }
    out << "$enddefinitions $end\n";

    // Values, one block per index, emitting only what changed.
    int saved_index = current_index;
    std::unordered_map<std::string, std::string> previous;
    for (int idx = 0; idx <= max_index; ++idx) {
        set_index(idx);
        std::ostringstream block;
        for (const auto& name : sorted) {
            std::string rendered;
            try {
                Value value = signal_value(name, 0);
                int width = signal_width(name);
                if (width <= 0) width = 1;
                if (value.is_string()) {
                    rendered = (width == 1) ? value.to_string()
                                            : "b" + value.to_string() + " ";
                } else if (width == 1) {
                    rendered = value.is_truthy() ? "1" : "0";
                } else {
                    std::string bits;
                    uint64_t raw = static_cast<uint64_t>(value.to_int());
                    for (int bit = width - 1; bit >= 0; --bit) {
                        bits += ((raw >> bit) & 1ULL) ? '1' : '0';
                    }
                    rendered = "b" + bits + " ";
                }
            } catch (const std::exception&) {
                rendered = "x";   // not readable at this index
            }
            auto it = previous.find(name);
            if (it == previous.end() || it->second != rendered) {
                block << rendered << ids[name] << "\n";
                previous[name] = rendered;
            }
        }
        std::string changes = block.str();
        if (idx == 0 || !changes.empty()) {
            out << "#" << get_timestamp() << "\n" << changes;
        }
    }
    set_index(saved_index);

    if (path.empty()) {
        std::cout << out.str();
        return "";
    }
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("dump-trace: cannot write " + path);
    }
    file << out.str();
    return path;
}

} // namespace wal
