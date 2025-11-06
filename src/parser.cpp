#include "wal/parser.h"
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
        if (std::isspace(c) || c == '(' || c == ')' || c == '\'' || 
            c == '`' || c == ',' || c == ';') {
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

    // Otherwise it's a symbol
    return Value(std::make_shared<Symbol>(token));
}

Value Parser::parse_list() {
    skip_whitespace();
    
    if (peek() != '(') {
        throw EvalError("Expected '(' to start list");
    }
    next(); // consume '('

    std::vector<Value> elements;
    
    while (true) {
        skip_whitespace();
        
        // Skip comments
        while (peek() == ';') {
            skip_comment();
            skip_whitespace();
        }
        
        if (position >= input.length()) {
            throw EvalError("Unterminated list: expected ')' before end of input");
        }

        if (peek() == ')') {
            next(); // consume ')'
            break;
        }

        // Parse nested expression
        if (peek() == '(') {
            elements.push_back(parse_list());
        } else if (peek() == '\'') {
            elements.push_back(parse_quoted("quote"));
        } else if (peek() == '`') {
            elements.push_back(parse_quoted("quasiquote"));
        } else if (peek() == ',') {
            elements.push_back(parse_quoted("unquote"));
        } else {
            elements.push_back(parse_atom());
        }
    }

    // Create a WAL list
    auto list = std::make_shared<WList>(elements);
    return Value(list);
}

Value Parser::parse_quoted(const std::string& quote_type) {
    next(); // consume quote character
    
    // Create quote form: (quote expr)
    Value quoted_expr;
    
    skip_whitespace();
    if (peek() == '(') {
        quoted_expr = parse_list();
    } else {
        quoted_expr = parse_atom();
    }

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
    
    if (c == '(') {
        return parse_list();
    } else if (c == '\'') {
        return parse_quoted("quote");
    } else if (c == '`') {
        return parse_quoted("quasiquote");
    } else if (c == ',') {
        return parse_quoted("unquote");
    } else {
        return parse_atom();
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

