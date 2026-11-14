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
 * So these three say the same things the human-facing modes do, in JSON:
 *
 *  - inspect_trace()  what is in the trace: timescale, length, scopes, every signal
 *                     with its width, spelled exactly as the engine will accept it
 *  - lint_script()    what is wrong with a script, WITHOUT running it, located by
 *                     line and column
 *  - to_json()        a result value, so it can be read without parsing WAL syntax
 *
 * JSON is emitted directly rather than through a library: the shapes are small and
 * fixed, and a dependency for this would be out of proportion.
 */

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
