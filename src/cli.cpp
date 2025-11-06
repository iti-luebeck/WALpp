#include "wal/cli.h"
#include "wal/core.h"
#include "wal/repl.h"
#include <iostream>
#include <cstring>
#include <unistd.h>

namespace wal {

CLI::CLI(int argc, const char* argv[])
    : program_name(argv[0]) {
    // Convert to vector for easier handling
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    
    wal_instance = std::make_shared<Wal>();
    repl_instance = std::make_shared<REPL>(wal_instance);
}

bool CLI::parse_arguments(int argc, const char* argv[]) {
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        
        if (arg == "-h" || arg == "--help") {
            help_flag = true;
        } else if (arg == "-v" || arg == "--version") {
            version_flag = true;
        } else if (arg == "-e" || arg == "--eval") {
            eval_mode = true;
            if (i + 1 < args.size()) {
                eval_expr = args[++i];
            }
        } else if (arg == "-f" || arg == "--file") {
            file_mode = true;
            if (i + 1 < args.size()) {
                filename = args[++i];
            }
        } else if (arg == "-l" || arg == "--load") {
            trace_mode = true;
            if (i + 1 < args.size()) {
                trace_file = args[++i];
            }
        } else if (arg[0] != '-') {
            // Positional argument - assume it's a file
            file_mode = true;
            filename = arg;
        }
    }
    
    return true;
}

void CLI::print_usage() {
    std::cout << "Usage: " << program_name << " [OPTIONS] [FILE]\n"
              << "\n"
              << "OPTIONS:\n"
              << "  -h, --help              Show this help message\n"
              << "  -v, --version           Show version information\n"
              << "  -e, --eval EXPR         Evaluate expression\n"
              << "  -f, --file FILE         Execute WAL program file\n"
              << "  -l, --load FILE         Load waveform trace file (for analysis)\n"
              << "\n"
              << "EXAMPLES:\n"
              << "  " << program_name << "                    Start interactive REPL\n"
              << "  " << program_name << " program.wal          Execute program\n"
              << "  " << program_name << " -e '(+ 1 2)'        Evaluate expression\n"
              << "  " << program_name << " -l signal.dat        Load and analyze trace\n"
              << "\n";
}

void CLI::print_help() {
    print_usage();
    std::cout << "For interactive help, start the REPL and type (help)\n";
}

void CLI::print_version() {
    std::cout << "WAL++ v0.1.0\n"
              << "Language: S-expressions with 130+ operators\n"
              << "Build: C++20 with pybind11 Python bindings\n";
}

int CLI::run() {
    parse_arguments(args.size(), nullptr);
    
    // Handle flags
    if (help_flag) {
        print_help();
        return 0;
    }
    
    if (version_flag) {
        print_version();
        return 0;
    }
    
    // Eval mode
    if (eval_mode && !eval_expr.empty()) {
        try {
            Value result = wal_instance->eval_str(eval_expr);
            
            // Print result
            if (result.is_nil()) {
                std::cout << "nil" << std::endl;
            } else if (result.is_bool()) {
                std::cout << (result.to_bool() ? "true" : "false") << std::endl;
            } else if (result.is_int()) {
                std::cout << result.to_int() << std::endl;
            } else if (result.is_float()) {
                std::cout << result.to_float() << std::endl;
            } else if (result.is_string()) {
                std::cout << result.to_string() << std::endl;
            }
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    }
    
    // File mode
    if (file_mode && !filename.empty()) {
        return repl_instance->load_file(filename) ? 0 : 1;
    }
    
    // Default: REPL mode
    return repl_instance->run();
}

} // namespace wal
