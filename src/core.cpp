#include "wal/core.h"
#include "wal/parser.h"
#include <iostream>
#include <fstream>
#include <sstream>

namespace wal {

Wal::Wal() {
    traces = std::make_shared<TraceContainer>();
    eval_context = std::make_shared<SEval>(traces);
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
    Parser parser;
    Value expr = parser.parse(code);
    return eval_context->eval(expr);
}

Value Wal::eval_program(const std::string& code) {
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

