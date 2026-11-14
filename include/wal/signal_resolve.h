#pragma once

#include "ast.h"
#include "trace/trace_container.h"
#include "trace/trace.h"
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


/**
 * @brief Resolve one of WAL's special variables, such as INDEX or MAX-INDEX.
 *
 * `Trace::get_special_signal` has implemented these all along (INDEX, MAX-INDEX, TS,
 * TRACE-NAME, TRACE-FILE, SIGNALS, SCOPES, VIRTUAL-SIGNALS), but nothing ever called
 * it from the evaluator, so every one of them failed with *Undefined variable*. This
 * connects the two.
 *
 * MAX-INDEX also closes a functional gap rather than only a compatibility one: there
 * was no way at all to ask how long a trace is, so the timestamp count had to be
 * inferred from `(length (hamming-weight-scope ...))`.
 *
 * With several traces loaded the first one answers, which matches the single-trace case
 * every published WAL program assumes. Upstream qualifies these per trace id; that is
 * left for the trace-id work rather than guessed at here.
 *
 * @return true if @p name is a special variable that resolved
 */
/// Whether @p name is one of WAL's special variables, independent of any trace.
inline bool is_special_variable(const std::string& name) {
    return name == Trace::INDEX || name == Trace::MAX_INDEX ||
           name == Trace::TIMESTAMP || name == Trace::TRACE_NAME ||
           name == Trace::TRACE_FILE || name == Trace::SIGNALS ||
           name == Trace::SCOPES || name == Trace::VIRTUAL_SIGNALS ||
           name == Trace::LOCAL_SCOPES || name == Trace::LOCAL_SIGNALS;
}

inline bool resolve_special_variable(const std::shared_ptr<TraceContainer>& traces,
                                    const std::string& name,
                                    Value& out,
                                    const std::string& scope = "") {
    if (!traces || traces->empty()) {
        return false;
    }
    // Only the names Trace::get_special_signal actually implements. The header also
    // declares SIGNALS-NO-ALIAS, LOCAL-SIGNALS and LOCAL-SCOPES, which it does not
    // implement; LOCAL-* need a notion of current scope, so they belong with the scope
    // work rather than being faked here.
    // `a^INDEX` asks for trace a's INDEX. Upstream qualifies special variables per
    // trace id this way, which is the only way to read them with several loaded.
    std::string bare = name;
    std::string requested_tid;
    auto caret = name.find('^');
    if (caret != std::string::npos && caret > 0) {
        std::string prefix = name.substr(0, caret);
        std::string rest = name.substr(caret + 1);
        if (is_special_variable(rest)) {
            for (const auto& loaded : traces->get_trace_ids()) {
                if (loaded == prefix) {
                    requested_tid = prefix;
                    bare = rest;
                    break;
                }
            }
        }
    }

    if (!is_special_variable(bare)) {
        return false;
    }

    auto ids = traces->get_trace_ids();
    if (ids.empty()) {
        return false;
    }
    auto trace = traces->get_trace(requested_tid.empty() ? ids.front() : requested_tid);
    if (!trace) {
        return false;
    }
    try {
        // LOCAL-SCOPES and LOCAL-SIGNALS are relative to the caller's current scope.
        trace->set_query_scope(scope);
        out = trace->get_special_signal(bare);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace wal
