#pragma once

#include "ast.h"
#include "trace/trace_container.h"
#include <memory>
#include <string>
#include <vector>

namespace wal {

/**
 * @brief Resolve a signal name written in any of the accepted spellings.
 *
 * WAL++ and upstream WAL (ics-jku/wal) disagree about separators, and WAL++'s own
 * fast paths disagree with its interpreter:
 *
 *   - WAL++ interpreter: `tb^dut^counter`   (`^` joins the hierarchy)
 *   - WAL++ `--fast-*`:  `tb.dut.counter`   (`.` joins the hierarchy)
 *   - upstream WAL:      `tb.dut.counter`, with `^` reserved to prefix a TRACE ID,
 *                        as in `a^tb.dut.counter`
 *
 * Verified against upstream 0.9.1: `(print tb^dut^counter)` is an error there, while
 * `a^tb.dut.counter` selects the signal from the trace loaded under id `a`.
 *
 * Rather than pick a winner and break existing scripts, this resolver accepts all of
 * them. It tries candidate spellings in order of specificity and returns the first that
 * a loaded trace actually contains, so a name that was already valid keeps resolving to
 * the same signal.
 *
 * @param traces  loaded traces (may be null or empty, in which case nothing resolves)
 * @param name    the name as written by the user
 * @param out     receives the canonical name a trace recognises
 * @return true if some spelling resolved
 */
inline bool resolve_signal_name(const std::shared_ptr<TraceContainer>& traces,
                                const std::string& name,
                                std::string& out) {
    if (!traces || traces->empty() || name.empty()) {
        return false;
    }

    auto translate = [](const std::string& s, char from, char to) {
        std::string r = s;
        for (auto& c : r) {
            if (c == from) c = to;
        }
        return r;
    };

    std::vector<std::string> candidates;
    candidates.push_back(name);

    // Hierarchy-separator spellings. A name using one convention should resolve
    // against a trace indexed with the other.
    if (name.find('.') != std::string::npos) {
        candidates.push_back(translate(name, '.', '^'));
    }
    if (name.find('^') != std::string::npos) {
        candidates.push_back(translate(name, '^', '.'));
    }

    // `tid^path` as upstream spells it: strip a leading trace id, then try the
    // remaining path in both conventions. Only done when the prefix really is a
    // loaded trace id, so a WAL++ `tb^dut^counter` is not mistaken for one.
    auto caret = name.find('^');
    if (caret != std::string::npos && caret > 0) {
        std::string tid = name.substr(0, caret);
        std::string path = name.substr(caret + 1);
        for (const auto& loaded : traces->get_trace_ids()) {
            if (loaded == tid) {
                candidates.push_back(path);
                if (path.find('.') != std::string::npos) {
                    candidates.push_back(translate(path, '.', '^'));
                }
                break;
            }
        }
    }

    for (const auto& candidate : candidates) {
        if (traces->contains(candidate)) {
            out = candidate;
            return true;
        }
    }
    return false;
}

} // namespace wal
