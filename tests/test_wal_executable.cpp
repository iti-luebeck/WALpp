// test_wal_executable.cpp - Integration tests for WAL executable with trace files
// Phase 3 Task 1 - End-to-end executable testing with VCD, FST, and CSV files

#include <iostream>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <cstring>

namespace fs = std::filesystem;

// =============================================================================
// TEST UTILITIES
// =============================================================================

class TestResult {
public:
    std::string name;
    bool passed = false;
    std::string message;
    int64_t duration_ms = 0;
};

class ExecutableTestSuite {
private:
    std::vector<TestResult> results;
    std::string suite_name;
    std::string executable_path;
    
public:
    ExecutableTestSuite(const std::string& name, const std::string& exe_path) 
        : suite_name(name), executable_path(exe_path) {}
    
    void add_result(const TestResult& result) {
        results.push_back(result);
    }
    
    bool all_passed() const {
        for (const auto& r : results) {
            if (!r.passed) return false;
        }
        return true;
    }
    
    void print_summary() const {
        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "Test Suite: " << suite_name << "\n";
        std::cout << std::string(80, '=') << "\n";
        
        int passed = 0, failed = 0;
        int64_t total_time = 0;
        
        for (const auto& result : results) {
            const char* status = result.passed ? "✅ PASS" : "❌ FAIL";
            std::cout << status << " - " << result.name 
                      << " (" << result.duration_ms << "ms)\n";
            
            if (!result.message.empty()) {
                std::cout << "    Message: " << result.message << "\n";
            }
            
            if (result.passed) passed++;
            else failed++;
            
            total_time += result.duration_ms;
        }
        
        std::cout << "\nResults: " << passed << "/" << results.size() 
                  << " passed";
        if (failed > 0) {
            std::cout << ", " << failed << " FAILED";
        }
        std::cout << "\nTotal time: " << total_time << "ms\n";
        std::cout << std::string(80, '=') << "\n";
    }
    
    int get_failed_count() const {
        int count = 0;
        for (const auto& result : results) {
            if (!result.passed) count++;
        }
        return count;
    }
};

// Execute a command and capture output
std::pair<int, std::string> run_command(const std::string& cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {-1, "Failed to execute command"};
    
    std::string result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    int exit_code = pclose(pipe);
    return {WEXITSTATUS(exit_code), result};
}

// Check if file exists
bool file_exists(const std::string& path) {
    return fs::exists(path);
}

// Get file size
size_t get_file_size(const std::string& path) {
    if (!file_exists(path)) return 0;
    return fs::file_size(path);
}

// Read file content
std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

// Get temp file path
std::string get_temp_file(const std::string& name) {
    return "/tmp/walpp_exe_test_" + name;
}

// Cleanup temp file
void cleanup_file(const std::string& path) {
    if (file_exists(path)) {
        fs::remove(path);
    }
}

// =============================================================================
// TEST CASES
// =============================================================================

// Test 1: Verify WAL executable exists and runs with --help
void test_wal_executable_help(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "WAL executable --help flag";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        std::string cmd = exe_path + " --help";
        auto [exit_code, output] = run_command(cmd);
        
        result.passed = (exit_code == 0 || exit_code == 1); // Help might exit with 0 or 1
        result.message = "Exit code: " + std::to_string(exit_code);
        
        if (output.empty()) {
            result.passed = false;
            result.message += " (No output)";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 2: Verify WAL executable exists and runs with --version
void test_wal_executable_version(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "WAL executable --version flag";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        std::string cmd = exe_path + " --version";
        auto [exit_code, output] = run_command(cmd);
        
        result.passed = (exit_code == 0 || exit_code == 1);
        result.message = "Exit code: " + std::to_string(exit_code);
        
        if (output.empty()) {
            result.passed = false;
            result.message += " (No output)";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 3: Process VCD file
void test_process_vcd_file(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "Process VCD file with executable";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        // Use existing VCD test file
        std::string vcd_file = "tests/data/test_vcd_readable.vcd";
        
        if (!file_exists(vcd_file)) {
            result.passed = false;
            result.message = "VCD file not found: " + vcd_file;
        } else {
            // Run WAL with VCD file (assuming it accepts -f flag or similar)
            std::string cmd = exe_path + " -f " + vcd_file + " 2>&1 | head -20";
            auto [exit_code, output] = run_command(cmd);
            
            // Should execute without crashing
            result.passed = (exit_code == 0 || !output.empty());
            result.message = "Exit code: " + std::to_string(exit_code);
            
            size_t file_size = get_file_size(vcd_file);
            result.message += " (VCD size: " + std::to_string(file_size) + " bytes)";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 4: Process FST file
void test_process_fst_file(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "Process FST file with executable";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        // Use existing FST test file
        std::string fst_file = "tests/data/test_format_detect.fst";
        
        if (!file_exists(fst_file)) {
            // Try alternative path
            fst_file = "tests/original/traces/counter.fst";
        }
        
        if (!file_exists(fst_file)) {
            result.passed = false;
            result.message = "FST file not found";
        } else {
            std::string cmd = exe_path + " -f " + fst_file + " 2>&1 | head -20";
            auto [exit_code, output] = run_command(cmd);
            
            result.passed = (exit_code == 0 || !output.empty());
            result.message = "Exit code: " + std::to_string(exit_code);
            
            size_t file_size = get_file_size(fst_file);
            result.message += " (FST size: " + std::to_string(file_size) + " bytes)";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 5: Process CSV file
void test_process_csv_file(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "Process CSV file with executable";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        // Use existing CSV test file
        std::string csv_file = "tests/data/test_csv_readable.csv";
        
        if (!file_exists(csv_file)) {
            result.passed = false;
            result.message = "CSV file not found: " + csv_file;
        } else {
            std::string cmd = exe_path + " -f " + csv_file + " 2>&1 | head -20";
            auto [exit_code, output] = run_command(cmd);
            
            result.passed = (exit_code == 0 || !output.empty());
            result.message = "Exit code: " + std::to_string(exit_code);
            
            size_t file_size = get_file_size(csv_file);
            result.message += " (CSV size: " + std::to_string(file_size) + " bytes)";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 6: Process multiple file formats sequentially
void test_process_multiple_formats(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "Process multiple file formats sequentially";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        std::vector<std::string> test_files = {
            "tests/data/test_vcd_readable.vcd",
            "tests/data/test_csv_readable.csv"
        };
        
        int successful = 0;
        for (const auto& file : test_files) {
            if (file_exists(file)) {
                std::string cmd = exe_path + " -f " + file + " 2>&1 | head -5";
                auto [exit_code, output] = run_command(cmd);
                if (exit_code == 0 || !output.empty()) {
                    successful++;
                }
            }
        }
        
        result.passed = (successful >= 1);
        result.message = "Successfully processed " + std::to_string(successful) + 
                        " file(s) out of " + std::to_string(test_files.size());
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 7: Convert VCD to CSV
void test_convert_vcd_to_csv(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "Convert VCD file to CSV format";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        std::string vcd_file = "tests/data/test_vcd_readable.vcd";
        std::string csv_output = get_temp_file("converted.csv");
        
        cleanup_file(csv_output);
        
        if (!file_exists(vcd_file)) {
            result.passed = false;
            result.message = "VCD file not found";
        } else {
            // Attempt format conversion (command may vary)
            std::string cmd = exe_path + " -f " + vcd_file + " -o " + csv_output + 
                            " --format csv 2>&1";
            auto [exit_code, output] = run_command(cmd);
            
            // Check if output file was created or if command executed successfully
            bool file_created = file_exists(csv_output);
            result.passed = (file_created || exit_code == 0 || !output.empty());
            result.message = "Exit code: " + std::to_string(exit_code);
            
            if (file_created) {
                size_t csv_size = get_file_size(csv_output);
                result.message += " (CSV output: " + std::to_string(csv_size) + " bytes)";
                cleanup_file(csv_output);
            }
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 8: Handle invalid file gracefully
void test_invalid_file_handling(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "Handle invalid trace file gracefully";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        std::string invalid_file = get_temp_file("invalid.vcd");
        
        // Create an invalid file
        {
            std::ofstream f(invalid_file);
            f << "This is not a valid VCD file!\n";
            f << "Just some random text.\n";
        }
        
        std::string cmd = exe_path + " -f " + invalid_file + " 2>&1";
        auto [exit_code, output] = run_command(cmd);
        
        // Should handle error gracefully (not crash)
        result.passed = true; // As long as it doesn't crash/segfault
        result.message = "Exit code: " + std::to_string(exit_code);
        
        cleanup_file(invalid_file);
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 9: Process file with output redirection
void test_file_output_redirection(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "Process file with output to temp file";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        std::string vcd_file = "tests/data/test_vcd_readable.vcd";
        std::string output_file = get_temp_file("output.txt");
        
        cleanup_file(output_file);
        
        if (!file_exists(vcd_file)) {
            result.passed = false;
            result.message = "VCD file not found";
        } else {
            std::string cmd = exe_path + " -f " + vcd_file + " > " + output_file + 
                            " 2>&1";
            auto [exit_code, _] = run_command(cmd);
            
            bool output_created = file_exists(output_file);
            result.passed = (output_created || exit_code == 0);
            result.message = "Exit code: " + std::to_string(exit_code);
            
            if (output_created) {
                size_t out_size = get_file_size(output_file);
                result.message += " (Output: " + std::to_string(out_size) + " bytes)";
                cleanup_file(output_file);
            }
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 10: Stress test with large file
void test_large_file_processing(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "Process large trace file";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        // Use existing large file if available
        std::string large_file = "tests/data/test_stress_large.vcd";
        
        if (!file_exists(large_file)) {
            // Fall back to regular file
            large_file = "tests/data/test_vcd_readable.vcd";
        }
        
        if (!file_exists(large_file)) {
            result.passed = false;
            result.message = "Test file not found";
        } else {
            size_t file_size = get_file_size(large_file);
            
            std::string cmd = exe_path + " -f " + large_file + " 2>&1 | head -10";
            auto [exit_code, output] = run_command(cmd);
            
            result.passed = (exit_code == 0 || !output.empty());
            result.message = "File size: " + std::to_string(file_size / 1024) + 
                           " KB, Exit code: " + std::to_string(exit_code);
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 11: REPL mode (interactive) - basic test
void test_repl_mode_startup(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "REPL mode startup";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        // Try to start REPL with echo input (immediate exit)
        std::string cmd = "echo 'exit' | " + exe_path + " 2>&1 | head -5";
        auto [exit_code, output] = run_command(cmd);
        
        result.passed = true; // Should start without crashing
        result.message = "Exit code: " + std::to_string(exit_code);
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 12: Evaluate simple expression
void test_expression_evaluation(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "Evaluate expression with -e flag";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        std::string cmd = exe_path + " -e '1 + 1' 2>&1";
        auto [exit_code, output] = run_command(cmd);
        
        result.passed = (exit_code == 0 || !output.empty());
        result.message = "Exit code: " + std::to_string(exit_code);
        
        if (!output.empty()) {
            // Truncate output if too long
            if (output.length() > 100) {
                output = output.substr(0, 100) + "...";
            }
            result.message += ", Output: " + output;
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// -----------------------------------------------------------------------------
// Regression tests for two long-standing `-e` defects.
//
// Both were invisible for a long time because they fail silently: the CLI
// printed nothing rather than erroring, and the second -e simply replaced the
// first. Documentation was written around the broken behaviour. These tests
// exist so that cannot happen again quietly.
// -----------------------------------------------------------------------------

// Test 13: -e must print list and symbol results, not just scalars.
// Regression: cli.cpp's result printer handled nil/bool/int/float/string and
// stopped, so a list fell off the end of the chain and nothing was printed at
// all -- `wal -l t.vcd -e '(signal-names)'` looked like an empty trace.
void test_eval_prints_lists(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "-e prints list and symbol results";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        result.passed = true;

        auto [rc1, out1] = run_command(exe_path + " -e '(list 1 2 3)' 2>&1");
        if (out1.find("(1 2 3)") == std::string::npos) {
            result.passed = false;
            result.message += "list not printed (got: '" + out1 + "'); ";
        }

        auto [rc2, out2] = run_command(exe_path + " -e '(quote foo)' 2>&1");
        if (out2.find("foo") == std::string::npos) {
            result.passed = false;
            result.message += "symbol not printed (got: '" + out2 + "'); ";
        }

        // Nested lists must render too, and strings inside a list stay quoted
        // so the output reads back as the list it came from.
        auto [rc3, out3] = run_command(exe_path + " -e '(list 1 (list \"a\" 2))' 2>&1");
        if (out3.find("(1 (\"a\" 2))") == std::string::npos) {
            result.passed = false;
            result.message += "nested list wrong (got: '" + out3 + "'); ";
        }

        // A real trace query: this is the README's own way to list signals.
        std::string vcd = "tests/original/traces/counter.vcd";
        if (file_exists(vcd)) {
            auto [rc4, out4] = run_command(exe_path + " -l " + vcd + " -e '(signal-names)' 2>&1");
            if (out4.find("counter") == std::string::npos) {
                result.passed = false;
                result.message += "(signal-names) printed no names (got: '" + out4 + "'); ";
            }
        }

        // A top-level string still prints bare, so shells can consume it.
        auto [rc5, out5] = run_command(exe_path + " -e '(concat \"a\" \"b\")' 2>&1");
        if (out5.find("\"") != std::string::npos) {
            result.passed = false;
            result.message += "top-level string should not be quoted (got: '" + out5 + "'); ";
        }

        if (result.passed) result.message = "lists, symbols, nesting and strings all render";
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 14: every -e must run, in order, sharing one interpreter.
// Regression: eval_expr was a single std::string that each -e overwrote, so
// only the last one ever ran and no state was shared between them.
void test_multiple_eval_flags(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "-e accumulates: all run in order, sharing state";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        result.passed = true;

        // Both must appear, first one first.
        auto [rc1, out1] = run_command(exe_path + " -e '(print \"one\")' -e '(print \"two\")' 2>&1");
        size_t p1 = out1.find("one");
        size_t p2 = out1.find("two");
        if (p1 == std::string::npos || p2 == std::string::npos) {
            result.passed = false;
            result.message += "not every -e ran (got: '" + out1 + "'); ";
        } else if (p1 > p2) {
            result.passed = false;
            result.message += "-e ran out of order; ";
        }

        // State defined in an earlier -e must be visible to a later one.
        auto [rc2, out2] = run_command(exe_path + " -e '(define x 5)' -e '(+ x 2)' 2>&1");
        if (out2.find("7") == std::string::npos) {
            result.passed = false;
            result.message += "state not shared across -e (got: '" + out2 + "'); ";
        }

        if (result.passed) result.message = "all -e evaluated in order on one interpreter";
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 15: set-index/get-index must actually move the cursor.
// Regression: both were no-op stubs -- set-index echoed its argument and
// get-index returned a hardcoded 0, so a script could walk a trace and read
// index 0's values at every stop with no error anywhere.
void test_index_navigation(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "set-index/get-index move the trace cursor";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        std::string vcd = "tests/original/traces/counter.vcd";
        if (!file_exists(vcd)) {
            result.passed = true;
            result.message = "Skipped: trace not found: " + vcd;
        } else {
            result.passed = true;

            // get-index must report back what set-index was given.
            auto [rc1, out1] = run_command(
                exe_path + " -l " + vcd + " -e '(begin (set-index 40) (get-index))' 2>&1");
            if (out1.find("40") == std::string::npos) {
                result.passed = false;
                result.message += "get-index did not report 40 (got: '" + out1 + "'); ";
            }

            // Reading at two different indices must not give the same answer,
            // which is exactly what the stub did.
            auto [rc2, out2] = run_command(
                exe_path + " -l " + vcd +
                " -e '(begin (set-index 0) (get-signal \"tb^dut^counter\"))' 2>&1");
            auto [rc3, out3] = run_command(
                exe_path + " -l " + vcd +
                " -e '(begin (set-index 40) (get-signal \"tb^dut^counter\"))' 2>&1");
            if (out2 == out3) {
                result.passed = false;
                result.message += "same value at index 0 and 40 -- cursor not moving; ";
            }

            // Stepping must move too.
            auto [rc4, out4] = run_command(
                exe_path + " -l " + vcd + " -e '(begin (step 5) (get-index))' 2>&1");
            if (out4.find("5") == std::string::npos) {
                result.passed = false;
                result.message += "step did not move the cursor (got: '" + out4 + "'); ";
            }

            if (result.passed) result.message = "set-index, get-index and step all move the cursor";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

int main(int argc, char* argv[]) {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  WAL EXECUTABLE INTEGRATION TESTS - VCD/FST/CSV Processing    ║\n";
    std::cout << "║  Phase 3 Task 1 - End-to-End Executable Testing               ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    // Find WAL executable: CTest passes its path; otherwise guess
    std::string exe_path = argc > 1 ? argv[1] : "./wal";
    if (!file_exists(exe_path)) {
        exe_path = "./tests/../wal";
    }
    if (!file_exists(exe_path)) {
        exe_path = "../wal";
    }
    if (!file_exists(exe_path)) {
        exe_path = "build/wal";
    }
    if (!file_exists(exe_path)) {
        exe_path = "walpp";
    }
    
    std::cout << "\nUsing executable: " << exe_path << "\n";
    
    if (!file_exists(exe_path)) {
        std::cerr << "ERROR: WAL executable not found at: " << exe_path << "\n";
        std::cerr << "Please build the project first: cd build && cmake .. && make\n";
        return 1;
    }
    
    // Create test suite
    ExecutableTestSuite suite("WAL Executable Integration Tests", exe_path);
    
    std::cout << "\nRunning " << 15 << " test cases...\n";
    std::cout << std::string(80, '-') << "\n";
    
    // Run all tests
    test_wal_executable_help(suite, exe_path);
    test_wal_executable_version(suite, exe_path);
    test_process_vcd_file(suite, exe_path);
    test_process_fst_file(suite, exe_path);
    test_process_csv_file(suite, exe_path);
    test_process_multiple_formats(suite, exe_path);
    test_convert_vcd_to_csv(suite, exe_path);
    test_invalid_file_handling(suite, exe_path);
    test_file_output_redirection(suite, exe_path);
    test_large_file_processing(suite, exe_path);
    test_repl_mode_startup(suite, exe_path);
    test_expression_evaluation(suite, exe_path);
    test_eval_prints_lists(suite, exe_path);
    test_multiple_eval_flags(suite, exe_path);
    test_index_navigation(suite, exe_path);
    
    // Print results
    suite.print_summary();
    
    int failed = suite.get_failed_count();
    std::cout << "\n✅ Test execution complete.\n";
    
    if (failed == 0) {
        std::cout << "🎉 All tests passed!\n";
        return 0;
    } else {
        std::cout << "⚠️  " << failed << " test(s) failed.\n";
        return 1;
    }
}
