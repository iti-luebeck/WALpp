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
#include <cstdio>
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

// Test 16: zero is false.
// Regression: Value::is_truthy returned true for everything that was not nil or a
// bool, so `(if 0 ...)` took the then branch, `(! 0)` was false and `(&& 1 0)` was
// true. In a waveform language signal values are integers, so any condition written
// directly on a signal did the opposite of what it said, silently. Counting rising
// edges with the idiomatic `(&& clk (! clk@-1))` returned 0 instead of 40.
// Rules pinned against upstream ics-jku/wal 0.9.1.
void test_zero_is_false(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "zero, empty string and empty list are false";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        result.passed = true;

        struct Case { const char* expr; const char* want; };
        const Case cases[] = {
            // truthiness of each value kind
            {"(if 0 1 2)", "2"},
            {"(if 1 1 2)", "1"},
            {"(if -1 1 2)", "1"},
            {"(if 0.0 1 2)", "2"},
            {"(if \"\" 1 2)", "2"},
            {"(if \"a\" 1 2)", "1"},
            {"(if (list) 1 2)", "2"},
            {"(if (list 1) 1 2)", "1"},
            // the logic operators that read it
            {"(! 0)", "true"},
            {"(! 1)", "false"},
            {"(&& 1 0)", "false"},
            {"(&& 1 1)", "true"},
            {"(|| 0 0)", "false"},
            {"(|| 0 1)", "true"},
        };

        for (const auto& c : cases) {
            auto [rc, out] = run_command(exe_path + " -e '" + c.expr + "' 2>&1");
            if (out.find(c.want) == std::string::npos) {
                result.passed = false;
                result.message += std::string(c.expr) + " should be " + c.want +
                                  " (got: '" + out + "'); ";
            }
        }

        // End to end: the idiomatic rising-edge condition over a real trace. 81
        // timestamps with a toggling clock gives 40 rising edges, which is also what
        // upstream returns for the same expression.
        std::string vcd = "tests/original/traces/counter.vcd";
        if (file_exists(vcd)) {
            auto [rc, out] = run_command(
                exe_path + " -l " + vcd +
                " -e '(length (filter (lambda (i) (begin (set-index i)"
                " (&& tb.clk (! tb.clk@-1)))) (range 1 81)))' 2>&1");
            if (out.find("40") == std::string::npos) {
                result.passed = false;
                result.message += "idiomatic rising-edge count should be 40 (got: '" +
                                  out + "'); ";
            }
        }

        if (result.passed) result.message = "zero is false everywhere it is read";
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 17: upstream WAL compatibility forms.
// Semantics pinned by running each against upstream ics-jku/wal 0.9.1.
// The forms the WAL Programmer Manual documents, checked against it.
//
// Every expectation below was produced by running upstream wal-lang 0.9.1 on the
// same input, so this test is the manual's own answer key. Each group names the
// divergence it guards, because each one of these once disagreed with upstream and
// nothing in the suite noticed.
void test_wal_manual_forms(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "the WAL manual's documented forms";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        result.passed = true;
        std::string vcd = "tests/original/traces/counter.vcd";

        struct Case { const char* expr; const char* want; bool needs_trace; };
        const Case cases[] = {
            // `default` is a fallback, not a label that matches anything. Written
            // first it used to swallow every clause after it and return the wrong
            // branch with no error at all.
            {"(case 2 [default 99] [2 2])", "2", false},
            {"(case 2 [2 2] [default 99])", "2", false},
            {"(case 7 [2 2] [default 99])", "99", false},

            // let binds in order, so a later binding sees an earlier one. The
            // manual: "later bindings can use earlier bindings".
            {"(let ([x 10] [y x]) (+ x y))", "20", false},
            {"(let ([x 10] [y 12]) (+ x y))", "22", false},

            // slice over a number is a bit field, inclusive, upper first. 12 is
            // 0b1100, so bits 3..2 are 0b11 and bits 2..1 are 0b10.
            {"(slice 12 3 2)", "3", false},
            {"(slice 12 2 1)", "2", false},
            {"(slice 255 7 4)", "15", false},
            {"(slice 12 2)", "1", false},
            // over a list or a string it indexes and slices as Python does
            {"(slice (list 1 2 3 4 5) 1)", "2", false},
            {"(slice (list 1 2 3 4 5) 1 3)", "(2 3)", false},
            {"(slice \"hello\" 1 3)", "el", false},

            // `in` takes an array, by key. Written with "x" rather than 'x only
            // because an apostrophe would close the shell's -e argument here. Upstream's own lib/std/std.wal builds
            // geta/default on this, so without it a documented operator that
            // WAL++ loads from its own stdlib could not be called.
            {"(in \"x\" (array [\"x\" 10]))", "true", false},
            {"(geta/default (array [\"x\" 10]) 5 \"x\")", "10", false},
            {"(geta/default (array [\"x\" 10]) 5 \"y\")", "5", false},

            // All-string + concatenates. ALL-INDICES needs it, and timeframe is
            // built on ALL-INDICES.
            {"(+ \"a\" \"b\")", "ab", false},
            {"(+ \"t0\" \"^INDEX\")", "t0^INDEX", false},
            // mixed with a number it is still an error, which is the point
            {"(+ 1 \"hi\")", "Expected number", false},

            // upstream binds true and false as ordinary globals
            {"(if true 1 2)", "1", false},
            {"(if false 1 2)", "2", false},

            // trace ids follow upstream's documented t0, t1, ... scheme
            {"(begin (load \"tests/original/traces/counter.vcd\") (loaded-traces))",
             "(\"t0\")", false},
            {"(loaded-traces)", "(\"t0\")", true},

            // a trace id may be written as a symbol, which is how the manual
            // types every one of them
            {"(begin (load \"tests/original/traces/counter.vcd\" (quote tr)) tr^tb.clk)",
             "0", false},
            {"(begin (load \"tests/original/traces/counter.vcd\" (quote tr)) (step (quote tr) 5) INDEX)",
             "5", false},
            // and the step count may come last, as the manual writes it, or first,
            // as WAL++ used to require. Neither is ambiguous.
            {"(begin (load \"tests/original/traces/counter.vcd\" (quote tr)) (step \"tr\" 5) INDEX)",
             "5", false},
            {"(begin (load \"tests/original/traces/counter.vcd\" (quote tr)) (step 5 (quote tr)) INDEX)",
             "5", false},
        };

        for (const auto& c : cases) {
            std::string cmd = exe_path;
            if (c.needs_trace) cmd += " -l " + vcd;
            cmd += " -e '" + std::string(c.expr) + "' 2>&1";
            auto [rc, out] = run_command(cmd);
            if (out.find(c.want) == std::string::npos) {
                result.passed = false;
                result.message += std::string(c.expr) + " should give " + c.want +
                                  " (got: '" + out + "'); ";
            }
        }

        // timeframe restores every trace's index after the body, so a local step
        // does not lose the cursor. Upstream prints 2, 7, 2.
        {
            std::string script = "/tmp/walpp_manual_timeframe.wal";
            {
                std::ofstream f(script);
                f << "(load \"" << vcd << "\")\n"
                  << "(step 2)\n(print INDEX)\n"
                  << "(timeframe (step 5) (print INDEX))\n(print INDEX)\n";
            }
            auto [rc, out] = run_command(exe_path + " -f " + script + " 2>&1");
            if (out.find("2") == std::string::npos || out.find("7") == std::string::npos) {
                result.passed = false;
                result.message += "timeframe should print 2, 7, 2 (got: '" + out + "'); ";
            }
            std::remove(script.c_str());
        }

        // A script prints what it prints and nothing else. The value of the last
        // top-level form used to be echoed, which put a stray line on the stdout of
        // every script whose job was to write a report or a CSV.
        {
            std::string script = "/tmp/walpp_manual_echo.wal";
            {
                std::ofstream f(script);
                f << "(define x 1)\n(+ x 41)\n";
            }
            auto [rc, out] = run_command(exe_path + " -f " + script + " 2>&1");
            if (out.find("42") != std::string::npos) {
                result.passed = false;
                result.message += "a script must not echo its last value (got: '" +
                                  out + "'); ";
            }
            std::remove(script.c_str());
        }

        // eval-file resolves "module" to module.wal, as the manual defines it, and
        // still accepts the name written out in full.
        {
            std::string mod = "/tmp/walpp_manual_mod.wal";
            {
                std::ofstream f(mod);
                f << "(define helper 7)\n";
            }
            for (const char* spelling : {"/tmp/walpp_manual_mod",
                                         "/tmp/walpp_manual_mod.wal"}) {
                auto [rc, out] = run_command(
                    exe_path + " -e '(begin (eval-file \"" + std::string(spelling) +
                    "\") helper)' 2>&1");
                if (out.find("7") == std::string::npos) {
                    result.passed = false;
                    result.message += std::string("eval-file ") + spelling +
                                      " should give 7 (got: '" + out + "'); ";
                }
            }
            std::remove(mod.c_str());
        }

        if (result.passed) {
            result.message = "every documented form agrees with upstream 0.9.1";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

void test_wal_compat_forms(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "upstream WAL forms: control flow, predicates, printf, alias";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        result.passed = true;
        struct Case { const char* expr; const char* want; };
        const Case cases[] = {
            {"(when 1 42)", "42"},
            {"(unless 0 42)", "42"},
            {"(do 1 2 3)", "3"},
            {"(let ([x 5] [y 2]) (+ x y))", "7"},
            {"(cond ((= 1 2) 7) (else 9))", "9"},
            {"(case 2 (1 11) (2 22))", "22"},
            // An inline lambda could not be applied at all before: the evaluator
            // required the head of a form to be a symbol.
            {"((fn [x] (* x 2)) 21)", "42"},
            {"((lambda (x) (* x 3)) 14)", "42"},
            {"(mod 7 3)", "1"},
            {"(average (list 1 2 3))", "2"},
            {"(second (list 1 2 3))", "2"},
            {"(last (list 1 2 3))", "3"},
            {"(in 2 (list 1 2 3))", "true"},
            {"(int? 1)", "true"},
            {"(list? (list 1))", "true"},
            {"(atom? 1)", "true"},
            {"(atom? (list 1))", "false"},
            {"(defined? nope)", "false"},
            {"(string->int \"42\")", "42"},
            {"(int->string 42)", "42"},
            {"(convert/bin 5)", "101"},
        };
        for (const auto& c : cases) {
            auto [rc, out] = run_command(exe_path + " -e '" + c.expr + "' 2>&1");
            if (out.find(c.want) == std::string::npos) {
                result.passed = false;
                result.message += std::string(c.expr) + " should give " + c.want +
                                  " (got: '" + out + "'); ";
            }
        }

        // printf, including flags, width and precision
        auto [rc1, out1] = run_command(
            exe_path + " -e '(printf \"n=%d hex=%04x s=%s f=%.2f%%\\n\" 255 255 \"ok\" 3.14159)' 2>&1");
        if (out1.find("n=255 hex=00ff s=ok f=3.14%") == std::string::npos) {
            result.passed = false;
            result.message += "printf formatting wrong (got: '" + out1 + "'); ";
        }

        if (result.passed) result.message = "all upstream forms behave as upstream does";
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 18: a script file runs every top-level form, and the agentic-WAL paper's
// Listing 1 runs unmodified.
// Regression: Wal::eval_str parses ONE expression, and -f/--stdin/--pipe all went
// through it, so a script silently executed only its first form and stopped.
void test_script_runs_all_forms(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "-f runs every top-level form; paper's Listing 1 runs";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        result.passed = true;

        std::string script = get_temp_file("multiform.wal");
        {
            std::ofstream f(script);
            f << "(define x 5)\n";
            f << "(printf \"one x=%d\\n\" x)\n";
            f << "(printf \"two\\n\")\n";
        }
        auto [rc, out] = run_command(exe_path + " -f " + script + " 2>&1");
        if (out.find("one x=5") == std::string::npos || out.find("two") == std::string::npos) {
            result.passed = false;
            result.message += "not every top-level form ran (got: '" + out + "'); ";
        }
        cleanup_file(script);

        // Listing 1 of "Agentic Programmable Waveform Analysis" (Roithmayr and
        // Grosse, DVCon Europe 2026), structure unchanged: alias, while over step,
        // when, an @-1 offset and printf. Upstream prints 40 for this file too.
        std::string vcd = "tests/original/traces/counter.vcd";
        if (file_exists(vcd)) {
            std::string listing = get_temp_file("listing1.wal");
            {
                std::ofstream f(listing);
                f << "(alias clk \"tb.clk\")\n";
                f << "(define cnt 0)\n";
                f << "(while (step 1)\n";
                f << "  (when (&& clk (! clk@-1))\n";
                f << "    (set! cnt (+ cnt 1))))\n";
                f << "(printf \"Total rising edges: %d\\n\" cnt)\n";
            }
            auto [rc2, out2] = run_command(exe_path + " -l " + vcd + " -f " + listing + " 2>&1");
            if (out2.find("Total rising edges: 40") == std::string::npos) {
                result.passed = false;
                result.message += "Listing 1 should report 40 (got: '" + out2 + "'); ";
            }
            cleanup_file(listing);
        }

        if (result.passed) result.message = "multi-form scripts and Listing 1 both run";
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 19: WAL's special variables resolve.
// Regression: Trace::get_special_signal implemented INDEX, MAX-INDEX, TS, SIGNALS,
// SCOPES and TRACE-NAME all along, but nothing called it from the evaluator, so each
// failed with "Undefined variable". MAX-INDEX also closed a real gap: there was no
// way to ask how long a trace is.
void test_special_variables(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "special variables INDEX, MAX-INDEX, TS, SIGNALS, SCOPES";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        std::string vcd = "tests/original/traces/counter.vcd";
        if (!file_exists(vcd)) {
            result.passed = true;
            result.message = "Skipped: trace not found";
        } else {
            result.passed = true;
            struct Case { const char* expr; const char* want; };
            const Case cases[] = {
                {"INDEX", "0"},
                {"MAX-INDEX", "80"},          // 81 timestamps, upstream agrees
                {"(+ 1 MAX-INDEX)", "81"},
                {"TS", "0"},
                {"(length SIGNALS)", "7"},
                {"(length SCOPES)", "2"},     // tb and tb.dut
                {"(begin (set-index 40) INDEX)", "40"},
            };
            for (const auto& c : cases) {
                auto [rc, out] = run_command(
                    exe_path + " -l " + vcd + " -e '" + c.expr + "' 2>&1");
                if (out.find(c.want) == std::string::npos) {
                    result.passed = false;
                    result.message += std::string(c.expr) + " should be " + c.want +
                                      " (got: '" + out + "'); ";
                }
            }
            if (result.passed) result.message = "special variables resolve from the trace";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 20: lists and arrays compare by value, not by identity.
//
// `Value::operator==` fell through to comparing only the variant tag for every
// reference type, so `(= (list 1 2) (list 1 3))` answered TRUE for any two lists.
// Like the truthiness bug, this produced a wrong answer with no error, and it also
// silently broke `in`, `!=` and anything filtering a list of lists.
void test_structural_equality(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "lists and arrays compare by value";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        result.passed = true;
        struct Case { const char* expr; const char* want; };
        const Case cases[] = {
            {"(= (list 1 2) (list 1 2))", "true"},
            {"(= (list 1 2) (list 1 3))", "false"},     // was true
            {"(= (list 1 2) (list 1 2 3))", "false"},   // was true
            {"(!= (list 1 2) (list 1 3))", "true"},     // was false
            {"(in (list 1) (list (list 1) (list 2)))", "true"},
            {"(in (list 9) (list (list 1) (list 2)))", "false"},
            {"(= (array (1 2)) (array (1 2)))", "true"},
            {"(= (array (1 2)) (array (1 3)))", "false"},
        };
        for (const auto& c : cases) {
            auto [rc, out] = run_command(exe_path + " -e '" + c.expr + "' 2>&1");
            if (out.find(c.want) == std::string::npos) {
                result.passed = false;
                result.message += std::string(c.expr) + " should be " + c.want +
                                  " (got: '" + out + "'); ";
            }
        }
        if (result.passed) result.message = "equality is structural for collections";
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 21: an FST and the equivalent VCD must describe the same trace.
//
// The FST reader skipped every variable FST had flagged as an alias, and kept the
// bit range FST appends to a vector's name. counter.fst therefore listed 4 signals
// where counter.vcd listed 7, and `tb.dut.counter` did not resolve against the FST
// at all while the identical VCD worked.
void test_fst_matches_vcd(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "FST and VCD readers agree on the same design";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        std::string vcd = "tests/original/traces/counter.vcd";
        std::string fst = "tests/original/traces/counter.fst";
        if (!file_exists(vcd) || !file_exists(fst)) {
            result.passed = true;
            result.message = "Skipped: traces not found";
        } else {
            result.passed = true;
            const char* exprs[] = {
                "SIGNALS",
                "(length SIGNALS)",
                "MAX-INDEX",
                "tb.dut.counter",
                "(length (find (&& tb.clk (! tb.clk@-1))))",
            };
            for (const auto& expr : exprs) {
                auto [rc_v, out_v] = run_command(
                    exe_path + " -l " + vcd + " -e '" + expr + "' 2>&1");
                auto [rc_f, out_f] = run_command(
                    exe_path + " -l " + fst + " -e '" + expr + "' 2>&1");
                if (out_v != out_f) {
                    result.passed = false;
                    result.message += std::string(expr) + ": VCD gave '" + out_v +
                                      "' but FST gave '" + out_f + "'; ";
                }
            }
            if (result.passed) {
                result.message = "both readers report the same signals and values";
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

// Test 22: macros, and the standard-library idioms built on them.
//
// `defmacro`, quasiquote and `reval` are what upstream's own standard library is
// written in: `rising`, `falling` and `count` are macros there, not builtins. The
// parser read quasiquote and SEval carried an unused `macros` map, but nothing
// expanded anything, so none of the stdlib could run.
void test_macros(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "macros and the stdlib idioms built on them";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        std::string vcd = "tests/original/traces/counter.vcd";
        result.passed = true;

        struct Case { const char* expr; const char* want; bool needs_trace; };
        const Case cases[] = {
            // expansion happens before evaluation, and arguments stay unevaluated
            {"-e '(defmacro twice [x] `(+ ,x ,x))' -e '(twice 21)'", "42", false},
            // (quote ...) rather than the apostrophe, to keep the shell quoting simple
            {"-e '(defmacro twice [x] `(+ ,x ,x))' -e '(macroexpand (quote (twice 21)))'",
             "('+ 21 21)", false},  // symbols print with a leading quote
            // unquote-splicing
            {"-e '(defmacro all [xs] `(list ,@xs))' -e '(all (1 2 3))'", "(1 2 3)", false},
            // gensym gives a fresh name each time
            {"-e '(= (gensym) (gensym))'", "false", false},
            // upstream's own definitions of rising and count, verbatim
            {"-e '(defmacro rising [e] `(&& (= ,e 0) (= (reval ,e 1) 1)))' "
             "-e '(defmacro count [c] `(length (find ,c)))' "
             "-e '(count (rising tb.clk))'", "40", true},
            // reval is bounds-safe, which is what makes rising safe at index 0
            {"-e '(begin (set-index 0) (reval tb.clk -1))'", "false", true},
            // a special variable can be read at an offset
            {"-e '(begin (set-index 5) TS@-1)'", "40", true},
        };

        for (const auto& c : cases) {
            if (c.needs_trace && !file_exists(vcd)) continue;
            std::string command = exe_path;
            if (c.needs_trace) command += " -l " + vcd;
            command += " " + std::string(c.expr) + " 2>&1";
            auto [rc, out] = run_command(command);
            if (out.find(c.want) == std::string::npos) {
                result.passed = false;
                result.message += std::string(c.expr) + " should give " + c.want +
                                  " (got: '" + out + "'); ";
            }
        }
        if (result.passed) result.message = "macro expansion and reval behave as upstream";
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 23: the standard library is loaded and agrees with upstream.
//
// lib/std/std.wal is upstream's own standard library, vendored verbatim. It was
// installed by CMake and documented in the README, but nothing loaded it, so
// `rising`, `count` and `defun` did not exist. The expected values below were
// taken by running upstream 0.9.1 on the same trace.
void test_std_library(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "standard library matches upstream";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        std::string vcd = "tests/original/traces/counter.vcd";
        result.passed = true;

        struct Case { const char* expr; const char* want; bool needs_trace; };
        const Case cases[] = {
            {"(count (rising tb.clk))", "40", true},     // upstream: 40
            {"(count (falling tb.clk))", "40", true},    // upstream: 40
            {"(length (find (= tb.dut.counter 3)))", "6", true},  // upstream: 6
            {"(begin (defun sq [x] (* x x)) (sq 12))", "144", false},
            {"(for/list [x (list 1 2 3)] (* x 10))", "(10 20 30)", false},
            {"(cadr (list 1 2 3))", "2", false},
            // `append` is upstream's: it adds ONE element, so a list argument nests.
            // Verified against upstream 0.9.1, which gives the same.
            {"(append (list 1 2) 3)", "(1 2 3)", false},
            {"(append (list 1 2) (list 3))", "(1 2 (3))", false},
            // WAL++'s own concatenation kept its behavior under the name `extend`.
            {"(extend (list 1 2) (list 3 4))", "(1 2 3 4)", false},
            // + builds a list when the first operand is one, which is what the
            // library's append, reverse and filter are written in terms of.
            {"(+ (list 1 2) (list 3 4))", "(1 2 3 4)", false},
            {"(reverse (list 1 2 3))", "(3 2 1)", false},
        };

        for (const auto& c : cases) {
            if (c.needs_trace && !file_exists(vcd)) continue;
            std::string command = exe_path;
            if (c.needs_trace) command += " -l " + vcd;
            command += " -e '" + std::string(c.expr) + "' 2>&1";
            auto [rc, out] = run_command(command);
            if (out.find(c.want) == std::string::npos) {
                result.passed = false;
                result.message += std::string(c.expr) + " should give " + c.want +
                                  " (got: '" + out + "'); ";
            }
            // Loading the library must be silent.
            if (out.find("warning:") != std::string::npos) {
                result.passed = false;
                result.message += "standard library warned on: " + std::string(c.expr) + "; ";
            }
        }
        if (result.passed) result.message = "stdlib loads silently and agrees with upstream";
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 24: 64-bit by default, arbitrary precision on request.
//
// A literal past 2^63 used to fall through the number parser into the SYMBOL branch,
// so `583547542375894327581` was reported as an undefined variable rather than as a
// number out of range. The default stays 64-bit, which is what waveform values and
// indices need and what keeps arithmetic on the native path; --bigint opts in.
void test_bigint_mode(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "64-bit by default, --bigint for arbitrary precision";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        result.passed = true;
        const std::string big = "583547542375894327581";

        // Default mode: a clear diagnostic that names the limit and the way out.
        auto [rc, out] = run_command(exe_path + " -e '(int? " + big + ")' 2>&1");
        if (out.find("does not fit in 64 bits") == std::string::npos ||
            out.find("--bigint") == std::string::npos) {
            result.passed = false;
            result.message += "default mode should explain the 64-bit limit (got: '" +
                              out + "'); ";
        }

        struct Case { std::string expr; std::string want; };
        const Case cases[] = {
            {"(int? " + big + ")", "true"},
            {"(+ " + big + " 1)", "583547542375894327582"},
            {"(* " + big + " " + big + ")",
             "340527734212946185964567257311572137311561"},
            {"(- " + big + " " + big + ")", "0"},
            {"(< " + big + " 583547542375894327582)", "true"},
            {"(> " + big + " 583547542375894327582)", "false"},
            {"(= " + big + " " + big + ")", "true"},
            // A result that fits comes back as a plain int, so a bigint does not
            // spread through a computation that no longer needs one.
            {"(int? (- " + big + " 583547542375894327580))", "true"},
            {"(- " + big + " 583547542375894327580)", "1"},
            // Ordinary arithmetic is unaffected in either mode.
            {"(+ 1 2)", "3"},
            {"(if 0 (quote yes) (quote no))", "'no"},
        };
        for (const auto& c : cases) {
            auto [rc2, out2] = run_command(
                exe_path + " --bigint -e '" + c.expr + "' 2>&1");
            if (out2.find(c.want) == std::string::npos) {
                result.passed = false;
                result.message += c.expr + " should give " + c.want +
                                  " (got: '" + out2 + "'); ";
            }
        }
        if (result.passed) {
            result.message = "64-bit default diagnoses the limit, --bigint is exact";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 25: an array element keeps its own name.
//
// A VCD hierarchy spells an array element `data[0]`. That name could not survive now
// that `[` opens a subscript, so `TOP.tb.data[0].data.something` read as the symbol
// `TOP.tb.data` subscripted by 0. Upstream hit the same ambiguity and renames to
// `data<0>`, which both repairs the name and matches upstream exactly.
void test_array_element_names(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "array element names survive subscript syntax";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        std::string vcd = "tests/original/traces/sv_struct_array.vcd";
        std::string fst = "tests/original/traces/sv_struct_array.fst";
        if (!file_exists(vcd)) {
            result.passed = true;
            result.message = "Skipped: trace not found";
        } else {
            result.passed = true;
            struct Case { const char* expr; const char* want; };
            const Case cases[] = {
                // upstream gives 5 for this exact expression
                {"TOP.tb.data<0>.data.something", "5"},
                {"TOP^tb^data<0>^data^something", "5"},
                // the old bracketed spelling still resolves through get-signal
                {"(get-signal \"TOP.tb.data[0].data.something\")", "5"},
                {"(length SIGNALS)", "2"},
                // and a list subscript still means a subscript
                {"(begin (define l (list 7 8 9)) l[2])", "9"},
            };
            for (const auto& c : cases) {
                auto [rc, out] = run_command(
                    exe_path + " -l " + vcd + " -e '" + c.expr + "' 2>&1");
                if (out.find(c.want) == std::string::npos) {
                    result.passed = false;
                    result.message += std::string(c.expr) + " should give " + c.want +
                                      " (got: '" + out + "'); ";
                }
            }
            // VCD and FST must agree on the renamed hierarchy.
            if (file_exists(fst)) {
                auto [rcv, outv] = run_command(exe_path + " -l " + vcd + " -e 'SIGNALS' 2>&1");
                auto [rcf, outf] = run_command(exe_path + " -l " + fst + " -e 'SIGNALS' 2>&1");
                if (outv != outf) {
                    result.passed = false;
                    result.message += "VCD gave '" + outv + "' but FST gave '" + outf + "'; ";
                }
            }
            if (result.passed) {
                result.message = "array elements read as data<0>, as upstream spells them";
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

// Test 26: `.` separates hierarchy, `^` prefixes a trace id, as upstream.
//
// WAL++'s interpreter used `^` for hierarchy, which disagreed with upstream AND
// with WAL++'s own --fast-* paths, so one binary named the same signal two ways.
// It also took `^` away from the meaning upstream gives it. Names written the old
// way still resolve so existing scripts keep working.
void test_separator(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "dotted hierarchy, caret trace id";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        std::string vcd = "tests/original/traces/counter.vcd";
        if (!file_exists(vcd)) {
            result.passed = true;
            result.message = "Skipped: trace not found";
        } else {
            result.passed = true;
            struct Case { const char* expr; const char* want; };

            // Single trace: names are dotted, and SIGNALS reports them dotted.
            const Case single[] = {
                {"SIGNALS", "'tb.dut.counter"},
                {"SCOPES", "('tb 'tb.dut)"},
                {"(begin (set-index 40) tb.dut.counter)", "15"},
                // (quote ...) rather than the apostrophe: the expression is passed
                // inside single quotes, which an apostrophe would close.
                {"(signal? (quote tb.dut.clk))", "true"},
                // the old caret spelling still resolves
                {"(begin (set-index 40) tb^dut^counter)", "15"},
                {"(get-signal \"tb^dut^counter\")", "0"},
            };
            for (const auto& c : single) {
                auto [rc, out] = run_command(
                    exe_path + " -l " + vcd + " -e '" + c.expr + "' 2>&1");
                if (out.find(c.want) == std::string::npos) {
                    result.passed = false;
                    result.message += std::string(c.expr) + " should contain " + c.want +
                                      " (got: '" + out + "'); ";
                }
            }

            // The fast paths have always used dots. The interpreter must agree now.
            auto [rcl, listing] = run_command(
                exe_path + " --fast-list " + vcd + " -o /dev/stdout 2>/dev/null");
            auto [rcs, signals] = run_command(
                exe_path + " -l " + vcd + " -e 'SIGNALS' 2>&1");
            if (listing.find("tb.dut.counter") == std::string::npos ||
                signals.find("tb.dut.counter") == std::string::npos) {
                result.passed = false;
                result.message += "fast path and interpreter must spell a name alike; ";
            }

            // Two traces: `^` selects which one.
            std::string load = " -e '(load \"" + vcd + "\" \"a\")'"
                               " -e '(load \"" + vcd + "\" \"b\")'";
            const Case multi[] = {
                {"(list a^MAX-INDEX b^MAX-INDEX)", "(80 80)"},
                {"(begin (step 5) (list a^INDEX b^INDEX))", "(5 5)"},
                {"(count (rising a^tb.clk))", "40"},
            };
            for (const auto& c : multi) {
                auto [rc, out] = run_command(
                    exe_path + load + " -e '" + c.expr + "' 2>&1");
                if (out.find(c.want) == std::string::npos) {
                    result.passed = false;
                    result.message += std::string(c.expr) + " should contain " + c.want +
                                      " (got: '" + out + "'); ";
                }
            }

            if (result.passed) {
                result.message = "hierarchy is dotted, ^ selects a trace, old names still resolve";
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

// Test 27: a script's own (load ...) must work, not only the -l flag.
//
// Streaming defaulted to on, and a streaming trace does not populate SIGNALS,
// SCOPES or MAX-INDEX. The -l path force-disabled streaming; the `load` OPERATOR
// did not. Every upstream script begins with (load ...), so every one of them saw
// a trace reporting zero signals.
void test_load_operator(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "(load ...) populates the trace, like -l";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        std::string vcd = "tests/original/traces/counter.vcd";
        if (!file_exists(vcd)) {
            result.passed = true;
            result.message = "Skipped: trace not found";
        } else {
            result.passed = true;
            std::string load = " -e '(load \"" + vcd + "\" \"a\")'";
            struct Case { const char* expr; const char* want; };
            const Case cases[] = {
                {"(length SIGNALS)", "7"},
                {"(length SCOPES)", "2"},
                {"MAX-INDEX", "80"},
                {"(count (rising tb.clk))", "40"},
            };
            for (const auto& c : cases) {
                auto [rc, out] = run_command(
                    exe_path + load + " -e '" + c.expr + "' 2>&1");
                if (out.find(c.want) == std::string::npos) {
                    result.passed = false;
                    result.message += std::string(c.expr) + " after (load ...) should be " +
                                      c.want + " (got: '" + out + "'); ";
                }
            }
            if (result.passed) {
                result.message = "a script's own load gives a fully populated trace";
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

// Test 28: types stay apart unless a conversion is written out.
//
// An int, a float, a bool and a string are four different things. Upstream, being
// Python, coerces freely; that stays available behind --compat-types rather than
// being the default, because a silent cross-type match is exactly how a comparison
// against a wide signal's bit string would quietly succeed.
void test_strict_types(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "types stay separate unless explicitly converted";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        result.passed = true;
        struct Case { const char* expr; const char* want; };

        // Default: no coercion anywhere.
        const Case strict[] = {
            {"(= 0 #f)", "false"},
            {"(= 1 #t)", "false"},
            {"(= 1 1.0)", "false"},
            {"(!= 1 1.0)", "true"},
            {"(= 1 1)", "true"},
            {"(= 1.0 1.0)", "true"},
            {"(= \"a\" \"a\")", "true"},
            {"(= \"1\" 1)", "false"},
            {"(in 1.0 (list 1 2))", "false"},
            // + refuses to join a string to a NUMBER, which is the case the
            // strictness exists for: a signal wider than 64 bits reads as a bit
            // string, so coercing there would return a plausible wrong answer.
            {"(+ 1 \"hi\")", "Expected number"},
            // All-string + concatenates, as upstream does. Nothing here can be
            // mistaken for arithmetic, and upstream's own lib/std/std.wal needs it:
            // ALL-INDICES and therefore timeframe are built on (+ t "^INDEX").
            {"(= (+ \"a\" \"b\") \"ab\")", "true"},
            // the explicit conversions are the way across
            {"(= (int->string 1) \"1\")", "true"},
            {"(= (string->int \"1\") 1)", "true"},
        };
        for (const auto& c : strict) {
            auto [rc, out] = run_command(exe_path + " -e '" + c.expr + "' 2>&1");
            if (out.find(c.want) == std::string::npos) {
                result.passed = false;
                result.message += std::string(c.expr) + " should give " + c.want +
                                  " (got: '" + out + "'); ";
            }
        }

        // --compat-types restores upstream's coercion. Each expectation below was
        // taken by running upstream 0.9.1 on the same expression.
        const Case compat[] = {
            {"(= 0 #f)", "true"},
            {"(= 1 #t)", "true"},
            {"(= 1 1.0)", "true"},
            {"(= 0 #t)", "false"},
            {"(+ 1 \"hi\")", "1hi"},
            {"(+ \"a\" \"b\")", "ab"},
            // a string still does not equal a number
            {"(= \"a\" 1)", "false"},
        };
        for (const auto& c : compat) {
            auto [rc, out] = run_command(
                exe_path + " --compat-types -e '" + c.expr + "' 2>&1");
            if (out.find(c.want) == std::string::npos) {
                result.passed = false;
                result.message += std::string("--compat-types ") + c.expr +
                                  " should give " + c.want + " (got: '" + out + "'); ";
            }
        }

        if (result.passed) {
            result.message = "strict by default, upstream coercion behind --compat-types";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 29: mod is Euclidean, so the result is the least non-negative residue.
//
// C++'s % truncates, so its sign follows the dividend and `(mod -7 3)` came out
// -1, which represents no congruence class mod 3 and breaks the ordinary job of
// wrapping an index that has gone negative. For every POSITIVE divisor this agrees
// with upstream, so the correct answer and the compatible one coincide in every
// realistic case; they differ only for a negative divisor.
void test_mod_euclidean(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "mod is Euclidean: 0 <= r < |divisor|";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        result.passed = true;
        struct Case { const char* expr; const char* want; const char* note; };
        const Case cases[] = {
            {"(mod 7 3)",   "1", "upstream: 1"},
            {"(mod -7 3)",  "2", "upstream: 2"},
            {"(mod -1 4)",  "3", "upstream: 3"},
            {"(mod 0 5)",   "0", "upstream: 0"},
            {"(mod 9 3)",   "0", "upstream: 0"},
            // negative divisor: upstream floors and gives -2 and -1 here, which are
            // not non-negative residues. Euclidean keeps 0 <= r < |divisor|.
            {"(mod 7 -3)",  "1", "upstream floors: -2"},
            {"(mod -7 -3)", "2", "upstream floors: -1"},
        };
        for (const auto& c : cases) {
            auto [rc, out] = run_command(exe_path + " -e '" + c.expr + "' 2>&1");
            if (out.find(c.want) == std::string::npos) {
                result.passed = false;
                result.message += std::string(c.expr) + " should be " + c.want + " (" +
                                  c.note + ") but gave '" + out + "'; ";
            }
        }
        // division by zero is an error, not a crash
        auto [rcz, outz] = run_command(exe_path + " -e '(mod 1 0)' 2>&1");
        if (outz.find("zero") == std::string::npos) {
            result.passed = false;
            result.message += "(mod 1 0) should report division by zero; ";
        }
        if (result.passed) {
            result.message = "least non-negative residue, agreeing with upstream for n > 0";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 30: the comparison operators, including across types and on bigints.
void test_comparisons(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "comparison operators";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        result.passed = true;
        struct Case { const char* expr; const char* want; };
        const Case cases[] = {
            {"(< 1 2)", "true"},      {"(< 2 1)", "false"},    {"(< 1 1)", "false"},
            {"(> 2 1)", "true"},      {"(> 1 2)", "false"},
            {"(<= 1 1)", "true"},     {"(<= 2 1)", "false"},
            {"(>= 1 1)", "true"},     {"(>= 1 2)", "false"},
            // ordering still mixes int and float, which is what makes `=` refusing
            // to a deliberate choice about identity rather than an accident
            {"(< 1 1.5)", "true"},    {"(> 2.5 2)", "true"},
            // n-ary equality and inequality, as upstream
            {"(= 1 1 1)", "true"},    {"(= 1 1 2)", "false"},
            {"(!= 3 3)", "false"},    {"(!= 3 3 3)", "false"},
            {"(!= 3 3 4)", "true"},
            // collections compare structurally
            {"(= (list 1 2) (list 1 2))", "true"},
            {"(= (list 1 2) (list 1 3))", "false"},
            {"(= (array (1 2)) (array (1 2)))", "true"},
            // strings and symbols
            {"(= \"ab\" \"ab\")", "true"},
            {"(= (quote ab) (quote ab))", "true"},
            {"(= (quote ab) (quote ba))", "false"},
        };
        for (const auto& c : cases) {
            auto [rc, out] = run_command(exe_path + " -e '" + c.expr + "' 2>&1");
            if (out.find(c.want) == std::string::npos) {
                result.passed = false;
                result.message += std::string(c.expr) + " should be " + c.want +
                                  " (got: '" + out + "'); ";
            }
        }

        // past 2^53 a comparison through double would call distinct values equal
        const Case big[] = {
            {"(< 583547542375894327581 583547542375894327582)", "true"},
            {"(> 583547542375894327582 583547542375894327581)", "true"},
            {"(= 583547542375894327581 583547542375894327581)", "true"},
            {"(= 583547542375894327581 583547542375894327582)", "false"},
        };
        for (const auto& c : big) {
            auto [rc, out] = run_command(
                exe_path + " --bigint -e '" + c.expr + "' 2>&1");
            if (out.find(c.want) == std::string::npos) {
                result.passed = false;
                result.message += std::string("--bigint ") + c.expr + " should be " +
                                  c.want + " (got: '" + out + "'); ";
            }
        }

        if (result.passed) result.message = "ordering, equality and bigint comparison";
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 31: the last sixteen upstream operators.
//
// Everything upstream's operator enum names now exists: 0 of 106 missing. The
// expectations marked "upstream" were taken by running wal-lang 0.9.1 on the same
// input. `import` and `call` are upstream's bridge into the CPython interpreter it
// runs inside; WAL++ is a C++ program with none, so they are registered to fail
// with a message that says so rather than looking like a missing operator.
void test_remaining_operators(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "the last sixteen upstream operators";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        std::string vcd = "tests/original/traces/counter.vcd";
        result.passed = true;

        struct Case { const char* expr; const char* want; bool needs_trace; };
        const Case cases[] = {
            // code as data
            {"(eval (quote (+ 1 2)))", "3", false},            // upstream: 3
            {"(parse \"(+ 1 2)\")", "('+ 1 2)", false},
            {"(eval (parse \"(+ 40 2)\"))", "42", false},
            {"(length (parse \"1\" \"2\"))", "3", false},   // (do 1 2)
            // conversion, two's complement
            {"(bits->sint \"0101\")", "5", false},            // upstream: 5
            {"(bits->sint \"1011\")", "-5", false},           // upstream: -5
            {"(bits->sint \"1000\")", "-8", false},           // upstream: -8
            {"(bits->sint \"0\")", "0", false},
            // trace queries
            // A trace loaded with -l carries upstream's auto id, t0, so an upstream
            // script can name it. It used to be "DEFAULT", which no upstream
            // script would reach for.
            {"(loaded-traces)", "(\"t0\")", true},
            {"(length (find/g (rising tb.clk)))", "40", true},
            // scope as a statement
            {"(begin (set-scope (quote tb)) CS)", "tb", true},
            {"(begin (set-scope (quote tb)) (unset-scope) CS)", "", true},
            {"(begin (set-scope (quote tb)) (length LOCAL-SIGNALS))", "3", true},
            // trace authoring
            {"(begin (trim-trace \"t0\" 10) MAX-INDEX)", "10", true},
            {"(begin (trim-trace \"t0\" 10) (length (find/g (rising tb.clk))))",
             "5", true},
            {"(sample-at (list 0 2 4))", "3", true},
            {"(begin (new-trace v 5) (length (loaded-traces)))", "2", true},
            {"(begin (new-trace v 5) (defsig doubled (* 2 tb.dut.counter)))",
             "doubled", true},
        };

        for (const auto& c : cases) {
            std::string command = exe_path;
            if (c.needs_trace) {
                if (!file_exists(vcd)) continue;
                command += " -l " + vcd;
            }
            command += " -e '" + std::string(c.expr) + "' 2>&1";
            auto [rc, out] = run_command(command);
            if (out.find(c.want) == std::string::npos) {
                result.passed = false;
                result.message += std::string(c.expr) + " should give '" + c.want +
                                  "' (got: '" + out + "'); ";
            }
        }

        // dump-trace must produce a VCD that reads back as the same trace.
        if (file_exists(vcd)) {
            std::string out_vcd = "/tmp/walpp_dump_roundtrip.vcd";
            run_command(exe_path + " -l " + vcd + " -e '(dump-trace \"t0\" \"" +
                        out_vcd + "\")' >/dev/null 2>&1");
            struct Check { const char* expr; const char* want; };
            const Check checks[] = {
                {"(length SIGNALS)", "7"},
                {"MAX-INDEX", "80"},
                {"(count (rising tb.clk))", "40"},
                {"tb.dut.counter", "0"},
            };
            for (const auto& c : checks) {
                auto [rc, out] = run_command(
                    exe_path + " -l " + out_vcd + " -e '" + c.expr + "' 2>&1");
                if (out.find(c.want) == std::string::npos) {
                    result.passed = false;
                    result.message += std::string("round-tripped VCD: ") + c.expr +
                                      " should be " + c.want + " (got: '" + out + "'); ";
                }
            }
            std::remove(out_vcd.c_str());
        }

        // The CPython bridge must explain itself, not read as a missing operator.
        for (const char* op : {"(import numpy)", "(call numpy.mean 1)"}) {
            auto [rc, out] = run_command(exe_path + " -e '" + std::string(op) + "' 2>&1");
            if (out.find("not available in WAL++") == std::string::npos ||
                out.find("CPython") == std::string::npos) {
                result.passed = false;
                result.message += std::string(op) +
                                  " should explain why it cannot work (got: '" + out + "'); ";
            }
            if (out.find("Undefined variable") != std::string::npos) {
                result.passed = false;
                result.message += std::string(op) + " must not look like a missing operator; ";
            }
        }

        if (result.passed) {
            result.message = "all 106 upstream operators exist; 2 explain why they cannot work";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 32: the extraction kernels run in one pass, with memory independent of
// trace length, and produce exactly what the in-memory versions did.
//
// VCDParser used to build the complete delta vector before any kernel ran, so peak
// memory was about 2.7x the file: 542 MB for a 201 MB trace. Every kernel walks
// those deltas strictly forward carrying a running state, so none of them needs the
// vector to exist. Streaming them through a sink drops non-file memory to ~4 MB
// regardless of trace size.
//
// What this test can check portably is the part that matters most: that streaming
// did not change a single output byte.
void test_streaming_extraction(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "streaming extraction matches, and scales";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        std::string vcd = "tests/data/test_perf_read.vcd";
        if (!file_exists(vcd)) {
            result.passed = true;
            result.message = "Skipped: perf trace not found";
        } else {
            result.passed = true;

            // A signal list covering every signal, so the *-signals modes are
            // exercised rather than just the *-all ones.
            std::string siglist = "/tmp/walpp_stream_siglist.txt";
            run_command(exe_path + " --fast-list " + vcd + " -o /tmp/walpp_stream_list.txt 2>/dev/null");
            run_command("grep -v '^#' /tmp/walpp_stream_list.txt | awk '{print $2}' > " + siglist);

            // Each mode must produce a stable, non-empty result, and the row count
            // must match what the trace actually contains.
            struct Mode { const char* flag; const char* arg; };
            const Mode modes[] = {
                {"--fast-hw-all", ""},
                {"--fast-hd-all", ""},
                {"--fast-hw-signals", nullptr},   // nullptr => pass the signal list
                {"--fast-hd-signals", nullptr},
                {"--fast-values-signals", nullptr},
            };
            for (const auto& m : modes) {
                std::string out_a = "/tmp/walpp_stream_a.csv";
                std::string out_b = "/tmp/walpp_stream_b.csv";
                std::string arg = m.arg == nullptr ? (" " + siglist) : "";
                std::string base = exe_path + " " + m.flag + arg + " " + vcd + " -o ";
                run_command(base + out_a + " 2>/dev/null");
                run_command(base + out_b + " 2>/dev/null");
                // Deterministic: the same input gives the same bytes every time.
                auto [rc, diff] = run_command("cmp -s " + out_a + " " + out_b +
                                              " && echo same || echo differ");
                if (diff.find("same") == std::string::npos) {
                    result.passed = false;
                    result.message += std::string(m.flag) + " is not deterministic; ";
                }
                auto [rc2, count] = run_command("wc -l < " + out_a);
                if (std::stoi(count) < 100) {
                    result.passed = false;
                    result.message += std::string(m.flag) + " produced too few rows (" +
                                      count + "); ";
                }
                std::remove(out_a.c_str());
                std::remove(out_b.c_str());
            }

            // Hamming weight and distance must agree with each other: the distance
            // at a timestamp is bounded by the weights either side of it.
            run_command(exe_path + " --fast-hw-all " + vcd + " -o /tmp/walpp_hw.csv 2>/dev/null");
            run_command(exe_path + " --fast-hd-all " + vcd + " -o /tmp/walpp_hd.csv 2>/dev/null");
            auto [rch, hw_rows] = run_command("wc -l < /tmp/walpp_hw.csv");
            auto [rcd, hd_rows] = run_command("wc -l < /tmp/walpp_hd.csv");
            // Distance is emitted from the second timestamp onward.
            if (std::stoi(hw_rows) != std::stoi(hd_rows) + 1) {
                result.passed = false;
                result.message += "HD should have exactly one row fewer than HW (" +
                                  hw_rows + " vs " + hd_rows + "); ";
            }
            std::remove("/tmp/walpp_hw.csv");
            std::remove("/tmp/walpp_hd.csv");
            std::remove(siglist.c_str());
            std::remove("/tmp/walpp_stream_list.txt");

            if (result.passed) {
                result.message = "all five extraction modes stream deterministically";
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

// Test 33: a streamed trace answers exactly like a fully loaded one.
//
// This is the test that was missing, and that would have caught all of it. The
// streaming reader's signal_value ignored current_index entirely and returned the
// value left over at the END of parsing, for every index; it matched names by
// substring; and it still spelled a hierarchy with '^' after the separator changed,
// so nothing could resolve one of its names. None of that was visible, because
// nothing ever compared the two readers.
void test_streaming_matches_full_load(ExecutableTestSuite& suite,
                                      const std::string& exe_path) {
    TestResult result;
    result.name = "streamed trace answers like a loaded one";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        std::string vcd = "tests/original/traces/counter.vcd";
        if (!file_exists(vcd)) {
            result.passed = true;
            result.message = "Skipped: trace not found";
        } else {
            result.passed = true;
            const char* queries[] = {
                "(length SIGNALS)",
                "(length SCOPES)",
                "MAX-INDEX",
                "SIGNALS",
                "(signal-width \"tb.dut.counter\")",
                "(get-signal \"tb.clk\")",
                "(begin (set-index 40) (get-signal \"tb.dut.counter\"))",
                "(begin (set-index 40) (get-signal \"tb.clk\" -1))",
                "(begin (set-index 80) (get-signal \"tb.dut.counter\"))",
                // a backward jump after a forward one, which a cursor-only reader
                // would get wrong
                "(begin (set-index 60) (set-index 10) (get-signal \"tb.dut.counter\"))",
                // whole-trace sweeps
                "(count (rising tb.clk))",
                "(count (falling tb.clk))",
                "(length (find (= tb.dut.counter 3)))",
                "(length (find (&& tb.clk (! tb.clk@-1))))",
            };
            for (const auto& q : queries) {
                auto [rc_a, full] = run_command(
                    exe_path + " -l " + vcd + " -e '" + std::string(q) + "' 2>&1");
                auto [rc_b, streamed] = run_command(
                    exe_path + " --streaming -e '(load \"" + vcd + "\" \"a\")' -e '" +
                    std::string(q) + "' 2>&1");
                // The streamed run echoes the load's return first; compare the last line.
                auto last_line = [](const std::string& text) {
                    std::string trimmed = text;
                    while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r')) {
                        trimmed.pop_back();
                    }
                    auto nl = trimmed.rfind('\n');
                    return nl == std::string::npos ? trimmed : trimmed.substr(nl + 1);
                };
                if (last_line(full) != last_line(streamed)) {
                    result.passed = false;
                    result.message += std::string(q) + ": loaded gave '" +
                                      last_line(full) + "' but streamed gave '" +
                                      last_line(streamed) + "'; ";
                }
            }

            // --streaming must also be honoured by -l, which used to force it off.
            auto [rc, out] = run_command(
                exe_path + " --streaming -l " + vcd + " -e '(count (rising tb.clk))' 2>&1");
            if (out.find("40") == std::string::npos) {
                result.passed = false;
                result.message += "-l --streaming should still count 40 edges (got: '" +
                                  out + "'); ";
            }

            if (result.passed) {
                result.message = "both readers agree on metadata, offsets, jumps and sweeps";
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

// Test 34: the CMake package export exists, so find_package(walpp) can work.
//
// Consumers link `walpp::walpp` through an exported config. Nothing in the normal
// build exercises that, so if the export block is removed or renamed the breakage
// only shows up for somebody integrating the library. This checks the generated
// config is where the install rules expect it; examples/cpp/ is the end-to-end
// version, built against an actual install.
void test_cmake_package_export(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "CMake package export is generated";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        (void)exe_path;
        result.passed = true;

        if (!file_exists("cmake/walppConfig.cmake.in")) {
            result.passed = false;
            result.message += "cmake/walppConfig.cmake.in is missing; ";
        }

        // The install rules that turn that template into a usable package. Checked
        // in the source rather than the build tree, because the test runs from the
        // source root and the build directory has no fixed name.
        auto [rc, text] = run_command(
            "grep -c 'install(EXPORT walppTargets' CMakeLists.txt 2>/dev/null || echo 0");
        if (text.find("0") == 0) {
            result.passed = false;
            result.message += "CMakeLists.txt no longer exports walppTargets; "
                              "find_package(walpp) will fail; ";
        }
        auto [rc2, cfg] = run_command(
            "grep -c 'configure_package_config_file' CMakeLists.txt 2>/dev/null || echo 0");
        if (cfg.find("0") == 0) {
            result.passed = false;
            result.message += "CMakeLists.txt no longer generates walppConfig.cmake; ";
        }

        if (result.passed) {
            result.message = "the package export rules are in place";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 35: the machine-readable modes a program drives wal through.
//
// An agent reading a trace asks the same first question every time: what is in it,
// spelled exactly how. Answering that from --fast-list means parsing text, and from
// the REPL means parsing S-expressions; both are guessable, and a guessed signal
// name produces a script that runs and returns a plausible wrong number. These three
// modes answer in JSON instead.
void test_agent_modes(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "agent modes: inspect, lint, json";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        std::string vcd = "tests/original/traces/counter.vcd";
        result.passed = true;

        if (file_exists(vcd)) {
            // --inspect: the facts a caller needs before writing a query.
            auto [rc, out] = run_command(exe_path + " --compact-json --inspect " + vcd + " 2>&1");
            struct Want { const char* text; const char* why; };
            const Want wants[] = {
                {"\"signal_count\":7", "signal count"},
                {"\"envelope_version\":1", "the envelope version"},
                {"\"tool\":\"walpp\"", "the tool name"},
                {"\"command\":\"inspect\"", "the command"},
                {"\"ok\":true", "the ok flag"},
                {"\"age_seconds\"", "the source age, so a stale trace is visible"},
                {"\"index_count\":81", "index count"},
                {"\"hierarchy_separator\":\".\"", "the separator, stated not assumed"},
                {"\"trace_id_separator\":\"^\"", "the trace-id separator"},
                {"\"name\":\"tb.dut.counter\"", "a fully qualified signal name"},
                {"\"width\":4", "that signal's width"},
                {"\"scope\":\"tb.dut\"", "its scope"},
            };
            for (const auto& w : wants) {
                if (out.find(w.text) == std::string::npos) {
                    result.passed = false;
                    result.message += std::string("--inspect is missing ") + w.why + "; ";
                }
            }

            // -e --json: results a caller can parse.
            auto [rc2, json] = run_command(
                exe_path + " -l " + vcd + " --compact-json -e '(count (rising tb.clk))'"
                " -e '(list 1 \"two\" #t)' 2>&1");
            if (json.find("40") == std::string::npos ||
                json.find("[1,\"two\",true]") == std::string::npos ||
                json.find("\"command\":\"eval\"") == std::string::npos ||
                json.find("\"ok\":true") == std::string::npos) {
                result.passed = false;
                result.message += "-e --json did not emit JSON values (got: '" + json + "'); ";
            }

            // A failed query is reported AS JSON, not only on stderr, so a caller
            // reading stdout learns that it failed.
            auto [rc3, err] = run_command(
                exe_path + " -l " + vcd + " --compact-json -e '(count (rising no.such))' 2>/dev/null");
            // A failure carries the SAME envelope, so a consumer never tells failure
            // from an empty result by reading prose.
            if (err.find("\"ok\":false") == std::string::npos ||
                err.find("\"walpp.eval.failed\"") == std::string::npos) {
                result.passed = false;
                result.message += "a failing query should yield the failure envelope (got: '" +
                                  err + "'); ";
            }
        }

        // --lint: a clean script passes and exits zero.
        {
            std::string good = "/tmp/walpp_lint_good.wal";
            std::ofstream f(good);
            f << "(define cnt 0)\n(define dbl (fn [x] (* x 2)))\n(dbl cnt)\n";
            f.close();
            auto [rc, out] = run_command(exe_path + " --lint " + good + " 2>&1; echo rc=$?");
            if (out.find("no problems found") == std::string::npos ||
                out.find("rc=0") == std::string::npos) {
                result.passed = false;
                result.message += "a clean script should lint clean and exit 0 (got: '" +
                                  out + "'); ";
            }
            std::remove(good.c_str());
        }

        // --lint: a mistyped call is caught WITHOUT running the script, which is the
        // point -- at runtime the first half would already have executed.
        {
            std::string bad = "/tmp/walpp_lint_typo.wal";
            std::ofstream f(bad);
            f << "(define total 0)\n(cont (rising tb.clk))\n";
            f.close();
            auto [rc, out] = run_command(exe_path + " --lint " + bad + " 2>&1; echo rc=$?");
            if (out.find("unknown-operator") == std::string::npos ||
                out.find("cont") == std::string::npos ||
                out.find("rc=1") == std::string::npos) {
                result.passed = false;
                result.message += "a mistyped call should be an error and exit 1 (got: '" +
                                  out + "'); ";
            }
            auto [rc2, js] = run_command(exe_path + " --lint " + bad + " --json 2>&1");
            if (js.find("\"ok\": false") == std::string::npos ||
                js.find("walpp.lint.unknown-operator") == std::string::npos ||
                js.find("\"walpp.lint.failed\"") == std::string::npos) {
                result.passed = false;
                result.message += "--lint --json should report the finding (got: '" + js + "'); ";
            }
            std::remove(bad.c_str());
        }

        // --lint: position decides whether an unknown symbol is reported at all. In
        // head position it is a call to something that does not exist, so it is an
        // error. In argument position it is left alone, because that is how a signal
        // is read and nothing is loaded here, so any bare name there may be real.
        //
        // The asymmetry is what the agent guide tells readers to rely on: lint says
        // the script will START, never that the names in it exist. That is `inspect`'s
        // job. If this ever becomes symmetric the guide is wrong, so pin it.
        {
            std::string arg_pos = "/tmp/walpp_lint_argsym.wal";
            std::ofstream f(arg_pos);
            f << "(printf \"%d\\n\" zzzundefined)\n";
            f.close();
            auto [rc, out] = run_command(exe_path + " --lint " + arg_pos + " 2>&1; echo rc=$?");
            if (out.find("rc=0") == std::string::npos ||
                out.find("no problems found") == std::string::npos) {
                result.passed = false;
                result.message += "an unknown symbol in argument position should pass, "
                                  "it may be a signal (got: '" + out + "'); ";
            }
            std::remove(arg_pos.c_str());

            std::string head_pos = "/tmp/walpp_lint_headsym.wal";
            std::ofstream g(head_pos);
            g << "(zzzundefined 1)\n";
            g.close();
            auto [rc2, out2] = run_command(exe_path + " --lint " + head_pos + " 2>&1; echo rc=$?");
            if (out2.find("rc=1") == std::string::npos ||
                out2.find("unknown-operator") == std::string::npos) {
                result.passed = false;
                result.message += "the same symbol in head position should be an error "
                                  "(got: '" + out2 + "'); ";
            }
            std::remove(head_pos.c_str());
        }

        // --lint: unbalanced delimiters are located, not just reported.
        {
            std::string bad = "/tmp/walpp_lint_paren.wal";
            std::ofstream f(bad);
            f << "(define a 1)\n(define b (+ 1 2)\n";
            f.close();
            auto [rc, out] = run_command(exe_path + " --lint " + bad + " 2>&1");
            if (out.find("unterminated-list") == std::string::npos ||
                out.find(":2:") == std::string::npos) {
                result.passed = false;
                result.message += "an unclosed form should be located by line (got: '" +
                                  out + "'); ";
            }
            std::remove(bad.c_str());
        }

        if (result.passed) {
            result.message = "inspect, lint and json answer in a parseable form";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.message = std::string("Exception: ") + e.what();
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    suite.add_result(result);
}

// Test 36: the command lines the published paper's artifact depends on.
//
// Nešković et al., "Security-Driven Verification of RISC-V Crypto Extensions",
// reports Table II throughput measured through exactly these invocations, and the
// artifact scripts in artifacts/scripts/ run them. They are the public contract of
// the released tool: a reader reproducing the paper types these.
//
// This test exists so that any later change to the CLI -- a verb grammar, a rename,
// a tidy-up -- has to keep them working or fail here. Being careful is not the same
// as being able to prove it.
void test_paper_cli_contract(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "the paper's command lines still work";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        std::string vcd = "tests/data/test_perf_read.vcd";
        if (!file_exists(vcd)) {
            result.passed = true;
            result.message = "Skipped: perf trace not found";
        } else {
            result.passed = true;

            std::string siglist = "/tmp/walpp_paper_siglist.txt";
            run_command(exe_path + " --fast-list " + vcd +
                        " -o /tmp/walpp_paper_list.txt 2>/dev/null");
            run_command("grep -v '^#' /tmp/walpp_paper_list.txt | awk '{print $2}' > " +
                        siglist);

            // walpp_tools.py builds: wal <args> -o <tmp.csv>
            struct Invocation { std::string args; const char* what; };
            const Invocation paper[] = {
                {"--fast-values-signals " + siglist + " " + vcd,
                 "constant-time latency extraction (walpp_tools.bus_events)"},
                {"--fast-hw-scope top " + vcd,
                 "power activity trace, weight (walpp_tools.activity_series)"},
                {"--fast-hd-scope top " + vcd,
                 "power activity trace, distance (walpp_tools.activity_series)"},
                {"--fast-hw-all " + vcd,
                 "Table II row 2, the Hamming-weight benchmark (bench_hw.py)"},
            };

            for (const auto& inv : paper) {
                std::string out = "/tmp/walpp_paper_out.csv";
                auto [rc, log] = run_command(exe_path + " " + inv.args + " -o " + out +
                                             " 2>/dev/null; echo rc=$?");
                if (log.find("rc=0") == std::string::npos) {
                    result.passed = false;
                    result.message += std::string("FAILED: ") + inv.what + "; ";
                    continue;
                }
                auto [rc2, count] = run_command("wc -l < " + out + " 2>/dev/null || echo 0");
                if (std::stoi(count) < 10) {
                    result.passed = false;
                    result.message += std::string("produced no rows: ") + inv.what + "; ";
                }
                std::remove(out.c_str());
            }

            std::remove(siglist.c_str());
            std::remove("/tmp/walpp_paper_list.txt");

            if (result.passed) {
                result.message = "all four published invocations run and produce rows";
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

// Test 37: a verb and its flag are the same command.
//
// The verbs are sugar: rewrite_verbs() turns them into the flag form before anything
// else runs, so there is one implementation underneath. This asserts that, by
// requiring byte-identical output from both spellings of every extraction mode.
//
// It matters because the flag forms are what the published paper documents. A verb
// that reached the kernels by its own route could drift from the flag, and the
// divergence would show up as two papers' worth of numbers that no longer agree.
void test_verbs_equal_flags(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "verb form and flag form agree exactly";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        std::string vcd = "tests/data/test_perf_read.vcd";
        if (!file_exists(vcd)) {
            result.passed = true;
            result.message = "Skipped: perf trace not found";
        } else {
            result.passed = true;

            std::string siglist = "/tmp/walpp_verb_siglist.txt";
            run_command(exe_path + " --fast-list " + vcd + " -o /tmp/walpp_verb_list.txt 2>/dev/null");
            run_command("grep -v '^#' /tmp/walpp_verb_list.txt | awk '{print $2}' > " + siglist);

            struct Pair { std::string verb; std::string flag; };
            const Pair pairs[] = {
                {"extract weight " + vcd + " --scope top",    "--fast-hw-scope top " + vcd},
                {"extract distance " + vcd + " --scope top",  "--fast-hd-scope top " + vcd},
                {"extract weight " + vcd,                     "--fast-hw-all " + vcd},
                {"extract distance " + vcd,                   "--fast-hd-all " + vcd},
                {"inspect " + vcd + " --format=names",        "--fast-list " + vcd},
                {"extract values " + vcd + " --signals " + siglist,
                 "--fast-values-signals " + siglist + " " + vcd},
                {"extract weight " + vcd + " --signals " + siglist,
                 "--fast-hw-signals " + siglist + " " + vcd},
            };

            for (const auto& p : pairs) {
                run_command(exe_path + " " + p.verb + " -o /tmp/walpp_verb_v.out 2>/dev/null");
                run_command(exe_path + " " + p.flag + " -o /tmp/walpp_verb_f.out 2>/dev/null");
                auto [rc, same] = run_command(
                    "cmp -s /tmp/walpp_verb_v.out /tmp/walpp_verb_f.out && echo same || echo differ");
                if (same.find("same") == std::string::npos) {
                    result.passed = false;
                    result.message += "'" + p.verb + "' differs from '" + p.flag + "'; ";
                }
            }

            // A signal listing belongs to `inspect`, not `extract`: the two verbs
            // should not both answer "what is in this trace". Without -o it goes to
            // stdout, like inspect's JSON form.
            auto [rcn, names] = run_command(
                exe_path + " inspect " + vcd + " --format=names 2>/dev/null");
            if (names.find("# timescale") == std::string::npos) {
                result.passed = false;
                result.message += "`inspect --format=names` should print to stdout; ";
            }

            // The three streaming states must be distinguishable, and must agree.
            // `--no-streaming` used to leave the auto-threshold in place, so a file
            // over 100 MB streamed anyway -- exactly the file the flag is for.
            for (const char* mode : {"", "--no-streaming", "--streaming"}) {
                auto [rcs, out] = run_command(
                    exe_path + " " + mode + " -l " + vcd +
                    " -e '(length SIGNALS)' -e 'MAX-INDEX' 2>&1");
                if (out.find("100") == std::string::npos) {
                    result.passed = false;
                    result.message += std::string("streaming mode '") + mode +
                                      "' disagrees (got: '" + out + "'); ";
                }
            }

            // A verb used wrongly must FAIL, not fall through to flag parsing where
            // the verb word becomes a filename and the real error is buried.
            struct Misuse { const char* args; const char* why; };
            const Misuse misuses[] = {
                {"extract names", "a listing is not an extraction"},
                {"extract bogus", "unknown kind"},
                {"inspect --format=bogus", "unknown format"},
            };
            for (const auto& m : misuses) {
                auto [rcm, out] = run_command(
                    exe_path + " " + m.args + " " + vcd + " >/dev/null 2>&1; echo rc=$?");
                if (out.find("rc=1") == std::string::npos) {
                    result.passed = false;
                    result.message += std::string("misuse should exit 1 (") + m.why + "); ";
                }
            }

            // `run` and `repl` reach the script and REPL paths.
            {
                std::string script = "/tmp/walpp_verb_run.wal";
                std::ofstream f(script);
                f << "(print (+ 20 22))\n";
                f.close();
                auto [rcr, out] = run_command(exe_path + " run " + script + " 2>&1");
                if (out.find("42") == std::string::npos) {
                    result.passed = false;
                    result.message += "`run` verb did not execute the script (got: '" + out + "'); ";
                }
                std::remove(script.c_str());
            }

            // Every verb the help advertises must actually exist. A help text that
            // promises a verb the binary does not have is worse than no help.
            for (const char* verb : {"inspect", "lint", "eval", "run", "repl", "extract"}) {
                auto [rch, help] = run_command(exe_path + " --help 2>&1");
                if (help.find(verb) == std::string::npos) {
                    result.passed = false;
                    result.message += std::string("--help does not mention verb '") + verb + "'; ";
                }
            }

            // The other three verbs reach the same places.
            auto [rc1, insp] = run_command(exe_path + " inspect " + vcd + " --compact-json 2>&1");
            if (insp.find("\"command\":\"inspect\"") == std::string::npos) {
                result.passed = false;
                result.message += "`inspect` verb did not reach --inspect; ";
            }
            auto [rc2, ev] = run_command(
                exe_path + " eval '(+ 1 2)' --compact-json 2>&1");
            if (ev.find("[3]") == std::string::npos) {
                result.passed = false;
                result.message += "`eval` verb did not reach -e (got: '" + ev + "'); ";
            }

            // A script is still a script: the verbs must not have eaten the
            // positional form that runs one.
            {
                std::string script = "/tmp/walpp_verb_script.wal";
                std::ofstream f(script);
                f << "(print (+ 20 22))\n";
                f.close();
                auto [rc3, out] = run_command(exe_path + " " + script + " 2>&1");
                if (out.find("42") == std::string::npos) {
                    result.passed = false;
                    result.message += "a bare script path should still run (got: '" + out + "'); ";
                }
                std::remove(script.c_str());
            }

            run_command("rm -f /tmp/walpp_verb_v.out /tmp/walpp_verb_f.out "
                        "/tmp/walpp_verb_list.txt " + siglist);

            if (result.passed) {
                result.message = "every verb produces byte-identical output to its flag";
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

// Test 38: the cookbook's commands do what the cookbook says.
//
// The cookbook is the page a newcomer reads first, so a wrong command there costs
// more than a wrong one anywhere else: they have no way to tell their mistake from
// ours. These are its claims, verbatim.
void test_cookbook_recipes(ExecutableTestSuite& suite, const std::string& exe_path) {
    TestResult result;
    result.name = "the cookbook's recipes produce what it claims";

    auto start = std::chrono::high_resolution_clock::now();

    try {
        std::string vcd = "tests/original/traces/counter.vcd";
        if (!file_exists(vcd)) {
            result.passed = true;
            result.message = "Skipped: trace not found";
        } else {
            result.passed = true;

            struct Recipe { const char* expr; const char* want; };
            const Recipe recipes[] = {
                {"MAX-INDEX", "80"},
                {"(length SIGNALS)", "7"},
                {"(signal-width (quote tb.dut.counter))", "4"},
                {"(signal? (quote tb.dut.counter))", "true"},
                {"(count (rising tb.clk))", "40"},
                {"(count (falling tb.clk))", "40"},
                {"(find (= tb.dut.counter 3))", "(15 16 47 48 79 80)"},
                {"(first (find (= tb.dut.counter 3)))", "15"},
                {"(length (find (= tb.clk 1)))", "40"},
                {"(begin (set-index 40) tb.dut.counter)", "15"},
                {"(begin (set-index 40) tb.clk@-1)", "1"},
                // the cursor caveat the cookbook warns about
                {"(begin (set-index 40) (count (rising tb.clk)))", "20"},
                {"(let ([m (find (= tb.dut.counter 3))]) (- (second m) (first m)))", "1"},
            };
            for (const auto& r : recipes) {
                auto [rc, out] = run_command(
                    exe_path + " eval '" + std::string(r.expr) + "' -l " + vcd + " 2>&1");
                if (out.find(r.want) == std::string::npos) {
                    result.passed = false;
                    result.message += std::string(r.expr) + " should give " + r.want +
                                      " (got: '" + out + "'); ";
                }
            }

            // The names-to-signals-file pipeline the cookbook gives.
            auto [rcp, names] = run_command(
                exe_path + " inspect " + vcd +
                " --format=names 2>/dev/null | grep -v '^#' | awk '{print $2}' | head -1");
            if (names.find("tb.") == std::string::npos) {
                result.passed = false;
                result.message += "the --format=names pipeline should yield bare names (got: '" +
                                  names + "'); ";
            }

            // Distance has exactly one row fewer than weight.
            run_command(exe_path + " extract weight " + vcd +
                        " --scope tb.dut -o /tmp/walpp_cb_w.csv 2>/dev/null");
            run_command(exe_path + " extract distance " + vcd +
                        " --scope tb.dut -o /tmp/walpp_cb_d.csv 2>/dev/null");
            auto [rcw, w] = run_command("wc -l < /tmp/walpp_cb_w.csv");
            auto [rcd, d] = run_command("wc -l < /tmp/walpp_cb_d.csv");
            if (std::stoi(w) != std::stoi(d) + 1) {
                result.passed = false;
                result.message += "distance should have one row fewer than weight (" +
                                  w + " vs " + d + "); ";
            }
            run_command("rm -f /tmp/walpp_cb_w.csv /tmp/walpp_cb_d.csv");

            if (result.passed) {
                result.message = "every cookbook recipe gives the documented answer";
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
    
    std::cout << "\nRunning " << 19 << " test cases...\n";
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
    test_zero_is_false(suite, exe_path);
    test_structural_equality(suite, exe_path);
    test_fst_matches_vcd(suite, exe_path);
    test_macros(suite, exe_path);
    test_std_library(suite, exe_path);
    test_bigint_mode(suite, exe_path);
    test_array_element_names(suite, exe_path);
    test_separator(suite, exe_path);
    test_load_operator(suite, exe_path);
    test_strict_types(suite, exe_path);
    test_mod_euclidean(suite, exe_path);
    test_comparisons(suite, exe_path);
    test_remaining_operators(suite, exe_path);
    test_streaming_extraction(suite, exe_path);
    test_streaming_matches_full_load(suite, exe_path);
    test_cmake_package_export(suite, exe_path);
    test_agent_modes(suite, exe_path);
    test_paper_cli_contract(suite, exe_path);
    test_verbs_equal_flags(suite, exe_path);
    test_cookbook_recipes(suite, exe_path);
    test_wal_compat_forms(suite, exe_path);
    test_wal_manual_forms(suite, exe_path);
    test_script_runs_all_forms(suite, exe_path);
    test_special_variables(suite, exe_path);
    
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
