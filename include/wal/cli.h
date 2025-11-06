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
    
    std::string eval_expr;
    std::string filename;
    std::string trace_file;
    
    // Parsing
    bool parse_arguments(int argc, const char* argv[]);
    void print_help();
    void print_version();
    void print_usage();
    
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
     * - Help/version if requested
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
