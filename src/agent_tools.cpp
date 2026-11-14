#include "wal/agent_tools.h"
#include "wal/core.h"
#include "wal/parser.h"
#include "wal/evaluator.h"
#include "wal/trace/trace_container.h"

#include <algorithm>
#include <sstream>
#include <filesystem>
#include <ctime>
#include <chrono>
#include <unordered_set>

// Kept beside the version the CLI prints; CMake owns the number.
// Normally defined by CMake from project(VERSION). The fallback only matters for a
// translation unit built outside the project, and says so rather than naming a
// version that will quietly go stale.
#ifndef WALPP_VERSION
#define WALPP_VERSION "0.0.0-unknown"
#endif

namespace wal {

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------

std::string json_escape(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (unsigned char c : text) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    // A VCD identifier is printable ASCII, but a malformed file can
                    // carry anything and invalid JSON is worse than an ugly name.
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

namespace {

std::string quoted(const std::string& s) { return "\"" + json_escape(s) + "\""; }

/// Current UTC time, ISO 8601 to the second, as the envelope's generated_at.
std::string iso_now() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

/// The source file's modification time and age, so a stale trace cannot be read as
/// current. No budget is applied: WAL++ cannot know what the trace should be newer
/// than, so it reports and leaves the judgement to the caller.
std::string source_block(const std::string& file, const char* i1, const char* kv,
                         const char* nl) {
    std::ostringstream out;
    out << i1 << quoted("source") << kv;
    if (file.empty()) {
        out << "null," << nl;
        return out.str();
    }
    std::error_code ec;
    auto mtime = std::filesystem::last_write_time(file, ec);
    if (ec) {
        out << "{" << quoted("path") << kv << quoted(file) << "}," << nl;
        return out.str();
    }
    auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        mtime - std::filesystem::file_time_type::clock::now() +
        std::chrono::system_clock::now());
    std::time_t t = std::chrono::system_clock::to_time_t(sys);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - sys).count();
    if (age < 0) age = 0;

    out << "{" << quoted("path") << kv << quoted(file) << ","
        << quoted("modified_at") << kv << quoted(buf) << ","
        << quoted("age_seconds") << kv << age << "}," << nl;
    return out.str();
}

/// Scope prefix of a dotted name, empty for a top-level signal.
std::string scope_of(const std::string& name) {
    auto cut = name.rfind(Trace::SCOPE_SEPARATOR);
    return cut == std::string::npos ? std::string() : name.substr(0, cut);
}

std::string leaf_of(const std::string& name) {
    auto cut = name.rfind(Trace::SCOPE_SEPARATOR);
    return cut == std::string::npos ? name : name.substr(cut + 1);
}

} // namespace

// ---------------------------------------------------------------------------
// the envelope
// ---------------------------------------------------------------------------

namespace {

std::string envelope_impl(const std::string& command, const std::string& payload,
                          const std::string& source, bool ok,
                          const AgentError* error, bool pretty) {
    const char* nl = pretty ? "\n" : "";
    const char* i1 = pretty ? "  " : "";
    const char* kv = pretty ? ": " : ":";

    std::ostringstream out;
    out << "{" << nl;
    out << i1 << quoted("envelope_version") << kv << ENVELOPE_VERSION << "," << nl;
    out << i1 << quoted("tool") << kv << quoted("walpp") << "," << nl;
    out << i1 << quoted("tool_version") << kv << quoted(WALPP_VERSION) << "," << nl;
    out << i1 << quoted("command") << kv << quoted(command) << "," << nl;
    out << i1 << quoted("generated_at") << kv << quoted(iso_now()) << "," << nl;
    out << source_block(source, i1, kv, nl);
    out << i1 << quoted("ok") << kv << (ok ? "true" : "false") << "," << nl;
    out << i1 << quoted("warnings") << kv << "[]," << nl;
    if (error) {
        out << i1 << quoted("error") << kv << "{"
            << quoted("code") << kv << quoted(error->code) << ","
            << quoted("message") << kv << quoted(error->message);
        if (!error->subject.empty()) {
            out << "," << quoted("subject") << kv << quoted(error->subject);
        }
        if (!error->hint.empty()) {
            out << "," << quoted("hint") << kv << quoted(error->hint);
        }
        out << "}," << nl;
    }
    out << i1 << quoted("data") << kv << (payload.empty() ? "null" : payload) << nl;
    out << "}";
    return out.str();
}

} // namespace

std::string envelope(const std::string& command, const std::string& payload,
                     const std::string& source, bool pretty) {
    return envelope_impl(command, payload, source, true, nullptr, pretty);
}

std::string error_envelope(const std::string& command, const AgentError& error,
                           const std::string& source, bool pretty) {
    return envelope_impl(command, "null", source, false, &error, pretty);
}

// ---------------------------------------------------------------------------
// inspect
// ---------------------------------------------------------------------------

std::string inspect_trace(const std::string& file, bool pretty) {
    const char* nl = pretty ? "\n" : "";
    const char* i1 = pretty ? "  " : "";
    const char* i2 = pretty ? "    " : "";
    const char* i3 = pretty ? "      " : "";
    const char* kv = pretty ? ": " : ":";   // compact means compact

    Wal engine;
    engine.load(file, "t");

    auto traces = engine.get_traces();
    auto ids = traces->get_trace_ids();
    auto trace = traces->get_trace(ids.empty() ? "t" : ids.front());

    std::ostringstream out;
    out << "{" << nl;

    auto signals = trace->get_all_signals();
    auto scopes  = trace->get_scopes();

    if (!trace) {
        throw std::runtime_error("trace did not load");
    }
    out << i1 << quoted("max_index") << kv << trace->get_max_index() << "," << nl;
    out << i1 << quoted("index_count") << kv << (trace->get_max_index() + 1) << "," << nl;
    out << i1 << quoted("signal_count") << kv << signals.size() << "," << nl;
    out << i1 << quoted("scope_count") << kv << scopes.size() << "," << nl;

    // The separator is stated rather than assumed, because a caller building names
    // by hand is exactly who reads this.
    out << i1 << quoted("hierarchy_separator") << kv
        << quoted(std::string(1, Trace::SCOPE_SEPARATOR)) << "," << nl;
    out << i1 << quoted("trace_id_separator") << kv << quoted("^") << "," << nl;

    out << i1 << quoted("scopes") << ": [" << nl;
    for (size_t k = 0; k < scopes.size(); ++k) {
        out << i2 << quoted(scopes[k]) << (k + 1 < scopes.size() ? "," : "") << nl;
    }
    out << i1 << "]," << nl;

    out << i1 << quoted("signals") << ": [" << nl;
    for (size_t k = 0; k < signals.size(); ++k) {
        int width = 0;
        try { width = trace->signal_width(signals[k]); } catch (const std::exception&) {}
        out << i2 << "{" << nl;
        out << i3 << quoted("name")  << kv << quoted(signals[k]) << "," << nl;
        out << i3 << quoted("leaf")  << kv << quoted(leaf_of(signals[k])) << "," << nl;
        out << i3 << quoted("scope") << kv << quoted(scope_of(signals[k])) << "," << nl;
        out << i3 << quoted("width") << kv << width << nl;
        out << i2 << "}" << (k + 1 < signals.size() ? "," : "") << nl;
    }
    out << i1 << "]" << nl;
    out << "}";
    return out.str();
}

// ---------------------------------------------------------------------------
// lint
// ---------------------------------------------------------------------------

namespace {

/// Where an opening delimiter was, so an unterminated list can be pointed at.
struct OpenDelim { char ch; int line; int column; };

/**
 * A character scan that locates the delimiter problems. The parser reports WHAT went
 * wrong but not where, and "unterminated list" without a position is close to
 * useless in a file of any size.
 */
void scan_delimiters(const std::string& src, std::vector<LintFinding>& out) {
    std::vector<OpenDelim> open;
    int line = 1, column = 1;
    bool in_string = false, in_comment = false;

    for (size_t i = 0; i < src.size(); ++i) {
        char c = src[i];

        if (c == '\n') {
            ++line; column = 1; in_comment = false;
            continue;
        }

        if (!in_comment) {
            if (in_string) {
                if (c == '\\') { ++i; ++column; }
                else if (c == '"') in_string = false;
            } else if (c == '"') {
                in_string = true;
            } else if (c == ';') {
                in_comment = true;
            } else if (c == '(' || c == '[') {
                open.push_back({c, line, column});
            } else if (c == ')' || c == ']') {
                if (open.empty()) {
                    out.push_back({LintFinding::Severity::Error, line, column,
                                   "unmatched-close",
                                   std::string("unmatched '") + c + "'",
                                   "there is no open delimiter for this one"});
                } else {
                    char want = open.back().ch == '(' ? ')' : ']';
                    if (c != want) {
                        out.push_back({LintFinding::Severity::Error, line, column,
                                       "mismatched-close",
                                       std::string("'") + c + "' closes a '" +
                                           open.back().ch + "' opened at line " +
                                           std::to_string(open.back().line),
                                       std::string("expected '") + want + "'"});
                    }
                    open.pop_back();
                }
            }
        }
        ++column;
    }

    if (in_string) {
        out.push_back({LintFinding::Severity::Error, line, column,
                       "unterminated-string", "string is never closed", ""});
    }
    for (auto it = open.rbegin(); it != open.rend(); ++it) {
        out.push_back({LintFinding::Severity::Error, it->line, it->column,
                       "unterminated-list",
                       std::string("'") + it->ch + "' is never closed",
                       "the form opened here reaches the end of the file"});
    }
}

/// Names a form introduces, so a later use of them is not reported as unknown.
void collect_bindings(const Value& form, std::unordered_set<std::string>& known);

void collect_parameter_list(const Value& params, std::unordered_set<std::string>& known) {
    if (params.is_symbol()) {
        known.insert(params.to_symbol()->get_name());   // variadic macro form
        return;
    }
    if (!params.is_list()) return;
    for (const auto& p : *params.to_list()) {
        if (p.is_symbol()) known.insert(p.to_symbol()->get_name());
    }
}

void collect_bindings(const Value& form, std::unordered_set<std::string>& known) {
    if (!form.is_list()) return;
    auto list = form.to_list();
    if (list->empty() || !(*list)[0].is_symbol()) return;

    const std::string& head = (*list)[0].to_symbol()->get_name();

    if ((head == "define" || head == "defmacro" || head == "defun") && list->size() >= 2) {
        if ((*list)[1].is_symbol()) known.insert((*list)[1].to_symbol()->get_name());
        if ((head == "defmacro" || head == "defun") && list->size() >= 3) {
            collect_parameter_list((*list)[2], known);
        }
    } else if ((head == "lambda" || head == "fn") && list->size() >= 2) {
        collect_parameter_list((*list)[1], known);
    } else if (head == "let" && list->size() >= 2 && (*list)[1].is_list()) {
        for (const auto& binding : *(*list)[1].to_list()) {
            if (binding.is_list() && !binding.to_list()->empty() &&
                (*binding.to_list())[0].is_symbol()) {
                known.insert((*binding.to_list())[0].to_symbol()->get_name());
            }
        }
    } else if (head == "alias" && list->size() >= 2 && (*list)[1].is_symbol()) {
        known.insert((*list)[1].to_symbol()->get_name());
    }

    for (const auto& child : *list) collect_bindings(child, known);
}

/// Walk for calls to things that do not exist.
void check_calls(const Value& form, const std::unordered_set<std::string>& known,
                 std::vector<LintFinding>& out, bool quoted_context) {
    if (!form.is_list()) return;
    auto list = form.to_list();
    if (list->empty()) return;

    // Inside a quote nothing is a call, it is data.
    bool quoting = quoted_context;
    if ((*list)[0].is_symbol()) {
        const std::string& head = (*list)[0].to_symbol()->get_name();
        if (head == "quote") quoting = true;

        if (!quoting && known.count(head) == 0) {
            out.push_back({LintFinding::Severity::Error, 0, 0, "unknown-operator",
                           "call to '" + head + "', which is not an operator, "
                           "a macro, or defined in this file",
                           "check the spelling, or define it before use"});
        }
    }

    for (size_t k = 0; k < list->size(); ++k) {
        // The head has been handled; arguments are checked as expressions.
        check_calls((*list)[k], known, out, quoting);
    }
}

} // namespace

std::vector<LintFinding> lint_script(const std::string& source,
                                     const std::string& filename) {
    (void)filename;
    std::vector<LintFinding> findings;

    scan_delimiters(source, findings);
    if (!findings.empty()) {
        // A file that does not balance will not parse, and every later finding would
        // be noise from a confused parse.
        return findings;
    }

    std::vector<Value> forms;
    try {
        Parser parser;
        forms = parser.parse_program(source);
    } catch (const std::exception& e) {
        findings.push_back({LintFinding::Severity::Error, 0, 0, "parse-error",
                            e.what(), ""});
        return findings;
    }

    // Everything the engine provides, plus everything this file defines.
    Wal engine;
    engine.eval_str("1");                 // force the standard library to load
    auto eval = engine.get_eval_context();

    std::unordered_set<std::string> known;
    for (const auto& name : eval->get_operator_registry().get_all_operators()) {
        known.insert(name);
    }
    for (const auto& entry : eval->get_macros()) known.insert(entry.first);
    // Special forms are dispatched in the evaluator, not registered as operators.
    for (const char* form : {"quote", "quasiquote", "unquote", "unquote-splicing",
                             "if", "define", "defmacro", "macroexpand", "gensym",
                             "lambda", "fn", "begin", "do", "set!", "let", "while",
                             "when", "unless", "cond", "case", "eval", "parse"}) {
        known.insert(form);
    }
    for (const auto& form : forms) collect_bindings(form, known);

    for (const auto& form : forms) check_calls(form, known, findings, false);
    return findings;
}

std::string lint_findings_text(const std::vector<LintFinding>& findings,
                               const std::string& filename) {
    std::ostringstream out;
    for (const auto& f : findings) {
        out << filename;
        if (f.line > 0) out << ":" << f.line << ":" << f.column;
        out << ": " << (f.severity == LintFinding::Severity::Error ? "error" : "warning")
            << ": " << f.message << " [" << f.code << "]\n";
        if (!f.hint.empty()) out << "    " << f.hint << "\n";
    }
    size_t errors = 0;
    for (const auto& f : findings) {
        if (f.severity == LintFinding::Severity::Error) ++errors;
    }
    if (findings.empty()) {
        out << filename << ": no problems found\n";
    } else {
        out << findings.size() << " finding(s), " << errors << " error(s)\n";
    }
    return out.str();
}

std::string lint_findings_json(const std::vector<LintFinding>& findings,
                               const std::string& filename, bool pretty) {
    const char* nl = pretty ? "\n" : "";
    const char* i1 = pretty ? "    " : "";
    const char* i2 = pretty ? "      " : "";
    const char* i3 = pretty ? "        " : "";
    const char* kv = pretty ? ": " : ":";

    size_t errors = 0;
    for (const auto& f : findings) {
        if (f.severity == LintFinding::Severity::Error) ++errors;
    }

    // The payload only: the caller wraps it in the envelope, so `ok` at the top
    // level always means the same thing whichever command produced it.
    std::ostringstream out;
    out << "{" << nl;
    out << i1 << quoted("file") << kv << quoted(filename) << "," << nl;
    out << i1 << quoted("error_count") << kv << errors << "," << nl;
    out << i1 << quoted("finding_count") << kv << findings.size() << "," << nl;
    out << i1 << quoted("findings") << kv << "[" << nl;
    for (size_t k = 0; k < findings.size(); ++k) {
        const auto& f = findings[k];
        out << i2 << "{" << nl;
        out << i3 << quoted("severity") << kv
            << quoted(f.severity == LintFinding::Severity::Error ? "error" : "warning")
            << "," << nl;
        out << i3 << quoted("code") << kv << quoted("walpp.lint." + f.code) << "," << nl;
        out << i3 << quoted("line") << kv << f.line << "," << nl;
        out << i3 << quoted("column") << kv << f.column << "," << nl;
        out << i3 << quoted("message") << kv << quoted(f.message) << "," << nl;
        out << i3 << quoted("hint") << kv << quoted(f.hint) << nl;
        out << i2 << "}" << (k + 1 < findings.size() ? "," : "") << nl;
    }
    out << i1 << "]" << nl << (pretty ? "  " : "") << "}";
    return out.str();
}

// ---------------------------------------------------------------------------
// values as JSON
// ---------------------------------------------------------------------------

std::string value_to_json(const Value& value) {
    if (value.is_nil())    return "null";
    if (value.is_bool())   return value.to_bool() ? "true" : "false";
    if (value.is_int())    return std::to_string(value.to_int());
    if (value.is_bigint()) {
        // As a string: JSON numbers have no defined precision beyond a double, and
        // a bigint is used precisely when that is not enough.
        return quoted(value.to_bigint()->to_string());
    }
    if (value.is_float()) {
        std::ostringstream out;
        out << value.to_float();
        return out.str();
    }
    if (value.is_string()) return quoted(value.to_string());
    if (value.is_symbol()) return quoted(value.to_symbol()->get_name());
    if (value.is_list()) {
        std::ostringstream out;
        out << "[";
        auto list = value.to_list();
        for (size_t k = 0; k < list->size(); ++k) {
            if (k) out << ",";
            out << value_to_json((*list)[k]);
        }
        out << "]";
        return out.str();
    }
    if (value.is_array()) {
        std::ostringstream out;
        out << "{";
        const auto& entries = value.to_array()->entries();
        for (size_t k = 0; k < entries.size(); ++k) {
            if (k) out << ",";
            out << quoted(entries[k].first) << ":" << value_to_json(entries[k].second);
        }
        out << "}";
        return out.str();
    }
    return quoted(value.to_string_repr());
}

} // namespace wal
