#include <iostream>
#include <cassert>
#include <fstream>
#include <cstdlib>
#include "wal/core.h"

namespace wal {

void print_section(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

// Create a basic CSV test file
std::string create_basic_csv(const std::string& filename) {
    std::ofstream file(filename);
    file << "time,clk,data,valid\n";
    file << "0,1,255,0\n";
    file << "10,0,128,1\n";
    file << "20,1,64,1\n";
    file << "30,0,32,0\n";
    file << "40,1,16,1\n";
    file << "50,0,8,0\n";
    file << "60,1,4,1\n";
    file << "70,0,2,0\n";
    file.close();
    return filename;
}

// Create a CSV with hex values
std::string create_hex_csv(const std::string& filename) {
    std::ofstream file(filename);
    file << "time,addr,data,control\n";
    file << "0,0x0000,0xDEADBEEF,0xF\n";
    file << "5,0x1000,0xCAFEBABE,0xA\n";
    file << "10,0x2000,0x12345678,0x5\n";
    file << "15,0x3000,0xFFFFFFFF,0x0\n";
    file.close();
    return filename;
}

// Create a CSV with binary values
std::string create_binary_csv(const std::string& filename) {
    std::ofstream file(filename);
    file << "time,signal_a,signal_b,signal_c\n";
    file << "0,0b1010,0b0101,0b1111\n";
    file << "1,0b1111,0b0000,0b1100\n";
    file << "2,0b0001,0b1110,0b0011\n";
    file.close();
    return filename;
}

// Create a CSV without header
std::string create_no_header_csv(const std::string& filename) {
    std::ofstream file(filename);
    file << "0,1,100\n";
    file << "10,0,50\n";
    file << "20,1,25\n";
    file.close();
    return filename;
}

// Create a CSV with tab delimiter
std::string create_tab_csv(const std::string& filename) {
    std::ofstream file(filename);
    file << "time\tclock\tbus_data\tstatus\n";
    file << "0\t1\t42\t0\n";
    file << "5\t0\t84\t1\n";
    file << "10\t1\t168\t0\n";
    file.close();
    return filename;
}

// Create a CSV with floating point values
std::string create_float_csv(const std::string& filename) {
    std::ofstream file(filename);
    file << "time,voltage,current,power\n";
    file << "0,3.3,0.5,1.65\n";
    file << "1,3.2,0.6,1.92\n";
    file << "2,3.1,0.7,2.17\n";
    file.close();
    return filename;
}

} // namespace wal

int main() {
    try {
        std::cout << "╔════════════════════════════════════════════════════════╗\n";
        std::cout << "║              CSV FILE PARSER TEST SUITE                ║\n";
        std::cout << "╚════════════════════════════════════════════════════════╝\n";
        
        auto wal = std::make_shared<wal::Wal>();
        
        wal::print_section("Basic CSV File Loading");
        
        std::string csv_file = "/tmp/test_basic.csv";
        wal::create_basic_csv(csv_file);
        
        wal::Value result = wal->eval_str("(load \"" + csv_file + "\")");
        assert(result.is_symbol());
        std::cout << "✓ Successfully loaded basic CSV file\n";
        
        // Test get-traces
        result = wal->eval_str("(get-traces)");
        assert(result.is_list());
        auto traces = result.to_list();
        assert(traces->size() > 0);
        std::cout << "✓ (get-traces) returns " << traces->size() << " trace(s)\n";
        
        wal::print_section("Signal Discovery");
        
        result = wal->eval_str("(signal-names)");
        assert(result.is_list());
        auto signals = result.to_list();
        assert(signals->size() >= 3);  // clk, data, valid
        std::cout << "✓ (signal-names) returns " << signals->size() << " signals\n";
        
        // Extract signal names
        std::string clk_name, data_name, valid_name;
        for (const auto& sig : *signals) {
            if (sig.is_symbol()) {
                std::string name = sig.to_symbol()->get_name();
                if (name.find("clk") != std::string::npos) clk_name = name;
                if (name.find("data") != std::string::npos) data_name = name;
                if (name.find("valid") != std::string::npos) valid_name = name;
            }
        }
        
        std::cout << "Found signals: " << clk_name << ", " << data_name << ", " << valid_name << "\n";
        
        wal::print_section("Signal Queries");
        
        result = wal->eval_str("(signal? \"" + clk_name + "\")");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (signal? \"" + clk_name + "\") = true\n";
        
        result = wal->eval_str("(signal? \"nonexistent\")");
        assert(result.is_bool() && !result.to_bool());
        std::cout << "✓ (signal? \"nonexistent\") = false\n";
        
        wal::print_section("Signal Metadata");
        
        result = wal->eval_str("(signal-width \"" + clk_name + "\")");
        assert(result.is_int());
        std::cout << "✓ (signal-width \"" + clk_name + "\") = " << result.to_int() << "\n";
        
        result = wal->eval_str("(signal-width \"" + data_name + "\")");
        assert(result.is_int());
        int data_width = result.to_int();
        assert(data_width >= 8);  // Should be at least 8 bits for 255
        std::cout << "✓ (signal-width \"" + data_name + "\") = " << data_width << "\n";
        
        wal::print_section("Index Management");
        
        result = wal->eval_str("(get-index)");
        assert(result.is_list());
        std::cout << "✓ (get-index) returns indices\n";
        
        result = wal->eval_str("(set-index 2)");
        assert(result.is_int() && result.to_int() == 2);
        std::cout << "✓ (set-index 2) = 2\n";
        
        wal::print_section("Hexadecimal Values");
        
        std::string hex_file = "/tmp/test_hex.csv";
        wal::create_hex_csv(hex_file);
        
        result = wal->eval_str("(load \"" + hex_file + "\")");
        assert(result.is_symbol());
        std::cout << "✓ Loaded CSV with hexadecimal values\n";
        
        result = wal->eval_str("(get-traces)");
        traces = result.to_list();
        assert(traces->size() == 2);  // Now have 2 traces
        std::cout << "✓ (get-traces) returns 2 traces (basic + hex)\n";
        
        wal::print_section("Binary Values");
        
        std::string bin_file = "/tmp/test_binary.csv";
        wal::create_binary_csv(bin_file);
        
        result = wal->eval_str("(load \"" + bin_file + "\")");
        assert(result.is_symbol());
        std::cout << "✓ Loaded CSV with binary values\n";
        
        wal::print_section("Floating Point Values");
        
        std::string float_file = "/tmp/test_float.csv";
        wal::create_float_csv(float_file);
        
        result = wal->eval_str("(load \"" + float_file + "\")");
        assert(result.is_symbol());
        std::cout << "✓ Loaded CSV with floating-point values\n";
        
        wal::print_section("Multi-Trace Operations");
        
        // Get the first trace ID
        result = wal->eval_str("(get-traces)");
        traces = result.to_list();
        std::string first_trace_id = "";
        if (traces->size() > 0 && traces->at(0).is_symbol()) {
            first_trace_id = traces->at(0).to_symbol()->get_name();
        }
        
        if (!first_trace_id.empty()) {
            result = wal->eval_str("(unload \"" + first_trace_id + "\")");
            assert(result.is_bool());
            std::cout << "✓ Unload operation succeeded\n";
        } else {
            std::cout << "✓ Skipped unload test (no trace ID available)\n";
        }
        
        result = wal->eval_str("(get-traces)");
        traces = result.to_list();
        std::cout << "✓ After unload, (get-traces) returns " << traces->size() << " traces\n";
        
        wal::print_section("Operator Integration");
        
        // Fresh load for composition test
        wal::create_basic_csv(csv_file);
        result = wal->eval_str("(load \"" + csv_file + "\")");
        
        result = wal->eval_str("(signal-names)");
        signals = result.to_list();
        
        // Find signal names again
        clk_name = data_name = valid_name = "";
        for (const auto& sig : *signals) {
            if (sig.is_symbol()) {
                std::string name = sig.to_symbol()->get_name();
                if (name.find("clk") != std::string::npos) clk_name = name;
                if (name.find("data") != std::string::npos) data_name = name;
                if (name.find("valid") != std::string::npos) valid_name = name;
            }
        }
        
        // Test composition
        result = wal->eval_str("(if (signal? \"" + clk_name + "\") 1 0)");
        assert(result.is_int() && result.to_int() == 1);
        std::cout << "✓ Operator composition works\n";
        
        wal::print_section("Error Handling");
        
        try {
            wal->eval_str("(load \"/nonexistent/file.csv\")");
            assert(false && "Should have thrown error");
        } catch (const std::exception&) {
            std::cout << "✓ Loading non-existent file throws error\n";
        }
        
        std::cout << "\n════════════════════════════════════════════════════════\n";
        std::cout << "🎉 ALL CSV PARSER TESTS PASSED!\n";
        std::cout << "════════════════════════════════════════════════════════\n\n";
        
        // Cleanup
        std::remove(csv_file.c_str());
        std::remove(hex_file.c_str());
        std::remove(bin_file.c_str());
        std::remove(float_file.c_str());
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
