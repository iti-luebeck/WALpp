#pragma once

#include <string>
#include <vector>
#include <memory>

namespace wal {

class Wal;
class REPL;

/**
 * @brief Command-line interface for WAL++ with argument parsing
 * 
 * Handles:
 * - Argument parsing
 * - File execution
 * - Expression evaluation
 * - REPL startup
 * - Help and version info
 */
class CLI {
private:
    std::string program_name;
    std::vector<std::string> args;
    std::shared_ptr<Wal> wal_instance;
    std::shared_ptr<REPL> repl_instance;
    
    // Command flags
    bool help_flag = false;
    bool version_flag = false;
    bool eval_mode = false;
    bool file_mode = false;
    bool trace_mode = false;
    bool pipe_mode = false;
    bool stdin_mode = false;
    bool streaming_enabled = true;  // Streaming auto-detection (default: enabled)
    bool streaming_explicit = false;  // User explicitly set streaming
    
    // Fast mode flags
    bool fast_hd_all = false;
    bool fast_hd_scope = false;
    bool fast_hw_all = false;
    bool fast_hw_scope = false;
    bool fast_hd_signals = false;
    bool fast_hw_signals = false;
    bool fast_values_signals = false;  // raw per-signal value series (changes only)
    bool fast_list = false;            // list signals (width + name) + timescale
    std::string fast_scope;
    std::string fast_vcd_file;
    std::string fast_output_file;
    std::string fast_signals_file;
    
    std::vector<std::string> eval_exprs;   // every -e, in command-line order
    std::string filename;
    std::string trace_file;
    std::string pipe_file;
    size_t chunk_size_bytes = 100 * 1024 * 1024;  // Default 100 MB
    size_t cache_capacity = 1000;  // Default 1000 entries per signal
    
    // Parsing
    bool parse_arguments(int argc, const char* argv[]);
    void print_help();
    void print_version();
    void print_usage();
    void apply_streaming_config();
    int run_fast_mode();
    
public:
    /**
     * @brief Initialize CLI with command-line arguments
     * 
     * @param argc Argument count
     * @param argv Argument values
     */
    CLI(int argc, const char* argv[]);
    
    /**
     * @brief Run the CLI application
     * 
     * Dispatches to appropriate mode:
     * - REPL if no arguments
     * - File execution with -f or filename
     * - Expression evaluation with -e
     * - Pipe file input with --pipe FILE
     * - Standard input with --stdin
     * - Help/version if requested
     * 
     * Input modes:
     * - --pipe FILE            Read trace data from named pipe file
     * - --stdin                Read trace data from standard input
     * 
     * Streaming options:
     * - --streaming            Enable streaming for large files (auto-detection)
     * - --no-streaming         Disable streaming (always use full-load)
     * - --chunk-size SIZE      Set chunk size in MB (default: 100)
     * - --cache-capacity NUM   Set cache entries per signal (default: 1000)
     * - --cache-stats          Show cache statistics after loading
     * 
     * @return Exit code (0 = success, non-zero = error)
     */
    int run();
    
    /**
     * @brief Get WAL instance
     */
    std::shared_ptr<Wal> get_wal() const { return wal_instance; }
    
    /**
     * @brief Get REPL instance
     */
    std::shared_ptr<REPL> get_repl() const { return repl_instance; }
};

} // namespace wal
