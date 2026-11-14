#include "wal/core.h"
#include "wal/parser.h"
#include "wal/std_library.h"
#include <iostream>
#include <fstream>
#include <sstream>

namespace wal {

Wal::Wal() {
    traces = std::make_shared<TraceContainer>();
    eval_context = std::make_shared<SEval>(traces);
}

/**
 * @brief Define the standard library, once, on first use of the interpreter.
 *
 * Deliberately not done in the constructor: the --fast-* extraction paths build a
 * Wal but never evaluate anything, and they are the throughput-critical ones, so
 * they must not pay to parse the library.
 */
void Wal::ensure_std_library() {
    if (std_library_loaded) {
        return;
    }
    std_library_loaded = true;  // set first, so a failure cannot loop

    // A definition whose name WAL++ already implements as a builtin is SKIPPED.
    //
    // The two are not always the same function. Upstream's `append` is
    // (append list element), while WAL++'s builtin concatenates two lists, so
    // loading the library wholesale silently changed what `(append '(1 2) '(3 4))`
    // means. Rather than let the library redefine tested behavior, the builtin wins
    // and only the genuinely missing names are defined. Names skipped today:
    // append, car, cdr, cond, filter, reverse, set-index, set!, sort, sum,
    // symbol-add, unless, when.
    auto defined_name = [](const Value& form) -> std::string {
        if (!form.is_list()) return "";
        auto list = form.to_list();
        if (list->size() < 2 || !(*list)[0].is_symbol() || !(*list)[1].is_symbol()) {
            return "";
        }
        const std::string& head = (*list)[0].to_symbol()->get_name();
        if (head == "define" || head == "defun" || head == "defmacro") {
            return (*list)[1].to_symbol()->get_name();
        }
        return "";
    };

    try {
        Parser parser;
        for (const auto& form : parser.parse_program(std_library_source())) {
            std::string name = defined_name(form);
            if (!name.empty() &&
                eval_context->get_operator_registry().has_operator(name)) {
                continue;
            }
            try {
                eval_context->eval(form);
            } catch (const std::exception& e) {
                // One bad definition must not take the rest of the library with it.
                std::cerr << "warning: standard library: could not define "
                          << (name.empty() ? std::string("a form") : name)
                          << ": " << e.what() << "\n";
            }
        }
    } catch (const std::exception& e) {
        // A library that will not even parse must not make the interpreter
        // unusable: report it and carry on with the builtins.
        std::cerr << "warning: standard library failed to load: " << e.what() << "\n";
    }
}

void Wal::load(const std::string& file, const std::string& tid) {
    traces->load(file, tid);
}

void Wal::unload(const std::string& tid) {
    traces->unload(tid);
}

void Wal::step(int steps, const std::string& tid) {
    traces->step(steps);
}

Value Wal::eval_str(const std::string& code) {
    ensure_std_library();
    Parser parser;
    Value expr = parser.parse(code);
    return eval_context->eval(expr);
}

Value Wal::eval_program(const std::string& code) {
    ensure_std_library();
    Parser parser;
    std::vector<Value> forms = parser.parse_program(code);
    Value result = Value(nullptr);
    for (const auto& form : forms) {
        result = eval_context->eval(form);
    }
    return result;
}

Value Wal::eval(const Value& expr) {
    return eval_context->eval(expr);
}

Value Wal::run_str(const std::string& code) {
    eval_context->reset();
    return eval_str(code);
}

Value Wal::run(const Value& expr) {
    eval_context->reset();
    return eval(expr);
}

Value Wal::run_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw WalError("Cannot open file: " + filename);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    Parser parser;
    std::vector<Value> expressions = parser.parse_program(buffer.str());
    
    eval_context->reset();
    Value result = Value(nullptr);
    
    for (const auto& expr : expressions) {
        result = eval_context->eval(expr);
    }
    
    return result;
}

int Wal::start_repl() {
    std::cout << "WAL++ REPL v1.0 (Phase 1)\n";
    std::cout << "Type 'exit' to quit, 'help' for help\n\n";
    
    Parser parser;
    std::string input;
    
    while (true) {
        std::cout << "wal> ";
        
        // Read potentially multi-line input
        std::getline(std::cin, input);
        
        if (!std::cin) {
            std::cout << "\n";
            break;
        }
        
        if (input == "exit") {
            break;
        }
        
        if (input == "help") {
            std::cout << "WAL++ REPL Help:\n"
                      << "  exit         - Exit the REPL\n"
                      << "  help         - Show this message\n"
                      << "  (expr)       - Evaluate an expression\n\n";
            continue;
        }
        
        if (input.empty()) {
            continue;
        }
        
        try {
            Value result = eval_str(input);
            std::cout << "=> " << result.to_string_repr() << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }
    
    return 0;
}

void Wal::print_error(const Value& expr, const std::string& error) {
    std::cerr << "Error evaluating: " << expr.to_string_repr() << "\n";
    std::cerr << "  " << error << "\n";
}

} // namespace wal

