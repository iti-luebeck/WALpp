#include "wal/repl.h"
#include "wal/core.h"
#include "wal/trace/trace.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <readline/readline.h>
#include <readline/history.h>

namespace wal {

REPL::REPL(std::shared_ptr<Wal> wal)
    : wal_instance(wal ? wal : std::make_shared<Wal>()) {
    load_history();
}

int REPL::run() {
    print_welcome();
    
    while (running) {
        std::string input = read_input("wal> ");
        
        if (input.empty()) {
            continue;
        }
        
        // Add to history
        history.push_back(input);
        if (history.size() > MAX_HISTORY) {
            history.pop_front();
        }
        
        if (is_command(input)) {
            handle_command(input);
        } else {
            execute(input);
        }
    }
    
    save_history();
    return 0;
}

bool REPL::execute(const std::string& expr) {
    try {
        Value result = wal_instance->eval_str(expr);
        print_output(result);
        return true;
    } catch (const std::exception& e) {
        print_error(e.what());
        return false;
    }
}

bool REPL::load_file(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            print_error("Failed to open file: " + filename);
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string contents = buffer.str();
        
        // Execute file contents
        execute(contents);
        return true;
    } catch (const std::exception& e) {
        print_error("Error loading file: " + std::string(e.what()));
        return false;
    }
}

std::string REPL::read_input(const std::string& prompt) {
    char* line = readline(prompt.c_str());
    if (!line) {
        // EOF reached
        running = false;
        return "";
    }
    
    std::string input(line);
    free(line);
    
    if (!input.empty()) {
        add_history(input.c_str());
    }
    
    return input;
}

void REPL::print_output(const Value& result) {
    if (result.is_nil()) {
        std::cout << "nil" << std::endl;
    } else if (result.is_bool()) {
        std::cout << (result.to_bool() ? "true" : "false") << std::endl;
    } else if (result.is_int()) {
        std::cout << result.to_int() << std::endl;
    } else if (result.is_float()) {
        std::cout << result.to_float() << std::endl;
    } else if (result.is_string()) {
        std::cout << "\"" << result.to_string() << "\"" << std::endl;
    } else if (result.is_symbol()) {
        std::cout << "'" << result.to_symbol()->get_name() << std::endl;
    } else if (result.is_list()) {
        auto list = result.to_list();
        std::cout << "(";
        for (size_t i = 0; i < list->size(); ++i) {
            if (i > 0) std::cout << " ";
            const auto& elem = (*list)[i];
            if (elem.is_string()) {
                std::cout << "\"" << elem.to_string() << "\"";
            } else if (elem.is_symbol()) {
                std::cout << "'" << elem.to_symbol()->get_name();
            } else {
                print_output(elem);
            }
        }
        std::cout << ")" << std::endl;
    } else {
        std::cout << "<#value>" << std::endl;
    }
}

void REPL::print_error(const std::string& error) {
    std::cerr << "\033[31mError\033[0m: " << error << std::endl;
}

void REPL::print_welcome() {
    std::cout << "\033[36mWAL++ Interactive Shell\033[0m" << std::endl;
    std::cout << "Type (quit) to exit or (help) for help" << std::endl;
    std::cout << std::endl;
}

void REPL::print_help() {
    std::cout << "\n\033[1mWAL++ Built-in Commands:\033[0m\n"
              << "  (quit)                   - Exit the REPL\n"
              << "  (help)                   - Show this help\n"
              << "  (help \"<operator>\")      - Show operator documentation\n"
              << "  (list-operators)         - List all available operators\n"
              << "  (version)                - Show version information\n"
              << "\n\033[1mExamples:\033[0m\n"
              << "  (+ 1 2 3)                - Arithmetic: 6\n"
              << "  (define x 42)            - Define variable\n"
              << "  (map (lambda (x) (* x 2)) '(1 2 3))  - Map function\n"
              << "  (load-file \"program.wal\") - Load a program file\n"
              << "\n";
}

void REPL::handle_command(const std::string& line) {
    std::string trimmed = line;
    
    // Remove leading/trailing whitespace
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    
    if (trimmed == "(quit)" || trimmed == "(exit)") {
        running = false;
        std::cout << "Goodbye!" << std::endl;
    } else if (trimmed == "(help)") {
        print_help();
    } else if (trimmed == "(version)") {
        std::cout << "WAL++ v0.1.0" << std::endl;
    } else if (trimmed == "(list-operators)") {
        auto registry = wal_instance->get_operator_registry();
        std::cout << "Available operators: " << std::endl;
        // This would need operator discovery in core.h
        std::cout << "(Use help \"<op>\" for details)" << std::endl;
    } else {
        // Not a recognized command, try to evaluate as expression
        execute(line);
    }
}

bool REPL::is_command(const std::string& line) const {
    std::string trimmed = line;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    
    return trimmed == "(quit)" || trimmed == "(exit)" ||
           trimmed == "(help)" || trimmed == "(version)" ||
           trimmed == "(list-operators)";
}

void REPL::save_history() {
    std::string history_file = load_history_file();
    std::ofstream file(history_file);
    
    for (const auto& cmd : history) {
        file << cmd << "\n";
    }
}

void REPL::load_history() {
    std::string history_file = load_history_file();
    std::ifstream file(history_file);
    
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                history.push_back(line);
                add_history(line.c_str());
            }
        }
    }
}

std::string REPL::load_history_file() const {
    const char* home = std::getenv("HOME");
    if (!home) {
        home = ".";
    }
    return std::string(home) + "/.wal_history";
}

} // namespace wal
