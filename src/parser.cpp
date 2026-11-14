#include "wal/parser.h"
#include "wal/bigint.h"
#include <cctype>
#include <stdexcept>
#include <sstream>

namespace wal {

void Parser::skip_whitespace() {
    while (position < input.length() && std::isspace(input[position])) {
        position++;
    }
}

void Parser::skip_comment() {
    if (position < input.length() && input[position] == ';') {
        while (position < input.length() && input[position] != '\n') {
            position++;
        }
        if (position < input.length()) {
            position++; // skip newline
        }
    }
}

char Parser::peek() const {
    if (position >= input.length()) return '\0';
    return input[position];
}

char Parser::next() {
    if (position >= input.length()) {
        throw EvalError("Unexpected end of input");
    }
    return input[position++];
}

std::string Parser::read_until_delimiter() {
    std::string result;
    while (position < input.length()) {
        char c = input[position];
        // `[` and `]` delimit lists too: upstream writes binding and parameter lists
        // that way, as in (let ([x 5]) x) and (fn [x] x).
        if (std::isspace(c) || c == '(' || c == ')' || c == '[' || c == ']' ||
            c == '\'' || c == '`' || c == ',' || c == ';') {
            break;
        }
        result += next();
    }
    return result;
}

Value Parser::parse_number(const std::string& token) {
    try {
        // Try to parse as integer first
        size_t idx;
        int64_t int_val = std::stoll(token, &idx);
        
        // If we consumed the whole string, it's an integer
        if (idx == token.length()) {
            return Value(int_val);
        }
        
        // Otherwise try as double
        double double_val = std::stod(token);
        return Value(double_val);
    } catch (const std::out_of_range&) {
        // A decimal integer too large for int64_t. This used to fall into the
        // catch-all below and become a SYMBOL, so `583547542375894327581` was
        // reported as an undefined variable rather than as a number out of range.
        if (auto big = BigInt::from_decimal(token)) {
            if (bigint_mode()) {
                return Value(std::make_shared<BigInt>(*big));
            }
            throw EvalError(
                "Integer literal " + token + " does not fit in 64 bits. "
                "WAL++ uses 64-bit integers by default; run with --bigint for "
                "arbitrary precision.");
        }
        // Out of range but not a plain integer: try it as a float, else a symbol.
        try {
            return Value(std::stod(token));
        } catch (...) {
            return Value(std::make_shared<Symbol>(token));
        }
    } catch (...) {
        // Not a number
        return Value(std::make_shared<Symbol>(token));
    }
}

Value Parser::parse_atom() {
    skip_whitespace();
    
    if (position >= input.length()) {
        throw EvalError("Unexpected end of input while parsing atom");
    }

    char c = peek();

    // String literal
    if (c == '"') {
        next(); // consume opening quote
        std::string str;
        while (position < input.length() && peek() != '"') {
            if (peek() == '\\' && position + 1 < input.length()) {
                next(); // consume backslash
                char escaped = next();
                switch (escaped) {
                    case 'n': str += '\n'; break;
                    case 't': str += '\t'; break;
                    case 'r': str += '\r'; break;
                    case '"': str += '"'; break;
                    case '\\': str += '\\'; break;
                    default: str += escaped; break;
                }
            } else {
                str += next();
            }
        }
        if (position >= input.length()) {
            throw EvalError("Unterminated string literal");
        }
        next(); // consume closing quote
        return Value(str);
    }

    // `~name` reads a name relative to the current scope, `#name` relative to the
    // current group. Upstream's standard library uses both, as in `#INDEX`.
    if (c == '~' || c == '#') {
        char marker = c;
        bool is_shorthand = false;
        if (position + 1 < input.length()) {
            char after = input[position + 1];
            // Read the whole word after the marker, so `#t`, `#f` and `#nil` stay
            // literals while `#INDEX` and `#ready` are group references.
            size_t end = position + 1;
            while (end < input.length() &&
                   (std::isalnum(static_cast<unsigned char>(input[end])) ||
                    input[end] == '_' || input[end] == '-')) {
                ++end;
            }
            std::string word = input.substr(position + 1, end - (position + 1));
            bool literal = (marker == '#') &&
                           (word == "t" || word == "f" || word == "nil" || word == "n");
            is_shorthand = !literal && !word.empty() &&
                           (std::isalpha(static_cast<unsigned char>(after)) || after == '_');
        }
        if (is_shorthand) {
            next();  // consume the marker
            Value target = parse_atom();
            auto call = std::make_shared<WList>();
            call->push_back(Value(std::make_shared<Symbol>(
                marker == '~' ? "resolve-scope" : "resolve-group")));
            call->push_back(target);
            return Value(call);
        }
    }

    // Booleans and nil
    if (c == '#') {
        next(); // consume #
        if (position >= input.length()) {
            throw EvalError("Invalid boolean/nil literal");
        }
        char next_c = next();
        if (next_c == 't') {
            return Value(true);
        } else if (next_c == 'f') {
            return Value(false);
        } else if (next_c == 'n') {
            // #nil or just #n?
            if (peek() == 'i') {
                next();
                if (peek() == 'l') {
                    next();
                    return Value(nullptr);
                }
            } else {
                return Value(nullptr);
            }
        }
        throw EvalError("Invalid boolean literal");
    }

    // Numbers and symbols
    std::string token = read_until_delimiter();
    if (token.empty()) {
        throw EvalError("Empty token");
    }

    // Check if it's a number
    if ((std::isdigit(token[0]) || 
         (token[0] == '-' && token.length() > 1 && std::isdigit(token[1])) ||
         (token[0] == '+' && token.length() > 1 && std::isdigit(token[1])))) {
        return parse_number(token);
    }

    // `signal@offset` notation (upstream WAL): the value of a signal at an index
    // relative to the current one, as in `tb.clk@-1`. `@` is not a token delimiter,
    // so the whole thing arrives here as one symbol token and is desugared to the
    // accessor WAL++ already has: (get-signal "tb.clk" -1).
    //
    // Only desugared when the part after `@` is an integer or a plain identifier, so
    // a signal name that merely contains an `@` is still read as a symbol.
    auto at = token.find('@');
    if (at != std::string::npos && at > 0 && at + 1 < token.length()) {
        std::string name = token.substr(0, at);
        std::string offset = token.substr(at + 1);

        bool is_int = true;
        size_t start = (offset[0] == '-' || offset[0] == '+') ? 1 : 0;
        if (start >= offset.length()) {
            is_int = false;
        }
        for (size_t i = start; i < offset.length() && is_int; ++i) {
            if (!std::isdigit(static_cast<unsigned char>(offset[i]))) is_int = false;
        }

        bool is_ident = !is_int;
        for (size_t i = 0; i < offset.length() && is_ident; ++i) {
            char oc = offset[i];
            if (!(std::isalnum(static_cast<unsigned char>(oc)) || oc == '_' || oc == '-')) {
                is_ident = false;
            }
        }

        if (is_int || is_ident) {
            std::vector<Value> call;
            call.push_back(Value(std::make_shared<Symbol>("get-signal")));
            call.push_back(Value(name));
            if (is_int) {
                call.push_back(Value(static_cast<int64_t>(std::stoll(offset))));
            } else {
                // e.g. `sig@INDEX`: evaluated at run time
                call.push_back(Value(std::make_shared<Symbol>(offset)));
            }
            return Value(std::make_shared<WList>(call));
        }
    }

    // Otherwise it's a symbol
    return Value(std::make_shared<Symbol>(token));
}

Value Parser::parse_list() {
    skip_whitespace();
    
    // A list may be written with parentheses or brackets; upstream uses brackets for
    // binding and parameter lists, as in (let ([x 5]) x) and (fn [x] x). The closing
    // delimiter must match the opening one.
    const char opener = peek();
    if (opener != '(' && opener != '[') {
        throw EvalError("Expected '(' or '[' to start list");
    }
    const char closer = (opener == '[') ? ']' : ')';
    next(); // consume the opening delimiter

    std::vector<Value> elements;
    
    while (true) {
        skip_whitespace();
        
        // Skip comments
        while (peek() == ';') {
            skip_comment();
            skip_whitespace();
        }
        
        if (position >= input.length()) {
            throw EvalError(std::string("Unterminated list: expected '") + closer +
                            "' before end of input");
        }

        if (peek() == closer) {
            next(); // consume the matching closing delimiter
            break;
        }
        if (peek() == ')' || peek() == ']') {
            throw EvalError(std::string("Mismatched list delimiter: expected '") +
                            closer + "' but found '" + peek() + "'");
        }

        // Parse nested expression
        if (peek() == '(' || peek() == '[') {
            elements.push_back(parse_list());
        } else if (peek() == '\'') {
            elements.push_back(parse_quoted("quote"));
        } else if (peek() == '`') {
            elements.push_back(parse_quoted("quasiquote"));
        } else if (peek() == ',') {
            elements.push_back(parse_unquote());
        } else {
            elements.push_back(parse_subscripts(parse_atom()));
        }
    }

    // Create a WAL list
    auto list = std::make_shared<WList>(elements);
    Value result = Value(list);

    // `(expr)@offset` evaluates the whole expression at a relative index, as in
    // `(map get SIGNALS)@x`. Only a bare identifier was handled before, so `@` after
    // a parenthesised expression was read as the start of a new symbol named "@x".
    // A signal's `clk@-1` keeps its own desugaring in parse_atom; for a general
    // expression the equivalent is reval.
    if (peek() == '@') {
        next();
        auto wrapper = std::make_shared<WList>();
        wrapper->push_back(Value(std::make_shared<Symbol>("reval")));
        wrapper->push_back(result);
        if (peek() == '(' || peek() == '[') {
            wrapper->push_back(parse_list());
        } else {
            wrapper->push_back(parse_atom());
        }
        return Value(wrapper);
    }
    return result;
}

/// `,expr` is unquote, `,@expr` is unquote-splicing. The two differ only in the
/// second character, so they are read together.
/**
 * @brief Apply postfix `[index]` subscripts to an already-parsed expression.
 *
 * Upstream writes `lst[0]`, `args[0][1]` and `arr["key"]`. The bracket binds only
 * when it follows the expression with NO whitespace, so a binding list such as
 * `(let ([x 5]) x)` is still read as a list, not as a subscript of `let`.
 */
Value Parser::parse_subscripts(Value target) {
    while (position < input.length() && peek() == '[') {
        next();  // consume '['
        skip_whitespace();
        Value index = parse_one();
        skip_whitespace();
        if (position >= input.length() || peek() != ']') {
            throw EvalError("Unterminated subscript: expected ']'");
        }
        next();  // consume ']'
        auto call = std::make_shared<WList>();
        call->push_back(Value(std::make_shared<Symbol>("subscript")));
        call->push_back(target);
        call->push_back(index);
        target = Value(call);
    }
    return target;
}

Value Parser::parse_unquote() {
    next();  // consume ','
    std::string form = "unquote";
    if (peek() == '@') {
        next();
        form = "unquote-splicing";
    }
    skip_whitespace();
    // parse_one handles every form a target can take, including a nested quote,
    // quasiquote or unquote and a postfix subscript. Dispatching on only '(' and an
    // atom here meant `,'x` and `,,x` were read as an empty token.
    Value target = parse_one();
    auto elements = std::vector<Value>{
        Value(std::make_shared<Symbol>(form)),
        target
    };
    return Value(std::make_shared<WList>(elements));
}

Value Parser::parse_quoted(const std::string& quote_type) {
    next(); // consume quote character
    
    // Create quote form: (quote expr)
    Value quoted_expr;
    
    skip_whitespace();
    // Same as parse_unquote: a quoted target can itself be quoted or unquoted.
    // `',symbol`, which upstream's standard library uses, was an "Empty token".
    quoted_expr = parse_one();

    auto quote_symbol = std::make_shared<Symbol>(quote_type);
    std::vector<Value> quote_form;
    quote_form.push_back(Value(quote_symbol));
    quote_form.push_back(quoted_expr);
    
    auto result_list = std::make_shared<WList>(quote_form);
    return Value(result_list);
}

Value Parser::parse_one() {
    skip_whitespace();
    
    // Skip comments
    while (peek() == ';') {
        skip_comment();
        skip_whitespace();
    }
    
    if (position >= input.length()) {
        throw EvalError("Unexpected end of input");
    }

    char c = peek();
    
    if (c == '(' || c == '[') {
        // A subscript may follow a parenthesised expression: (first xs)[0].
        return parse_subscripts(parse_list());
    } else if (c == '\'') {
        return parse_quoted("quote");
    } else if (c == '`') {
        return parse_quoted("quasiquote");
    } else if (c == ',') {
        return parse_unquote();
    } else {
        return parse_subscripts(parse_atom());
    }
}

Value Parser::parse(const std::string& source) {
    input = source;
    position = 0;
    return parse_one();
}

std::vector<Value> Parser::parse_program(const std::string& source) {
    input = source;
    position = 0;
    std::vector<Value> results;

    while (position < input.length()) {
        skip_whitespace();
        
        // Skip comments
        while (position < input.length() && peek() == ';') {
            skip_comment();
            skip_whitespace();
        }
        
        if (position >= input.length()) {
            break;
        }

        results.push_back(parse_one());
    }

    return results;
}

} // namespace wal

