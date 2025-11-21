#include "wal/cli.h"
#include "wal/core.h"
#include "wal/repl.h"
#include "wal/trace/streaming_config.h"
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
        } else if (arg == "--pipe") {
            pipe_mode = true;
            if (i + 1 < args.size()) {
                pipe_file = args[++i];
            }
        } else if (arg == "--stdin") {
            stdin_mode = true;
        } else if (arg == "--streaming") {
            streaming_enabled = true;
            streaming_explicit = true;
        } else if (arg == "--no-streaming") {
            streaming_enabled = false;
            streaming_explicit = true;
        } else if (arg == "--chunk-size") {
            if (i + 1 < args.size()) {
                try {
                    size_t size_mb = std::stoul(args[++i]);
                    chunk_size_bytes = size_mb * 1024 * 1024;
                } catch (...) {
                    std::cerr << "Invalid chunk size: " << args[i] << std::endl;
                }
            }
        } else if (arg == "--cache-capacity") {
            if (i + 1 < args.size()) {
                try {
                    cache_capacity = std::stoul(args[++i]);
                } catch (...) {
                    std::cerr << "Invalid cache capacity: " << args[i] << std::endl;
                }
            }
        } else if (arg == "--cache-stats") {
            // This flag will be handled during trace loading
            // Store it for later use in run()
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
              << "  -h, --help                  Show this help message\n"
              << "  -v, --version               Show version information\n"
              << "  -e, --eval EXPR             Evaluate expression\n"
              << "  -f, --file FILE             Execute WAL program file\n"
              << "  -l, --load FILE             Load waveform trace file (for analysis)\n"
              << "\n"
              << "INPUT MODES:\n"
              << "  --pipe FILE                 Read trace data from named pipe file\n"
              << "  --stdin                     Read trace data from standard input\n"
              << "\n"
              << "STREAMING OPTIONS:\n"
              << "  --streaming                 Enable streaming (auto-detection for files > 100 MB)\n"
              << "  --no-streaming              Disable streaming (always use full-load)\n"
              << "  --chunk-size SIZE           Set chunk size in MB (default: 100)\n"
              << "  --cache-capacity NUM        Set cache entries per signal (default: 1000)\n"
              << "  --cache-stats               Show cache statistics after loading trace\n"
              << "\n"
              << "EXAMPLES:\n"
              << "  " << program_name << "                              Start interactive REPL\n"
              << "  " << program_name << " program.wal                    Execute program\n"
              << "  " << program_name << " -e '(+ 1 2)'                  Evaluate expression\n"
              << "  " << program_name << " -l signal.dat                 Load and analyze trace\n"
              << "  " << program_name << " -l huge.vcd --streaming       Load with streaming enabled\n"
              << "  " << program_name << " -l huge.vcd --no-streaming    Load without streaming\n"
              << "  " << program_name << " -l file.csv --chunk-size 50   Load with 50 MB chunks\n"
              << "  " << program_name << " -l file.vcd --cache-stats     Show cache statistics\n"
              << "  " << program_name << " --pipe /tmp/trace.fifo        Read from named pipe\n"
              << "  " << program_name << " --stdin                       Read from standard input\n"
              << "  simulator | " << program_name << " --stdin            Pipe simulation output directly\n"
              << "\n";
}

void CLI::print_help() {
    print_usage();
    std::cout << "For interactive help, start the REPL and type (help)\n";
}

void CLI::print_version() {
    std::cout << "WAL++ v0.1.0\n"
              << "Language: S-expressions with 130+ operators\n"
              << "Build: C++20 with pybind11 Python bindings\n"
              << "Features: Streaming trace support (VCD, CSV, FST)\n";
}

void CLI::apply_streaming_config() {
    // Get the global trace container from the WAL instance
    // Configure streaming options before any trace loading
    if (wal_instance) {
        auto container = wal_instance->get_traces();
        if (container) {
            // Apply streaming enable/disable if explicitly set
            if (streaming_explicit) {
                container->set_streaming_enabled(streaming_enabled);
            }
            
            // Create and apply configuration
            StreamingConfig config;
            config.enabled = streaming_enabled;
            config.chunk_size_bytes = chunk_size_bytes;
            config.cache_capacity_per_signal = cache_capacity;
            config.report_cache_stats = false;  // Stats will be printed manually if requested
            
            container->set_streaming_config(config);
        }
    }
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
    
    // Apply streaming configuration early
    apply_streaming_config();
    
    // Load trace file if specified
    if (trace_mode && !trace_file.empty()) {
        try {
            // Disable streaming for now - hamming operators require full trace access
            wal_instance->get_traces()->set_streaming_enabled(false);
            wal_instance->load(trace_file);
        } catch (const std::exception& e) {
            std::cerr << "Error loading trace file: " << e.what() << std::endl;
            return 1;
        }
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
    
    // Pipe mode
    if (pipe_mode && !pipe_file.empty()) {
        return repl_instance->load_pipe(pipe_file) ? 0 : 1;
    }
    
    // Stdin mode
    if (stdin_mode) {
        return repl_instance->read_from_stdin() ? 0 : 1;
    }
    
    // Default: REPL mode
    return repl_instance->run();
}

} // namespace wal
