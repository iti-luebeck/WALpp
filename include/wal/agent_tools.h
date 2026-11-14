#pragma once

#include "ast.h"
#include <string>
#include <vector>

namespace wal {

/**
 * @brief Machine-readable entry points, for a program driving WAL++ rather than a person.
 *
 * An agent reading a trace faces the same first question every time: what is in it,
 * spelled exactly how. Getting that from `--fast-list` means parsing a text format;
 * getting it from the REPL means parsing S-expression output. Both are guessable and
 * both go wrong on the details, which is how a wrong signal name ends up in a script
 * that then runs and produces a plausible number.
 *
 * The output follows the envelope used by the ITI `ekumen` tool, so an assistant that
 * has learned one of them can read the other. Two properties of it are safety
 * properties rather than conveniences:
 *
 *   **Failures have the same envelope as successes.** A consumer that must tell "the
 *   command failed" from "the command returned nothing" by reading prose will
 *   eventually get it wrong in the direction that matters. `ok` is always present.
 *
 *   **Age is materialised.** A trace is a file, and an agent can easily be reading one
 *   produced before the change it is trying to evaluate. Every payload carries the
 *   source's modification time and its age, so an answer from a stale trace cannot be
 *   skimmed as current. No freshness budget is invented here, because WAL++ has no way
 *   to know what the trace should be newer THAN; the facts are reported and the
 *   consumer decides.
 *
 * JSON is emitted directly rather than through a library: the shapes are small and
 * fixed, and a dependency for this would be out of proportion.
 */

/// Bumped when the envelope's own shape changes, never for payload additions.
inline constexpr int ENVELOPE_VERSION = 1;

/// A failure, in the shape the envelope carries it.
struct AgentError {
    std::string code;      ///< stable, machine-matchable, e.g. "walpp.trace.not_found"
    std::string message;
    std::string subject;   ///< what it was about, when there is one
    std::string hint;      ///< what to do about it, when there is something to say
};

/**
 * @brief Wrap a payload in the standard envelope.
 * @param command  the mode that produced it, e.g. "inspect"
 * @param payload  already-rendered JSON, or "null"
 * @param source   the file the answer came from, for the age fields; may be empty
 */
std::string envelope(const std::string& command, const std::string& payload,
                     const std::string& source, bool pretty);

/// The same envelope, carrying a failure instead of a payload.
std::string error_envelope(const std::string& command, const AgentError& error,
                           const std::string& source, bool pretty);

/// Escape a string for JSON, including the control characters a signal name can carry.
std::string json_escape(const std::string& text);

/**
 * @brief Describe a trace as JSON.
 *
 * Reads only the header where the format allows, so it stays cheap on a large file.
 * @param file   path to a VCD, FST or CSV trace
 * @param pretty indent the output
 */
std::string inspect_trace(const std::string& file, bool pretty);

/// One thing wrong with a script.
struct LintFinding {
    enum class Severity { Error, Warning };

    Severity severity = Severity::Error;
    int line = 0;             ///< 1-based; 0 when the position is not known
    int column = 0;           ///< 1-based
    std::string code;         ///< stable identifier, e.g. "unknown-operator"
    std::string message;      ///< what is wrong
    std::string hint;         ///< what to do about it, when there is something to say
};

/**
 * @brief Check a script without running it.
 *
 * Catches the two failures that are worth catching statically: source that does not
 * parse, and a call to something that does not exist. The second is the common one,
 * because a mistyped operator is indistinguishable from a signal name until the
 * moment it is evaluated, and by then half the script has already run.
 *
 * A symbol it cannot resolve is reported as a WARNING rather than an error, because a
 * bare symbol is how a signal is read and no trace is loaded here. The distinction
 * matters: an unknown symbol in head position is a call to something that does not
 * exist, which is an error; the same symbol in argument position is probably a signal.
 */
std::vector<LintFinding> lint_script(const std::string& source,
                                     const std::string& filename);

/// Render lint findings as JSON.
std::string lint_findings_json(const std::vector<LintFinding>& findings,
                               const std::string& filename, bool pretty);

/// Render lint findings the way a compiler would, for a person reading a terminal.
std::string lint_findings_text(const std::vector<LintFinding>& findings,
                               const std::string& filename);

/// Render an evaluated value as JSON, so a caller need not parse WAL syntax.
std::string value_to_json(const Value& value);

} // namespace wal
