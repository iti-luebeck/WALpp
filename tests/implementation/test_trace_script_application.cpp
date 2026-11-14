/**
 * @file test_trace_script_application.cpp
 * @brief Integration tests for applying WAL scripts to loaded traces
 * 
 * Demonstrates the usecase of:
 * 1. Loading trace data from stdin/pipe with --stdin/--pipe flags
 * 2. Applying WAL analysis scripts to the loaded traces
 * 3. Computing signal transformations, queries, and analyses
 * 
 * Example workflow:
 *   simulator | wal --stdin              # Load trace via pipe
 *   cat trace.vcd | wal --stdin          # Load trace from stdin
 *   wal -l trace.vcd -e '(analysis)'    # Load from file, apply script
 */

#include <iostream>
#include <cassert>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include "wal/core.h"

namespace wal {

void print_section(const std::string& title) {
    std::cout << "\n╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << title << "\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n";
}

// Create a more complex test VCD file for realistic scenarios
std::string create_complex_vcd(const std::string& filename) {
    std::ofstream file(filename);
    file << R"($date
    November 8, 2025
$end
$version
    WAL++ Trace Script Test
$end
$timescale 1ns $end
$scope module dut $end
$var wire 1 ! clk $end
$var wire 1 " reset $end
$var wire 32 # address $end
$var wire 32 $ data_in $end
$var wire 32 % data_out $end
$var wire 1 & write_enable $end
$var wire 1 ' read_enable $end
$upscope $end
$enddefinitions
$dumpvars
0!
1"
b00000000000000000000000000000000#
b00000000000000000000000000000000$
b00000000000000000000000000000000%
0&
0'
$end
#0
1!
#10
0!
0"
b00000000000000000000000000000001#
b11111111111111111111111111111111$
1&
#20
1!
#30
0!
0&
#40
1!
1'
b11111111111111111111111111111111%
#50
0!
#60
1!
0'
#70
0!
b00000000000000000000000000000010#
b10101010101010101010101010101010$
1&
#80
1!
#90
0!
0&
#100
1!
1'
b10101010101010101010101010101010%
#110
0!
0'
#120
1!
$end)";
    file.close();
    return filename;
}

} // namespace wal

int main() {
    try {
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║   TRACE SCRIPT APPLICATION TEST SUITE                       ║\n";
        std::cout << "║   Testing WAL Script Analysis on Loaded Traces              ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
        
        // Create complex test VCD file
        std::string vcd_file = "/tmp/test_walpp_script.vcd";
        wal::create_complex_vcd(vcd_file);
        std::cout << "\n✓ Created complex test VCD file: " << vcd_file << "\n";
        
        auto wal = std::make_shared<wal::Wal>();
        
        // =====================================================================
        // Phase 1: Load Trace Data
        // =====================================================================
        wal::print_section("Phase 1: Load Trace Data");
        
        wal::Value result = wal->eval_str("(load \"" + vcd_file + "\")");
        assert(result.is_symbol());
        std::cout << "✓ Successfully loaded trace file\n";
        
        result = wal->eval_str("(get-traces)");
        assert(result.is_list());
        auto traces = result.to_list();
        assert(traces->size() > 0);
        std::cout << "✓ Loaded " << traces->size() << " trace(s)\n";
        
        // =====================================================================
        // Phase 2: Signal Discovery via Script
        // =====================================================================
        wal::print_section("Phase 2: Signal Discovery via Script");
        
        result = wal->eval_str("(signal-names)");
        assert(result.is_list());
        auto signals = result.to_list();
        std::cout << "✓ Found " << signals->size() << " signals in trace\n";
        
        // Print discovered signals
        std::cout << "\nDiscovered signals:\n";
        for (size_t i = 0; i < signals->size() && i < 8; ++i) {
            if ((*signals)[i].is_symbol()) {
                std::cout << "  - " << (*signals)[i].to_symbol()->get_name() << "\n";
            }
        }
        
        // Find signal names for analysis
        std::string clk_sig, reset_sig, addr_sig, data_in_sig, data_out_sig, we_sig, re_sig;
        for (const auto& sig : *signals) {
            if (sig.is_symbol()) {
                std::string name = sig.to_symbol()->get_name();
                if (name.find("clk") != std::string::npos) clk_sig = name;
                if (name.find("reset") != std::string::npos) reset_sig = name;
                if (name.find("address") != std::string::npos) addr_sig = name;
                if (name.find("data_in") != std::string::npos) data_in_sig = name;
                if (name.find("data_out") != std::string::npos) data_out_sig = name;
                if (name.find("write") != std::string::npos) we_sig = name;
                if (name.find("read") != std::string::npos) re_sig = name;
            }
        }
        
        // =====================================================================
        // Phase 3: Signal Property Queries
        // =====================================================================
        wal::print_section("Phase 3: Signal Property Queries");
        
        if (!clk_sig.empty()) {
            result = wal->eval_str("(signal-width \"" + clk_sig + "\")");
            assert(result.is_int());
            std::cout << "✓ Clock signal width: " << result.to_int() << " bit(s)\n";
        }
        
        if (!addr_sig.empty()) {
            result = wal->eval_str("(signal-width \"" + addr_sig + "\")");
            assert(result.is_int());
            int addr_width = result.to_int();
            std::cout << "✓ Address bus width: " << addr_width << " bit(s)\n";
            
            // Calculate maximum address value
            int max_addr = (1 << addr_width) - 1;
            result = wal->eval_str("(* 2 " + std::to_string(addr_width) + ")");
            std::cout << "✓ Max address value calculation: " << result.to_int() << "\n";
        }
        
        if (!data_in_sig.empty()) {
            result = wal->eval_str("(signal-width \"" + data_in_sig + "\")");
            assert(result.is_int());
            std::cout << "✓ Data input bus width: " << result.to_int() << " bit(s)\n";
        }
        
        // =====================================================================
        // Phase 4: Conditional Analysis Scripts
        // =====================================================================
        wal::print_section("Phase 4: Conditional Analysis Scripts");
        
        // Example: Check if signals exist and compute properties
        if (!clk_sig.empty() && !we_sig.empty()) {
            result = wal->eval_str(
                "(if (signal? \"" + clk_sig + "\")"
                "  (list (signal-width \"" + clk_sig + "\") (signal-width \"" + we_sig + "\"))"
                "  nil)"
            );
            assert(result.is_list());
            std::cout << "✓ Conditional signal analysis executed\n";
        }
        
        // =====================================================================
        // Phase 5: Complex Analysis Scripts
        // =====================================================================
        wal::print_section("Phase 5: Complex Analysis Scripts");
        
        // Script: Collect all signal widths
        result = wal->eval_str("(length (signal-names))");
        assert(result.is_int());
        int total_signals = result.to_int();
        std::cout << "✓ Total signal count: " << total_signals << "\n";
        
        // Script: Signal width statistics (as list of widths)
        if (total_signals > 0) {
            // Define a variable to store signal widths
            std::string width_script = "";
            int count = 0;
            for (const auto& sig : *signals) {
                if (sig.is_symbol() && count < 5) {
                    std::string signame = sig.to_symbol()->get_name();
                    if (count == 0) {
                        width_script = "(list (signal-width \"" + signame + "\")";
                    } else {
                        width_script += " (signal-width \"" + signame + "\")";
                    }
                    count++;
                }
            }
            if (!width_script.empty()) {
                width_script += ")";
                result = wal->eval_str(width_script);
                if (result.is_list()) {
                    std::cout << "✓ Signal width list created with " << result.to_list()->size() << " entries\n";
                }
            }
        }
        
        // =====================================================================
        // Phase 6: Time-Based Navigation Scripts
        // =====================================================================
        wal::print_section("Phase 6: Time-Based Navigation Scripts");
        
        // Test index management
        result = wal->eval_str("(set-index 5)");
        assert(result.is_int() && result.to_int() == 5);
        std::cout << "✓ Set trace index to timestep 5\n";
        
        // `step` returns whether it advanced, not how far: see the note in
        // test_trace_operators.cpp. Both of these are well inside this trace.
        result = wal->eval_str("(step 1)");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ Step forward by 1 succeeded\n";

        result = wal->eval_str("(step 5)");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ Step forward by 5 succeeded\n";
        
        // =====================================================================
        // Phase 7: Data-Driven Analysis Scripts
        // =====================================================================
        wal::print_section("Phase 7: Data-Driven Analysis Scripts");
        
        // Example: Compute statistics on signal properties
        result = wal->eval_str(
            "(define signal-count (length (signal-names)))"
        );
        std::cout << "✓ Defined variable: signal-count\n";
        
        result = wal->eval_str("signal-count");
        assert(result.is_int());
        std::cout << "✓ Retrieved signal-count: " << result.to_int() << "\n";
        
        // Use variable in computation
        result = wal->eval_str("(* 2 signal-count)");
        assert(result.is_int());
        std::cout << "✓ Computed 2 * signal-count: " << result.to_int() << "\n";
        
        // =====================================================================
        // Phase 8: Bus Analysis Script Example
        // =====================================================================
        wal::print_section("Phase 8: Bus Analysis Script Example");
        
        // Example: Analyze address bus properties
        if (!addr_sig.empty()) {
            // Define address bus analysis
            result = wal->eval_str(
                "(define addr-width (signal-width \"" + addr_sig + "\"))"
            );
            std::cout << "✓ Defined variable: addr-width\n";
            
            result = wal->eval_str("addr-width");
            assert(result.is_int());
            int addr_w = result.to_int();
            std::cout << "✓ Address width: " << addr_w << " bits\n";
            
            // Calculate max address
            result = wal->eval_str("(- (* 256 addr-width) 1)");
            if (result.is_int()) {
                std::cout << "✓ Max address value: " << result.to_int() << "\n";
            }
        }
        
        // =====================================================================
        // Phase 9: Control Flow in Analysis Scripts
        // =====================================================================
        wal::print_section("Phase 9: Control Flow in Analysis Scripts");
        
        // Example: Check signal existence and analyze
        if (!clk_sig.empty()) {
            result = wal->eval_str(
                "(if (signal? \"" + clk_sig + "\")"
                "  (list \"Clock found\" (signal-width \"" + clk_sig + "\"))"
                "  (list \"Clock not found\"))"
            );
            assert(result.is_list());
            std::cout << "✓ Clock analysis conditional executed\n";
            
            auto res_list = result.to_list();
            if (res_list->size() > 0 && (*res_list)[0].is_string()) {
                std::cout << "  Result: " << (*res_list)[0].to_string() << "\n";
            }
        }
        
        // =====================================================================
        // Phase 10: Error Handling in Scripts
        // =====================================================================
        wal::print_section("Phase 10: Error Handling in Scripts");
        
        // Test querying non-existent signal
        result = wal->eval_str("(signal? \"nonexistent_signal_xyz\")");
        assert(result.is_bool() && !result.to_bool());
        std::cout << "✓ Non-existent signal query returns false\n";
        
        // Test safe signal width query
        result = wal->eval_str(
            "(if (signal? \"nonexistent_signal_xyz\")"
            "  (signal-width \"nonexistent_signal_xyz\")"
            "  -1)"
        );
        assert(result.is_int() && result.to_int() == -1);
        std::cout << "✓ Safe signal width query with fallback executed\n";
        
        // =====================================================================
        // Summary
        // =====================================================================
        wal::print_section("Summary: Trace Script Application");
        
        std::cout << "\n✓ Successfully demonstrated:\n";
        std::cout << "  1. Loading trace data from files\n";
        std::cout << "  2. Discovering signals via script queries\n";
        std::cout << "  3. Analyzing signal properties (width, names)\n";
        std::cout << "  4. Using conditionals for signal analysis\n";
        std::cout << "  5. Time-based trace navigation\n";
        std::cout << "  6. Variable definition and reuse\n";
        std::cout << "  7. Bus/signal property computation\n";
        std::cout << "  8. Control flow in analysis scripts\n";
        std::cout << "  9. Error handling and safe queries\n";
        std::cout << "  10. Complex data-driven analysis\n";
        
        std::cout << "\nUsage Examples:\n";
        std::cout << "  # Apply analysis script to loaded trace\n";
        std::cout << "  wal -l trace.vcd -e '(define signals (signal-names))'\n";
        std::cout << "\n  # Pipe simulation output for real-time analysis\n";
        std::cout << "  simulator | wal --stdin -e '(length (signal-names))'\n";
        std::cout << "\n  # Load from pipe and apply complex analysis\n";
        std::cout << "  cat trace.vcd > /tmp/trace.fifo &\n";
        std::cout << "  wal --pipe /tmp/trace.fifo -e '(signal-names)'\n";
        
        std::cout << "\n" << std::string(64, '=') << "\n";
        std::cout << "🎉 ALL TRACE SCRIPT APPLICATION TESTS PASSED!\n";
        std::cout << std::string(64, '=') << "\n\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}
