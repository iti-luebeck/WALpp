#include "wal/cli.h"
#include "wal/bigint.h"
#include "wal/core.h"
#include "wal/repl.h"
#include "wal/value_printer.h"
#include "wal/trace/streaming_config.h"
#include "wal/fast/vcd_parser.h"
#include "wal/fast/hamming_ops.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <unistd.h>
#include <chrono>

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
        } else if (arg == "--fast-hd-all") {
            fast_hd_all = true;
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "--fast-hd-scope") {
            fast_hd_scope = true;
            if (i + 1 < args.size()) fast_scope = args[++i];
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "--bigint") {
            // Opt-in arbitrary-precision integers. Off by default: 64-bit is what
            // waveform values and indices need, and the bigint path is slower.
            set_bigint_mode(true);
        } else if (arg == "--fast-hw-all") {
            fast_hw_all = true;
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "--fast-hw-scope") {
            fast_hw_scope = true;
            if (i + 1 < args.size()) fast_scope = args[++i];
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "--fast-hd-signals") {
            fast_hd_signals = true;
            if (i + 1 < args.size()) fast_signals_file = args[++i];
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "--fast-hw-signals") {
            fast_hw_signals = true;
            if (i + 1 < args.size()) fast_signals_file = args[++i];
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "--fast-values-signals") {
            fast_values_signals = true;
            if (i + 1 < args.size()) fast_signals_file = args[++i];
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "--fast-list") {
            fast_list = true;
            if (i + 1 < args.size()) fast_vcd_file = args[++i];
            if (i + 1 < args.size() && args[i + 1] == "-o") {
                ++i;
                if (i + 1 < args.size()) fast_output_file = args[++i];
            }
        } else if (arg == "-e" || arg == "--eval") {
            eval_mode = true;
            if (i + 1 < args.size()) {
                // Accumulate: several -e run in order and share one interpreter,
                // so (define ...) in an earlier -e is visible to a later one.
                eval_exprs.push_back(args[++i]);
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
              << "FAST MODE (optimized HD/HW calculation with CSV output):\n"
              << "  --fast-hd-all FILE -o OUT.csv       Hamming Distance (all signals)\n"
              << "  --fast-hd-scope SCOPE FILE -o OUT   Hamming Distance (scoped)\n"
              << "  --fast-hd-signals LIST FILE -o OUT  Hamming Distance (signal list)\n"
              << "  --bigint                            Arbitrary-precision integers\n"
              << "                                      (default is 64-bit, which is faster)\n"
              << "  --fast-hw-all FILE -o OUT.csv       Hamming Weight (all signals)\n"
              << "  --fast-hw-scope SCOPE FILE -o OUT   Hamming Weight (scoped)\n"
              << "  --fast-hw-signals LIST FILE -o OUT  Hamming Weight (signal list)\n"
              << "  --fast-values-signals LIST FILE -o OUT  Raw value-change rows (timestamp,index,value)\n"
              << "                                      for the listed signals (1-bit AND buses)\n"
              << "  --fast-list FILE -o OUT             List signals (width name) + a # timescale header\n"
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
    std::cout << "WAL++ v1.2.2\n"
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

int CLI::run_fast_mode() {
    using namespace std::chrono;
    
    // Helper lambda to format duration
    auto format_duration = [](int64_t ms) -> std::string {
        if (ms < 1000) {
            return std::to_string(ms) + "ms";
        } else if (ms < 60000) {
            double seconds = ms / 1000.0;
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << seconds << "s";
            return oss.str();
        } else if (ms < 3600000) {
            int minutes = ms / 60000;
            double seconds = (ms % 60000) / 1000.0;
            std::ostringstream oss;
            oss << minutes << "m " << std::fixed << std::setprecision(1) << seconds << "s";
            return oss.str();
        } else {
            int hours = ms / 3600000;
            int minutes = (ms % 3600000) / 60000;
            int seconds = (ms % 60000) / 1000;
            std::ostringstream oss;
            oss << hours << "h " << minutes << "m " << seconds << "s";
            return oss.str();
        }
    };
    
    if (fast_vcd_file.empty() || fast_output_file.empty()) {
        std::cerr << "Error: VCD file and output file required for fast mode\n";
        print_usage();
        return 1;
    }
    
    try {
        // Detect file format by extension
        auto ends_with = [](const std::string& s, const std::string& suffix) {
            return s.size() >= suffix.size() &&
                   s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
        };
        const bool is_fst = ends_with(fast_vcd_file, ".fst");

        // Parse VCD or FST
        auto parse_start = high_resolution_clock::now();

        fast_vcd::VCDParser parser;

        if (fast_hd_scope || fast_hw_scope) {
            if (fast_scope.empty()) {
                std::cerr << "Error: Scope required for scoped operations\n";
                return 1;
            }
            if (is_fst) {
                std::cerr << "Parsing FST with scope filter: " << fast_scope << "...\n";
                parser.parse_fst(fast_vcd_file, fast_scope);
            } else {
                std::cerr << "Parsing VCD with scope filter: " << fast_scope << "...\n";
                parser.parse(fast_vcd_file, fast_scope);
            }
        } else if (fast_hd_signals || fast_hw_signals || fast_values_signals) {
            if (fast_signals_file.empty()) {
                std::cerr << "Error: Signal list file required for signal list operations\n";
                return 1;
            }
            if (is_fst) {
                std::cerr << "Parsing FST (will filter by signal list later)...\n";
                parser.parse_fst(fast_vcd_file);
            } else {
                std::cerr << "Parsing VCD (will filter by signal list later)...\n";
                parser.parse(fast_vcd_file);
            }
        } else {
            if (is_fst) {
                std::cerr << "Parsing FST...\n";
                parser.parse_fst(fast_vcd_file);
            } else {
                std::cerr << "Parsing VCD...\n";
                parser.parse(fast_vcd_file);
            }
        }
        
        auto parse_end = high_resolution_clock::now();
        auto parse_ms = duration_cast<milliseconds>(parse_end - parse_start).count();
        
        std::cerr << "Parsed " << parser.num_signals() << " signals, "
                  << parser.num_timestamps() << " timestamps in "
                  << format_duration(parse_ms) << "\n";
        
        // Perform operation
        auto op_start = high_resolution_clock::now();
        
        fast_vcd::HammingOps ops(parser);
        fast_vcd::Result results;
        
        // Read signal list if needed
        std::vector<std::string> signal_names;
        if (fast_hd_signals || fast_hw_signals || fast_values_signals) {
            std::ifstream sigfile(fast_signals_file);
            if (!sigfile) {
                std::cerr << "Error: Could not open signal list file: " << fast_signals_file << "\n";
                return 1;
            }
            
            std::string line;
            while (std::getline(sigfile, line)) {
                // Trim whitespace
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);
                
                // Skip empty lines and comments
                if (!line.empty() && line[0] != '#') {
                    signal_names.push_back(line);
                }
            }
            sigfile.close();
            
            std::cerr << "Loaded " << signal_names.size() << " signal names from list\n";
        }
        
        // --fast-list: emit the timescale + (width, name) for every signal. The
        // companion timing tools resolve a substring against this and convert
        // tick timestamps to ns. (No op; written and returned here.)
        if (fast_list) {
            std::ofstream out(fast_output_file);
            if (!out) {
                std::cerr << "Error: Could not open output file: " << fast_output_file << "\n";
                return 1;
            }
            out << "# timescale " << parser.get_timescale() << "\n";
            for (uint32_t id : parser.get_signal_ids()) {
                const auto& info = parser.get_signal_info(id);
                out << info.width << " " << info.name << "\n";
            }
            std::cerr << "Listed " << parser.num_signals() << " signals to " << fast_output_file << "\n";
            return 0;
        }

        // --fast-values-signals: raw per-signal value-change rows
        // "timestamp,list-index,value" (+ a "# timescale ..." header), for bus
        // value queries where a popcount (HW/HD) is not the value.
        if (fast_values_signals) {
            fast_vcd::ValueRows rows = ops.values_signals(signal_names);
            std::ofstream out(fast_output_file);
            if (!out) {
                std::cerr << "Error: Could not open output file: " << fast_output_file << "\n";
                return 1;
            }
            out << "# timescale " << parser.get_timescale() << "\n";
            for (const auto& [ts, idx, val] : rows) {
                out << ts << "," << idx << "," << val << "\n";
            }
            std::cerr << "Wrote " << rows.size() << " value rows to " << fast_output_file << "\n";
            return 0;
        }

        if (fast_hd_all) {
            std::cerr << "Computing Hamming Distance (all signals)...\n";
            results = ops.hamming_distance_all();
        } else if (fast_hd_scope) {
            std::cerr << "Computing Hamming Distance (scope: " << fast_scope << ")...\n";
            results = ops.hamming_distance_scope(fast_scope);
        } else if (fast_hd_signals) {
            std::cerr << "Computing Hamming Distance (" << signal_names.size() << " signals)...\n";
            results = ops.hamming_distance_signals(signal_names);
        } else if (fast_hw_all) {
            std::cerr << "Computing Hamming Weight (all signals)...\n";
            results = ops.hamming_weight_all();
        } else if (fast_hw_scope) {
            std::cerr << "Computing Hamming Weight (scope: " << fast_scope << ")...\n";
            results = ops.hamming_weight_scope(fast_scope);
        } else if (fast_hw_signals) {
            std::cerr << "Computing Hamming Weight (" << signal_names.size() << " signals)...\n";
            results = ops.hamming_weight_signals(signal_names);
        }
        
        auto op_end = high_resolution_clock::now();
        auto op_ms = duration_cast<milliseconds>(op_end - op_start).count();
        
        std::cerr << "Computation completed in " << format_duration(op_ms) << "\n";
        std::cerr << "Total time: " << format_duration(parse_ms + op_ms) << "\n";
        
        // Write results to CSV
        std::ofstream outfile(fast_output_file);
        if (!outfile) {
            std::cerr << "Error: Could not open output file: " << fast_output_file << "\n";
            return 1;
        }
        
        for (const auto& result : results) {
            outfile << result.first << "," << result.second << "\n";
        }
        outfile.close();
        
        std::cerr << "Results written to: " << fast_output_file 
                  << " (" << results.size() << " entries)\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
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
    
    // Check for fast mode first (highest priority)
    if (fast_hd_all || fast_hd_scope || fast_hw_all || fast_hw_scope ||
        fast_hd_signals || fast_hw_signals || fast_values_signals || fast_list) {
        return run_fast_mode();
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
    if (eval_mode && !eval_exprs.empty()) {
        for (const auto& expr : eval_exprs) {
            try {
                Value result = wal_instance->eval_str(expr);

                // Print result. Unlike the REPL, -e echoes nil, so a caller can
                // tell "evaluated to nothing" from "produced no output". Strings
                // print bare so a shell can consume them. Lists and symbols are
                // printed by the shared renderer: they used to fall off the end
                // of this chain and vanish, which made `-e '(signal-names)'`
                // look like an empty trace.
                if (result.is_nil()) {
                    std::cout << "nil" << std::endl;
                } else {
                    print_value(result, std::cout, /*quote_strings=*/false);
                    std::cout << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
                return 1;
            }
        }
        return 0;
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
