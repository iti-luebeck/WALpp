#pragma once

#include "core.h"
#include <string>
#include <vector>
#include <memory>
#include <deque>

namespace wal {

/**
 * @brief Interactive Read-Eval-Print-Loop for WAL++
 * 
 * Provides a user-friendly interactive shell for WAL expressions with:
 * - Command history and navigation
 * - Multi-line expression support
 * - Pretty-printed output
 * - Built-in help system
 * - Error recovery
 */
class REPL {
private:
    std::shared_ptr<Wal> wal_instance;
    std::deque<std::string> history;
    const size_t MAX_HISTORY = 1000;
    bool running = true;
    
    // REPL state
    int trace_count = 0;
    
    // Internal helpers
    std::string read_input(const std::string& prompt = "wal> ");
    void print_output(const Value& result);
    void print_error(const std::string& error);
    void print_welcome();
    void print_help();
    void handle_command(const std::string& line);
    bool is_command(const std::string& line) const;
    void save_history();
    void load_history();
    std::string load_history_file() const;
    
public:
    /**
     * @brief Create REPL with existing Wal instance
     */
    explicit REPL(std::shared_ptr<Wal> wal = nullptr);
    
    /**
     * @brief Start interactive REPL loop
     * 
     * Runs until user types (quit) or EOF
     * @return Exit code (0 = normal, non-zero = error)
     */
    int run();
    
    /**
     * @brief Execute single expression
     * 
     * @param expr The expression to evaluate
     * @return true if execution succeeded
     */
    bool execute(const std::string& expr);
    
    /**
     * @brief Load and execute a file
     * 
     * @param filename Path to WAL program
     * @return true if successful
     */
    bool load_file(const std::string& filename);
    
    /**
     * @brief Read and execute from a named pipe (FIFO)
     * 
     * @param pipe_path Path to the named pipe file
     * @return true if successful
     */
    bool load_pipe(const std::string& pipe_path);
    
    /**
     * @brief Read and execute from standard input
     * 
     * Reads input from stdin until EOF and executes all input.
     * Useful for piping simulation output directly.
     * 
     * @return true if successful
     */
    bool read_from_stdin();
    
    /**
     * @brief Get the WAL instance
     */
    std::shared_ptr<Wal> get_wal() const { return wal_instance; }
    
    /**
     * @brief Stop the REPL
     */
    void stop() { running = false; }
    
    /**
     * @brief Get command history
     */
    const std::deque<std::string>& get_history() const { return history; }
};

} // namespace wal
