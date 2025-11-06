#include <iostream>
#include <cassert>
#include <fstream>
#include <cstdlib>
#include "wal/core.h"

namespace wal {

void print_section(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

// Create a test VCD file
std::string create_test_vcd(const std::string& filename) {
    std::ofstream file(filename);
    file << R"($date
    Test Date
$end
$version
    Test VCD Generator
$end
$timescale 1ps $end
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

} // namespace wal

int main() {
    try {
        std::cout << "╔════════════════════════════════════════════════════════╗\n";
        std::cout << "║              VCD FILE PARSER TEST SUITE                ║\n";
        std::cout << "╚════════════════════════════════════════════════════════╝\n";
        
        // Create test VCD file
        std::string vcd_file = "/tmp/test_walpp.vcd";
        wal::create_test_vcd(vcd_file);
        std::cout << "\n✓ Created test VCD file: " << vcd_file << "\n";
        
        auto wal = std::make_shared<wal::Wal>();
        
        wal::print_section("VCD File Loading");
        
        // Test loading VCD file
        wal::Value result = wal->eval_str("(load \"" + vcd_file + "\")");
        assert(result.is_symbol());
        std::cout << "✓ Successfully loaded VCD file\n";
        
        // Test get-traces
        result = wal->eval_str("(get-traces)");
        assert(result.is_list());
        auto traces = result.to_list();
        assert(traces->size() > 0);
        std::cout << "✓ (get-traces) returns " << traces->size() << " trace(s)\n";
        
        wal::print_section("Signal Names & Discovery");
        
        // First, get actual signal names with scopes
        result = wal->eval_str("(signal-names)");
        assert(result.is_list());
        auto signal_list = result.to_list();
        assert(signal_list->size() >= 4);  // At least our 4 signals
        std::cout << "✓ (signal-names) returns " << signal_list->size() << " signals\n";
        
        // Extract signal names (they may have scope prefixes like "logic^clk")
        std::string clk_name = "clk", data_name = "data", valid_name = "valid", counter_name = "counter";
        
        for (const auto& sig : *signal_list) {
            if (sig.is_symbol()) {
                std::string name = sig.to_symbol()->get_name();
                if (name.find("clk") != std::string::npos) clk_name = name;
                if (name.find("data") != std::string::npos) data_name = name;
                if (name.find("valid") != std::string::npos) valid_name = name;
                if (name.find("counter") != std::string::npos) counter_name = name;
            }
        }
        std::cout << "\nDiscovered signal names with scope prefixes:\n";
        std::cout << "  - clk: " << clk_name << "\n";
        std::cout << "  - data: " << data_name << "\n";
        std::cout << "  - valid: " << valid_name << "\n";
        std::cout << "  - counter: " << counter_name << "\n\n";
        
        wal::print_section("Signal Queries");
        
        // Test signal?
        result = wal->eval_str("(signal? \"" + clk_name + "\")");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (signal? \"" + clk_name + "\") = true\n";
        
        result = wal->eval_str("(signal? \"" + data_name + "\")");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (signal? \"" + data_name + "\") = true\n";
        
        result = wal->eval_str("(signal? \"nonexistent\")");
        assert(result.is_bool() && !result.to_bool());
        std::cout << "✓ (signal? \"nonexistent\") = false\n";
        
        wal::print_section("Signal Metadata");
        
        // Test signal-width
        result = wal->eval_str("(signal-width \"" + clk_name + "\")");
        assert(result.is_int() && result.to_int() == 1);
        std::cout << "✓ (signal-width \"" + clk_name + "\") = 1\n";
        
        result = wal->eval_str("(signal-width \"" + data_name + "\")");
        assert(result.is_int() && result.to_int() == 8);
        std::cout << "✓ (signal-width \"" + data_name + "\") = 8\n";
        
        result = wal->eval_str("(signal-width \"" + counter_name + "\")");
        assert(result.is_int() && result.to_int() == 4);
        std::cout << "✓ (signal-width \"" + counter_name + "\") = 4\n";
        
        wal::print_section("Index Management");
        
        // Test get-index
        result = wal->eval_str("(get-index)");
        assert(result.is_list());
        std::cout << "✓ (get-index) returns indices\n";
        
        // Test set-index
        result = wal->eval_str("(set-index 2)");
        assert(result.is_int() && result.to_int() == 2);
        std::cout << "✓ (set-index 2) = 2\n";
        
        // Test step
        result = wal->eval_str("(step 1)");
        assert(result.is_int() && result.to_int() == 1);
        std::cout << "✓ (step 1) = 1\n";
        
        wal::print_section("Complex Queries");
        
        // Test composition
        result = wal->eval_str("(if (signal? \"" + clk_name + "\") (signal-width \"" + clk_name + "\") 0)");
        assert(result.is_int() && result.to_int() == 1);
        std::cout << "✓ (if (signal? ...) (signal-width ...) 0) = 1\n";
        
        // Test with list comprehension using known signals
        result = wal->eval_str("(list (signal-width \"" + clk_name + "\") (signal-width \"" + data_name + "\") (signal-width \"" + valid_name + "\"))");
        assert(result.is_list());
        auto widths = result.to_list();
        assert(widths->size() == 3);
        std::cout << "✓ List of signal widths retrieved\n";
        
        wal::print_section("Error Handling");
        
        // Test loading non-existent file
        try {
            wal->eval_str("(load \"/nonexistent/path/file.vcd\")");
            assert(false && "Should have thrown error");
        } catch (const std::exception&) {
            std::cout << "✓ Loading non-existent file throws error\n";
        }
        
        // Test invalid file format
        try {
            wal->eval_str("(load \"file.xyz\")");
            assert(false && "Should have thrown error for unknown format");
        } catch (const std::exception&) {
            std::cout << "✓ Loading unknown format throws error\n";
        }
        
        wal::print_section("Multi-Trace Support");
        
        // Load same trace again with different ID
        result = wal->eval_str("(load \"" + vcd_file + "\" \"trace2\")");
        assert(result.is_symbol());
        std::cout << "✓ Loaded second trace with custom ID\n";
        
        // Get all traces
        result = wal->eval_str("(get-traces)");
        traces = result.to_list();
        assert(traces->size() == 2);
        std::cout << "✓ (get-traces) now returns 2 traces\n";
        
        wal::print_section("Unload Operations");
        
        // Test unload
        result = wal->eval_str("(unload \"trace2\")");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (unload \"trace2\") succeeded\n";
        
        result = wal->eval_str("(get-traces)");
        traces = result.to_list();
        assert(traces->size() == 1);
        std::cout << "✓ After unload, (get-traces) returns 1 trace\n";
        
        std::cout << "\n════════════════════════════════════════════════════════\n";
        std::cout << "🎉 ALL VCD PARSER TESTS PASSED!\n";
        std::cout << "════════════════════════════════════════════════════════\n\n";
        
        // Cleanup
        std::remove(vcd_file.c_str());
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
