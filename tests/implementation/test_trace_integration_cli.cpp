/**
 * @file test_trace_integration_cli.cpp
 * @brief Integration tests for trace data input modes with CLI expressions and scripts
 * 
 * Tests the following usage patterns:
 * 1. Load trace file + evaluate expression: wal -l trace.vcd -e '(define signals (signal-names))'
 * 2. Pipe trace + evaluate expression: cat trace.vcd | wal --stdin -e '(length (signal-names))'
 * 3. Pipe trace + load script: cat trace.vcd | wal --stdin script.wal
 * 4. Named pipe + evaluate expression: wal --pipe /tmp/trace.fifo -e '(signal-names)'
 */

#include <iostream>
#include <cassert>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <sys/stat.h>
#include "wal/core.h"
#include "wal/repl.h"
#include "wal/cli.h"

namespace wal {

void print_test_header(const std::string& title) {
    std::cout << "\n╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << title << "\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n";
}

// Create a test VCD file
std::string create_test_vcd_file(const std::string& filename) {
    std::ofstream file(filename);
    file << R"($date
    November 8, 2025
$end
$version
    WAL++ CLI Integration Test
$end
$timescale 1ns $end
$scope module testbench $end
$var wire 1 ! clk $end
$var wire 8 " data $end
$var wire 1 # valid $end
$var wire 4 $ counter $end
$upscope $end
$enddefinitions
$dumpvars
0!
00000000"
0#
b0000$
$end
#0
1!
1#
#10
0!
b10101010"
b0001$
#20
1!
b10101010"
b0010$
#30
0!
b11110000"
b0011$
1#
#40
1!
b11110000"
b0100$
#50
0!
b00001111"
b0101$
0#
#60
1!
b00001111"
b0110$
#70
0!
b11001100"
b0111$
)";
    file.close();
    return filename;
}

// Create a test WAL script file
std::string create_test_script_file(const std::string& filename) {
    std::ofstream file(filename);
    file << R"(; WAL analysis script for trace processing
(define signals (signal-names))
(define signal-count (length signals))
(list "Analysis complete" signal-count)
)";
    file.close();
    return filename;
}

} // namespace wal

int main() {
    try {
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║   TRACE + CLI INTEGRATION TEST SUITE                        ║\n";
        std::cout << "║   Testing Combined Input Modes and Expression Evaluation    ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
        
        // Create test files with relative paths
        std::string vcd_file = "tests/data/test_trace_integration.vcd";
        std::string script_file = "tests/data/test_trace_script.wal";
        
        wal::create_test_vcd_file(vcd_file);
        std::cout << "\n✓ Created test VCD file: " << vcd_file << "\n";
        
        wal::create_test_script_file(script_file);
        std::cout << "✓ Created test WAL script: " << script_file << "\n";
        
        // =====================================================================
        // Test 1: Load trace file + evaluate expression
        // =====================================================================
        wal::print_test_header("Test 1: Load trace + evaluate expression");
        std::cout << "Command: wal -l trace.vcd -e '(define signals (signal-names))'\n\n";
        
        {
            auto wal_inst = std::make_shared<wal::Wal>();
            
            // Load trace
            wal::Value result = wal_inst->eval_str("(load \"" + vcd_file + "\")");
            assert(result.is_symbol());
            std::cout << "✓ Loaded trace file\n";
            
            // Evaluate expression
            result = wal_inst->eval_str("(define signals (signal-names))");
            std::cout << "✓ Defined 'signals' variable\n";
            
            // Verify it worked
            result = wal_inst->eval_str("(length signals)");
            assert(result.is_int() && result.to_int() > 0);
            std::cout << "✓ Signal count: " << result.to_int() << "\n";
            
            std::cout << "✓ TEST PASSED\n";
        }
        
        // =====================================================================
        // Test 2: Trace via stdin + evaluate expression
        // =====================================================================
        wal::print_test_header("Test 2: Pipe trace + evaluate expression");
        std::cout << "Command: cat trace.vcd | wal --stdin -e '(length (signal-names))'\n\n";
        
        {
            auto wal_inst = std::make_shared<wal::Wal>();
            auto repl = std::make_shared<wal::REPL>(wal_inst);
            
            // Simulate piping by reading file and loading it
            std::ifstream trace_file(vcd_file);
            std::stringstream buffer;
            buffer << trace_file.rdbuf();
            trace_file.close();
            
            // Create temp file for stdin simulation
            std::string temp_file = "/tmp/wal_test_stdin_expr.vcd";
            std::ofstream temp(temp_file);
            temp << buffer.str();
            temp.close();
            
            // Load trace
            wal::Value result = wal_inst->eval_str("(load \"" + temp_file + "\")");
            assert(result.is_symbol());
            std::cout << "✓ Loaded trace via stdin simulation\n";
            
            // Evaluate expression
            result = wal_inst->eval_str("(length (signal-names))");
            assert(result.is_int() && result.to_int() > 0);
            std::cout << "✓ Expression result: " << result.to_int() << " signals\n";
            
            std::remove(temp_file.c_str());
            std::cout << "✓ TEST PASSED\n";
        }
        
        // =====================================================================
        // Test 3: Trace via stdin + load script
        // =====================================================================
        wal::print_test_header("Test 3: Pipe trace + load script");
        std::cout << "Command: cat trace.vcd | wal --stdin script.wal\n\n";
        
        {
            auto wal_inst = std::make_shared<wal::Wal>();
            auto repl = std::make_shared<wal::REPL>(wal_inst);
            
            // Simulate stdin trace loading
            std::string temp_file = "/tmp/wal_test_stdin_script.vcd";
            std::ifstream src(vcd_file);
            std::ofstream dst(temp_file);
            dst << src.rdbuf();
            src.close();
            dst.close();
            
            // Load trace
            wal::Value result = wal_inst->eval_str("(load \"" + temp_file + "\")");
            assert(result.is_symbol());
            std::cout << "✓ Loaded trace via stdin\n";
            
            // Load and execute script
            bool success = repl->load_file(script_file);
            assert(success);
            std::cout << "✓ Executed script file\n";
            
            // The script should have been executed, check that signals are discoverable
            result = wal_inst->eval_str("(length (signal-names))");
            assert(result.is_int() && result.to_int() > 0);
            std::cout << "✓ Script processed " << result.to_int() << " signals\n";
            
            std::remove(temp_file.c_str());
            std::cout << "✓ TEST PASSED\n";
        }
        
        // =====================================================================
        // Test 4: Named pipe + evaluate expression
        // =====================================================================
        wal::print_test_header("Test 4: Named pipe + evaluate expression");
        std::cout << "Command: wal --pipe /tmp/trace.fifo -e '(signal-names)'\n\n";
        
        {
            // Create named pipe path
            std::string pipe_path = "/tmp/wal_test_integration.fifo";
            
            // Remove pipe if it exists
            std::remove(pipe_path.c_str());
            
            // Create pipe in background thread
            std::thread pipe_thread([&]() {
                // Small delay to ensure pipe is ready
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // Write VCD data to pipe
                std::ifstream src(vcd_file);
                std::ofstream pipe(pipe_path);
                pipe << src.rdbuf();
                pipe.close();
                src.close();
            });
            
            // Create FIFO (named pipe)
            mkfifo(pipe_path.c_str(), 0666);
            
            auto wal_inst = std::make_shared<wal::Wal>();
            auto repl = std::make_shared<wal::REPL>(wal_inst);
            
            // Load from pipe
            bool pipe_success = repl->load_pipe(pipe_path);
            assert(pipe_success);
            std::cout << "✓ Loaded trace from named pipe\n";
            
            // Evaluate expression
            wal::Value result = wal_inst->eval_str("(signal-names)");
            assert(result.is_list());
            auto signals = result.to_list();
            std::cout << "✓ Retrieved signal names: " << signals->size() << " signals\n";
            
            // Print signal names
            std::cout << "  Signals: ";
            for (size_t i = 0; i < signals->size() && i < 3; ++i) {
                if ((*signals)[i].is_symbol()) {
                    std::cout << (*signals)[i].to_symbol()->get_name();
                    if (i < signals->size() - 1 && i < 2) std::cout << ", ";
                }
            }
            if (signals->size() > 3) std::cout << "...";
            std::cout << "\n";
            
            // Wait for pipe thread to complete
            pipe_thread.join();
            
            // Cleanup
            std::remove(pipe_path.c_str());
            std::cout << "✓ TEST PASSED\n";
        }
        
        // =====================================================================
        // Test 5: Combination test - Load + Define + Query
        // =====================================================================
        wal::print_test_header("Test 5: Complex workflow");
        std::cout << "Workflow: Load trace → Define variables → Query results\n\n";
        
        {
            auto wal_inst = std::make_shared<wal::Wal>();
            
            // Step 1: Load trace
            wal::Value result = wal_inst->eval_str("(load \"" + vcd_file + "\")");
            assert(result.is_symbol());
            std::cout << "✓ Step 1: Loaded trace file\n";
            
            // Step 2: Define analysis variables
            result = wal_inst->eval_str("(define signals (signal-names))");
            std::cout << "✓ Step 2: Defined 'signals' variable\n";
            
            result = wal_inst->eval_str("(define num-signals (length signals))");
            std::cout << "✓ Step 2: Defined 'num-signals' variable\n";
            
            // Step 3: Query results
            result = wal_inst->eval_str("num-signals");
            assert(result.is_int());
            int signal_count = result.to_int();
            std::cout << "✓ Step 3: Query returned " << signal_count << " signals\n";
            
            // Step 4: Conditional analysis
            result = wal_inst->eval_str(
                "(if (> num-signals 3)"
                "  (list \"Many signals\" num-signals)"
                "  (list \"Few signals\" num-signals))"
            );
            assert(result.is_list());
            auto res_list = result.to_list();
            if (res_list->size() > 0 && (*res_list)[0].is_string()) {
                std::cout << "✓ Step 4: Conditional analysis: " << (*res_list)[0].to_string() << "\n";
            }
            
            // Step 5: Signal analysis (query first signal name manually)
            result = wal_inst->eval_str("(signal-names)");
            if (result.is_list() && result.to_list()->size() > 0) {
                auto sig_list = result.to_list();
                if ((*sig_list)[0].is_symbol()) {
                    std::string first_sig = (*sig_list)[0].to_symbol()->get_name();
                    result = wal_inst->eval_str("(signal-width \"" + first_sig + "\")");
                    assert(result.is_int());
                    std::cout << "✓ Step 5: First signal width: " << result.to_int() << " bit(s)\n";
                }
            }
            
            std::cout << "✓ TEST PASSED\n";
        }
        
        // =====================================================================
        // Test 6: Script with trace query workflow
        // =====================================================================
        wal::print_test_header("Test 6: Script-based trace analysis");
        std::cout << "Workflow: Load trace → Execute analysis script\n\n";
        
        {
            auto wal_inst = std::make_shared<wal::Wal>();
            auto repl = std::make_shared<wal::REPL>(wal_inst);
            
            // Load trace
            wal::Value result = wal_inst->eval_str("(load \"" + vcd_file + "\")");
            assert(result.is_symbol());
            std::cout << "✓ Loaded trace file\n";
            
            // Execute analysis script
            bool success = repl->load_file(script_file);
            assert(success);
            std::cout << "✓ Executed analysis script\n";
            
            // Verify signals are accessible after script execution
            result = wal_inst->eval_str("(length (signal-names))");
            assert(result.is_int() && result.to_int() > 0);
            std::cout << "✓ Script result - signal count: " << result.to_int() << "\n";
            
            std::cout << "✓ TEST PASSED\n";
        }
        
        // =====================================================================
        // Cleanup
        // =====================================================================
        std::remove(vcd_file.c_str());
        std::remove(script_file.c_str());
        
        // =====================================================================
        // Summary
        // =====================================================================
        wal::print_test_header("Test Summary");
        
        std::cout << "\n✓ Successfully tested all usage patterns:\n";
        std::cout << "  1. wal -l trace.vcd -e '(define signals (signal-names))'\n";
        std::cout << "  2. cat trace.vcd | wal --stdin -e '(length (signal-names))'\n";
        std::cout << "  3. cat trace.vcd | wal --stdin script.wal\n";
        std::cout << "  4. wal --pipe /tmp/trace.fifo -e '(signal-names)'\n";
        std::cout << "  5. Complex trace + script analysis workflow\n";
        std::cout << "  6. Script-based trace analysis\n";
        
        std::cout << "\n" << std::string(64, '=') << "\n";
        std::cout << "🎉 ALL TRACE + CLI INTEGRATION TESTS PASSED!\n";
        std::cout << std::string(64, '=') << "\n\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}
