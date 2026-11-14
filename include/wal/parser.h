#pragma once

#include "ast.h"
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

namespace wal {

/**
 * @defgroup parser Parser
 * @brief S-expression parsing and tokenization
 * 
 * The Parser converts WAL source text into an Abstract Syntax Tree (AST).
 * It handles:
 * - Tokenization (whitespace, comments, delimiters)
 * - Recursive descent parsing of S-expressions
 * - Support for quoted forms
 * - Number and string literal parsing
 * 
 * **Grammar:**
 * 
 * ```
 * program    ::= expression*
 * expression ::= atom | list | quote
 * atom       ::= symbol | number | string | keyword
 * list       ::= '(' expression* ')'
 * quote      ::= '\'' expression        → (quote expression)
 * quasiquote ::= '`' expression         → (quasiquote expression)
 * unquote    ::= ',' expression         → (unquote expression)
 * 
 * symbol     ::= [a-zA-Z_][a-zA-Z0-9_-]*
 * number     ::= integer | float
 * string     ::= '"' ... '"'
 * keyword    ::= ':' symbol
 * 
 * comment    ::= ';' .*
 * ```
 * 
 * **Parsing Example:**
 * 
 * @mermaid
 * graph TD
 *     A["Input: (+ 1 2)"] --> B["Tokenize"]
 *     B --> C["Tokens: '(' '+' '1' '2' ')'"]
 *     C --> D["Parse"]
 *     D --> E["WList with:<br/>- Symbol +<br/>- Number 1<br/>- Number 2"]
 * @endmermaid
 * 
 * @see @ref AST "AST Types" for value representation
 * @see @ref evaluator "Evaluator" for how AST is evaluated
 * 
 * @{
 */

/**
 * Recursive descent parser for WAL S-expressions
 * 
 * Converts WAL source code strings into Value AST nodes.
 * Single-pass parsing with minimal lookahead.
 * 
 * **Usage:**
 * ```cpp
 * Parser parser;
 * Value expr = parser.parse("(+ 1 2)");
 * std::vector<Value> program = parser.parse_program(
 *     "(define x 10) "
 *     "(print x)"
 * );
 * ```
 * 
 * **State:**
 * - Maintains input string and position
 * - Resets on each parse() call
 * - Suitable for parsing many independent expressions
 */
class Parser {
private:
    std::string input;
    size_t position = 0;

    ///@name Tokenization Helpers
    ///@{
    /// Skip whitespace and update position
    void skip_whitespace();
    
    /// Skip comment until end of line
    void skip_comment();
    
    /// Peek at current character without advancing
    char peek() const;
    
    /// Consume and return current character, advance position
    char next();
    
    /// Read characters until delimiter (space, paren, etc.)
    std::string read_until_delimiter();
    ///@}

    ///@name Parsing Helpers
    ///@{
    /// Parse an atom (number, symbol, string, etc.)
    Value parse_atom();
    
    /// Parse a list (parenthesized expressions)
    Value parse_list();
    
    /// Parse quoted expression (handles ', `, ,)
    Value parse_quoted(const std::string& quote_type);
    Value parse_unquote();  ///< `,expr` and `,@expr`
    
    /// Parse number literal (integer or float)
    Value parse_number(const std::string& token);
    ///@}

public:
    /**
     * Create a new parser
     * 
     * Parser is stateless - create once and reuse.
     */
    Parser() = default;

    ///@name Parsing Interface
    ///@{
    /**
     * Parse a complete WAL expression from string
     * 
     * Parses a single top-level expression.
     * If input contains multiple expressions, only the first is parsed.
     * To parse all, use parse_program().
     * 
     * **Examples:**
     * ```cpp
     * parser.parse("42")                    // Number(42)
     * parser.parse("x")                     // Symbol("x")
     * parser.parse("(+ 1 2)")              // WList{Symbol("+"), 1, 2}
     * parser.parse("'x")                    // WList{Symbol("quote"), Symbol("x")}
     * parser.parse("(lambda (x) (* x x))") // Lambda with quoted body
     * ```
     * 
     * **Whitespace handling:**
     * - Leading/trailing whitespace ignored
     * - Whitespace between tokens ignored
     * - Comments (;...) skipped
     * 
     * @param source The source code to parse
     * @return The parsed expression as a Value
     * @throws ParseError if syntax is invalid:
     *         - Unmatched parentheses
     *         - Invalid number format
     *         - Unexpected end of input
     *         - Invalid string escapes
     * @anchor Parser::parse
     */
    Value parse(const std::string& source);

    /**
     * Parse multiple expressions from source
     * 
     * Parses all top-level expressions in input and returns them.
     * Useful for parsing WAL programs with multiple forms.
     * 
     * **Example:**
     * ```cpp
     * auto program = parser.parse_program(
     *     "(define x 10) "
     *     "(define y 20) "
     *     "(+ x y)"
     * );
     * // program.size() == 3
     * ```
     * 
     * @param source The source code
     * @return Vector of parsed expressions in order
     * @throws ParseError if any expression syntax is invalid
     */
    std::vector<Value> parse_program(const std::string& source);

    /**
     * Parse single expression at current position
     * 
     * Advanced: parse one expression from current parser state.
     * Used internally and for incremental parsing.
     * 
     * @return The parsed expression
     * @throws ParseError if at end of input
     */
    Value parse_one();
    ///@}
};

/**
 * @}
 * @endgroup parser
 */

} // namespace wal


