#include "wal/repl.h"
#include "wal/core.h"
#include "wal/value_printer.h"
#include "wal/trace/trace.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <cstdio>
#include <iostream>

#ifdef WAL_HAVE_LIBEDIT
// libedit's readline-compatible API (BSD-3-Clause), not GNU Readline (GPL)
#include <editline/readline.h>
#endif

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
        // eval_program, not eval_str: the input may hold several top-level forms,
        // which is the normal shape of a script file, stdin or a pipe. eval_str
        // parses one expression, so everything after the first was silently ignored.
        Value result = wal_instance->eval_program(expr);
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

#ifdef WAL_HAVE_LIBEDIT
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
#else
// Fallback without libedit: plain line input, no editing or history recall
std::string REPL::read_input(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string input;
    if (!std::getline(std::cin, input)) {
        // EOF reached
        running = false;
        return "";
    }
    return input;
}
#endif

void REPL::print_output(const Value& result) {
    // Skip printing nil values (commonly returned by print function)
    if (result.is_nil()) {
        return;
    }
    // Shared with the CLI's -e path; see include/wal/value_printer.h
    print_value(result, std::cout, /*quote_strings=*/true);
    std::cout << std::endl;
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
        std::cout << "WAL++ v" WALPP_VERSION << std::endl;
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
#ifdef WAL_HAVE_LIBEDIT
                add_history(line.c_str());
#endif
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

bool REPL::load_pipe(const std::string& pipe_path) {
    try {
        // Determine format from pipe_path if possible
        std::string format = "vcd";  // default
        size_t dot_pos = pipe_path.find_last_of(".");
        if (dot_pos != std::string::npos) {
            std::string ext = pipe_path.substr(dot_pos + 1);
            if (ext == "csv" || ext == "fst" || ext == "vcd") {
                format = ext;
            }
        }
        
        // Create a temporary file with correct extension
        std::string temp_file = "/tmp/wal_pipe_" + std::to_string(getpid()) + "." + format;
        
        // Open named pipe (FIFO) for reading
        std::ifstream pipe(pipe_path);
        if (!pipe.is_open()) {
            print_error("Failed to open pipe: " + pipe_path);
            return false;
        }
        
        // Write pipe content to temporary file
        std::ofstream temp(temp_file);
        if (!temp.is_open()) {
            print_error("Failed to create temporary file: " + temp_file);
            return false;
        }
        
        temp << pipe.rdbuf();
        temp.close();
        pipe.close();
        
        // Check file size
        std::ifstream check(temp_file);
        check.seekg(0, std::ios::end);
        size_t file_size = check.tellg();
        check.close();
        
        if (file_size == 0) {
            print_error("Pipe is empty: " + pipe_path);
            std::remove(temp_file.c_str());
            return false;
        }
        
        // Load as trace file (format auto-detected from extension)
        std::cout << "Loading trace data from pipe: " << pipe_path << " (" << file_size << " bytes)" << std::endl;
        try {
            wal_instance->load(temp_file);
            std::cout << "✓ Trace loaded successfully from pipe" << std::endl;
        } catch (const std::exception& e) {
            print_error("Failed to load trace: " + std::string(e.what()));
            std::remove(temp_file.c_str());
            return false;
        }
        
        // Clean up temporary file
        std::remove(temp_file.c_str());
        return true;
    } catch (const std::exception& e) {
        print_error("Error reading from pipe: " + std::string(e.what()));
        return false;
    }
}

bool REPL::read_from_stdin() {
    try {
        // Check if stdin is available
        if (std::cin.eof()) {
            print_error("Standard input is not available");
            return false;
        }
        
        // Try to detect format from stdin header
        // Read first few bytes to detect format
        std::string header_buf;
        char c;
        for (int i = 0; i < 20 && std::cin.get(c); ++i) {
            header_buf += c;
        }
        
        std::string format = "vcd";  // default
        if (header_buf.find("$") != std::string::npos) {
            format = "vcd";  // VCD starts with $
        } else if (header_buf.find(",") != std::string::npos && header_buf.find("\n") != std::string::npos) {
            format = "csv";  // CSV has commas and newlines
        } else if (header_buf.find("LSB") != std::string::npos || header_buf.find("FST") != std::string::npos) {
            format = "fst";  // FST has LSB/FST marker
        }
        
        // Create temp file with detected format
        std::string temp_file = "/tmp/wal_stdin_" + std::to_string(getpid()) + "." + format;
        
        // Read stdin into temporary file
        std::ofstream temp(temp_file);
        if (!temp.is_open()) {
            print_error("Failed to create temporary file: " + temp_file);
            return false;
        }
        
        // Write buffered header first
        temp << header_buf;
        
        // Write rest of stdin
        temp << std::cin.rdbuf();
        temp.close();
        
        // Check file size
        std::ifstream check(temp_file);
        check.seekg(0, std::ios::end);
        size_t file_size = check.tellg();
        check.close();
        
        if (file_size == 0) {
            print_error("No input from standard input");
            std::remove(temp_file.c_str());
            return false;
        }
        
        // Load as trace file
        std::cout << "Loading trace data from stdin... (detected format: " << format << ", " << file_size << " bytes)" << std::endl;
        try {
            wal_instance->load(temp_file);
            std::cout << "✓ Trace loaded successfully from stdin" << std::endl;
        } catch (const std::exception& e) {
            print_error("Failed to load trace: " + std::string(e.what()));
            std::remove(temp_file.c_str());
            return false;
        }
        
        // Clean up temporary file
        std::remove(temp_file.c_str());
        return true;
    } catch (const std::exception& e) {
        print_error("Error reading from stdin: " + std::string(e.what()));
        return false;
    }
}

} // namespace wal

