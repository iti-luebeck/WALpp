// test_vcd_writer.cpp - Unit tests for VCDWriter
// Phase 3 Task 1 - Trace File Writing API

#include <iostream>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include "wal/writers/vcd_writer.h"

using namespace wal::writers;

void print_section(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

std::string get_temp_file(const std::string& name) {
    std::string tmp = "/tmp/walpp_test_" + name;
    return tmp;
}

void cleanup_file(const std::string& path) {
    std::remove(path.c_str());
}

std::string read_file_content(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
}

void test_basic_construction() {
    print_section("Test: Basic construction and configuration");
    
    std::string test_file = get_temp_file("test_basic.vcd");
    
    VCDWriter writer(test_file);
    writer.set_format_version(2, 0);
    writer.set_time_scale("ns");
    writer.set_time_base(1);
    
    assert(writer.signal_count() == 0);
    writer.finalize();
    
    // Check file was created
    std::ifstream check(test_file);
    assert(check.good());
    check.close();
    
    cleanup_file(test_file);
    std::cout << "✓ Basic construction passed\n";
}

void test_add_flat_signals() {
    print_section("Test: Add flat signals");
    
    std::string test_file = get_temp_file("test_flat.vcd");
    
    VCDWriter writer(test_file);
    writer.set_time_scale("ps");
    
    writer.add_signal("clk", 1, "wire");
    writer.add_signal("data", 8, "reg");
    writer.add_signal("valid", 1, "wire");
    
    assert(writer.signal_count() == 3);
    assert(writer.has_signal("clk"));
    assert(writer.has_signal("data"));
    assert(writer.signal_width("clk") == 1);
    assert(writer.signal_width("data") == 8);
    
    writer.finalize();
    cleanup_file(test_file);
    std::cout << "✓ Add flat signals passed\n";
}

void test_add_hierarchical_signals() {
    print_section("Test: Add hierarchical signals");
    
    std::string test_file = get_temp_file("test_hier.vcd");
    
    VCDWriter writer(test_file);
    
    writer.add_hierarchical_signal("top.cpu", "clk", 1, "wire");
    writer.add_hierarchical_signal("top.cpu", "pc", 32, "reg");
    writer.add_hierarchical_signal("top.memory", "data", 8, "wire");
    
    assert(writer.signal_count() == 3);
    assert(writer.has_signal("top.cpu.clk"));
    assert(writer.has_signal("top.memory.data"));
    assert(writer.signal_width("top.cpu.pc") == 32);
    
    writer.finalize();
    cleanup_file(test_file);
    std::cout << "✓ Add hierarchical signals passed\n";
}

void test_reject_duplicate_signals() {
    print_section("Test: Reject duplicate signals");
    
    std::string test_file = get_temp_file("test_dup.vcd");
    
    VCDWriter writer(test_file);
    writer.add_signal("clk", 1);
    
    bool caught = false;
    try {
        writer.add_signal("clk", 1);
    } catch (const std::runtime_error&) {
        caught = true;
    }
    assert(caught);
    
    writer.finalize();
    cleanup_file(test_file);
    std::cout << "✓ Reject duplicate signals passed\n";
}

void test_invalid_time_scale() {
    print_section("Test: Reject invalid time scale");
    
    std::string test_file = get_temp_file("test_invalid_ts.vcd");
    
    VCDWriter writer(test_file);
    
    bool caught = false;
    try {
        writer.set_time_scale("invalid_scale");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    assert(caught);
    
    writer.finalize();
    cleanup_file(test_file);
    std::cout << "✓ Invalid time scale rejection passed\n";
}

void test_write_single_signal_value() {
    print_section("Test: Write single signal value");
    
    std::string test_file = get_temp_file("test_single_val.vcd");
    
    VCDWriter writer(test_file);
    writer.add_signal("clk", 1);
    writer.set_time_scale("ns");
    
    writer.write_value("clk", 0, "1");
    writer.write_value("clk", 100, "0");
    writer.write_value("clk", 200, "1");
    
    writer.finalize();
    
    std::string content = read_file_content(test_file);
    assert(content.find("#0") != std::string::npos);
    assert(content.find("#100") != std::string::npos);
    assert(content.find("#200") != std::string::npos);
    
    cleanup_file(test_file);
    std::cout << "✓ Write single signal value passed\n";
}

void test_write_multiple_signal_values() {
    print_section("Test: Write multiple signal values");
    
    std::string test_file = get_temp_file("test_multi_val.vcd");
    
    VCDWriter writer(test_file);
    writer.add_signal("a", 1);
    writer.add_signal("b", 8);
    writer.set_time_scale("ns");
    
    writer.write_value("a", 0, "1");
    writer.write_value("b", 0, "2a");
    writer.write_value("a", 100, "0");
    writer.write_value("b", 100, "63");
    
    writer.finalize();
    
    std::string content = read_file_content(test_file);
    assert(content.find("#0") != std::string::npos);
    assert(content.find("#100") != std::string::npos);
    
    cleanup_file(test_file);
    std::cout << "✓ Write multiple signal values passed\n";
}

void test_transactions() {
    print_section("Test: Transactions");
    
    std::string test_file = get_temp_file("test_trans.vcd");
    
    VCDWriter writer(test_file);
    writer.add_signal("addr", 32);
    writer.add_signal("data", 32);
    writer.set_time_scale("ns");
    
    writer.begin_transaction();
    writer.write_value("addr", 0, "deadbeef");
    writer.write_value("data", 0, "deadbeef");
    writer.commit_transaction();
    
    writer.finalize();
    
    std::string content = read_file_content(test_file);
    assert(!content.empty());
    
    cleanup_file(test_file);
    std::cout << "✓ Transactions passed\n";
}

void test_get_all_signals() {
    print_section("Test: Get all signals");
    
    std::string test_file = get_temp_file("test_all_signals.vcd");
    
    VCDWriter writer(test_file);
    writer.add_signal("sig1", 1);
    writer.add_signal("sig2", 8);
    writer.add_signal("sig3", 16);
    
    std::vector<std::string> signals = writer.get_all_signals();
    assert(signals.size() == 3);
    
    writer.finalize();
    cleanup_file(test_file);
    std::cout << "✓ Get all signals passed\n";
}

void test_signal_statistics() {
    print_section("Test: Signal statistics");
    
    std::string test_file = get_temp_file("test_stats.vcd");
    
    VCDWriter writer(test_file);
    writer.add_signal("clk", 1);
    writer.add_signal("data", 8);
    writer.add_signal("ctrl", 4);
    
    assert(writer.signal_count() == 3);
    
    writer.finalize();
    cleanup_file(test_file);
    std::cout << "✓ Signal statistics passed\n";
}

void test_state_protection() {
    print_section("Test: State protection");
    
    std::string test_file = get_temp_file("test_state.vcd");
    
    VCDWriter writer(test_file);
    writer.set_time_scale("ns");
    writer.add_signal("clk", 1);
    
    writer.finalize();
    
    // Should not be able to add signal after finalize
    bool caught = false;
    try {
        writer.add_signal("another", 1);
    } catch (const std::runtime_error&) {
        caught = true;
    }
    assert(caught);
    
    cleanup_file(test_file);
    std::cout << "✓ State protection passed\n";
}

void test_vcd_format_compliance() {
    print_section("Test: VCD format compliance");
    
    std::string test_file = get_temp_file("test_format.vcd");
    
    VCDWriter writer(test_file);
    writer.add_signal("clk", 1);
    writer.set_time_scale("ns");
    writer.write_value("clk", 0, "1");
    writer.write_value("clk", 100, "0");
    writer.finalize();
    
    std::string content = read_file_content(test_file);
    
    // Check for required VCD sections
    assert(content.find("$timescale") != std::string::npos);
    assert(content.find("$scope") != std::string::npos);
    assert(content.find("$var") != std::string::npos);
    assert(content.find("$upscope") != std::string::npos);
    assert(content.find("$enddefinitions") != std::string::npos);
    assert(content.find("$dumpvars") != std::string::npos);
    assert(content.find("#0") != std::string::npos);
    assert(content.find("#100") != std::string::npos);
    
    cleanup_file(test_file);
    std::cout << "✓ VCD format compliance passed\n";
}

void test_wide_signals() {
    print_section("Test: Wide signals");
    
    std::string test_file = get_temp_file("test_wide.vcd");
    
    VCDWriter writer(test_file);
    writer.add_signal("addr", 64);
    writer.add_signal("data", 128);
    writer.set_time_scale("ns");
    
    writer.write_value("addr", 0, "ffffffffffffffff");
    writer.write_value("data", 0, "0");
    
    writer.finalize();
    
    std::string content = read_file_content(test_file);
    assert(!content.empty());
    
    cleanup_file(test_file);
    std::cout << "✓ Wide signals passed\n";
}

void test_many_signals() {
    print_section("Test: Many signals");
    
    std::string test_file = get_temp_file("test_many.vcd");
    
    VCDWriter writer(test_file);
    
    // Add 100 signals
    for (int i = 0; i < 100; ++i) {
        std::string name = "sig_" + std::to_string(i);
        writer.add_signal(name, 1);
    }
    
    assert(writer.signal_count() == 100);
    
    writer.finalize();
    
    std::string content = read_file_content(test_file);
    assert(!content.empty());
    
    cleanup_file(test_file);
    std::cout << "✓ Many signals passed\n";
}

int main() {
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║              VCD WRITER UNIT TESTS                    ║\n";
    std::cout << "║           Phase 3 Task 1 - Trace Writers             ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n";
    
    try {
        test_basic_construction();
        test_add_flat_signals();
        test_add_hierarchical_signals();
        test_reject_duplicate_signals();
        test_invalid_time_scale();
        test_write_single_signal_value();
        test_write_multiple_signal_values();
        test_transactions();
        test_get_all_signals();
        test_signal_statistics();
        test_state_protection();
        test_vcd_format_compliance();
        test_wide_signals();
        test_many_signals();
        
        std::cout << "\n╔════════════════════════════════════════════════════════╗\n";
        std::cout << "║         ✓ ALL VCD WRITER TESTS PASSED (13/13)         ║\n";
        std::cout << "╚════════════════════════════════════════════════════════╝\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
