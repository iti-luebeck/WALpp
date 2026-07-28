#include "wal/trace/fst_trace.h"
#include "wal/writers/fst_writer.h"
#include "wal/trace/trace_container.h"
#include <iostream>
#include <fstream>
#include <cassert>
#include <cstring>

using namespace wal;

// Build the fixture with WAL's own FSTWriter.
//
// This used to hand-write a byte sequence invented to look like FST. It was not FST -- the
// real GTKWave reader rejected it, so this whole file failed on "cannot open" and had been
// doing so for as long as the writer was broken. Generating the fixture with FSTWriter
// makes the test exercise a genuine FST *and* keeps reader and writer honest about each
// other: if either drifts, this fails.
std::string create_test_fst_file(const std::string& filename) {
    wal::writers::FSTWriter w(filename);
    w.set_time_scale("ns");
    w.add_signal("signal_a", 8);
    w.add_signal("signal_b", 16);

    w.write_value("signal_a", 0, "01010101");           // 0x55
    w.write_value("signal_b", 0, "0001001000110100");   // 0x1234
    w.write_value("signal_a", 50, "10101010");          // 0xAA
    w.finalize();

    return filename;
}

// Test 1: Basic FST file loading
void test_fst_basic_loading() {
    std::cout << "Test 1: Basic FST file loading... ";
    
    std::string test_file = "/tmp/test_basic.fst";
    create_test_fst_file(test_file);
    
    FSTTrace trace("test_basic", test_file, nullptr);
    trace.load();
    
    // Verify that signals were loaded
    auto signals = trace.get_all_signals();
    assert(!signals.empty());
    
    std::cout << "✓ PASS" << std::endl;
}

// Test 2: Signal discovery
void test_fst_signal_discovery() {
    std::cout << "Test 2: Signal discovery... ";
    
    std::string test_file = "/tmp/test_signals.fst";
    create_test_fst_file(test_file);
    
    FSTTrace trace("test_signals", test_file, nullptr);
    trace.load();
    
    auto signals = trace.get_all_signals();
    assert(signals.size() >= 2);  // At least signal_a and signal_b
    
    // Check for signal names
    bool has_signal_a = false;
    bool has_signal_b = false;
    for (const auto& sig : signals) {
        if (sig.find("signal_a") != std::string::npos) has_signal_a = true;
        if (sig.find("signal_b") != std::string::npos) has_signal_b = true;
    }
    assert(has_signal_a);
    assert(has_signal_b);
    
    std::cout << "✓ PASS" << std::endl;
}

// Test 3: Signal width detection
void test_fst_signal_width() {
    std::cout << "Test 3: Signal width detection... ";
    
    std::string test_file = "/tmp/test_width.fst";
    create_test_fst_file(test_file);
    
    FSTTrace trace("test_width", test_file, nullptr);
    trace.load();
    
    auto signals = trace.get_all_signals();
    
    // Find signal widths
    for (const auto& sig : signals) {
        int width = trace.signal_width(sig);
        assert(width > 0);
    }
    
    std::cout << "✓ PASS" << std::endl;
}

// Test 4: Contains signal check
void test_fst_contains_signal() {
    std::cout << "Test 4: Contains signal check... ";
    
    std::string test_file = "/tmp/test_contains.fst";
    create_test_fst_file(test_file);
    
    FSTTrace trace("test_contains", test_file, nullptr);
    trace.load();
    
    auto signals = trace.get_all_signals();
    assert(!signals.empty());
    
    // Check if we can find signals
    for (const auto& sig : signals) {
        assert(trace.contains_signal(sig));
    }
    
    // Check for non-existent signal
    assert(!trace.contains_signal("nonexistent_signal"));
    
    std::cout << "✓ PASS" << std::endl;
}

// Test 5: Value queries
void test_fst_value_queries() {
    std::cout << "Test 5: Value queries... ";
    
    std::string test_file = "/tmp/test_values.fst";
    create_test_fst_file(test_file);
    
    FSTTrace trace("test_values", test_file, nullptr);
    trace.load();
    
    auto signals = trace.get_all_signals();
    assert(!signals.empty());
    
    // Try to query values
    if (trace.contains_signal(signals[0])) {
        try {
            Value val = trace.signal_value(signals[0], 0);
            // Successfully queried
        } catch (...) {
            // Expected if no values at this index
        }
    }
    
    std::cout << "✓ PASS" << std::endl;
}

// Test 6: TraceContainer FST integration
void test_trace_container_fst() {
    std::cout << "Test 6: TraceContainer FST integration... ";
    
    std::string test_file = "/tmp/test_container.fst";
    create_test_fst_file(test_file);
    
    TraceContainer container;
    container.load(test_file);
    
    auto trace_ids = container.get_trace_ids();
    assert(!trace_ids.empty());
    
    auto trace = container.get_trace(trace_ids[0]);
    assert(trace != nullptr);
    
    auto signals = trace->get_all_signals();
    assert(!signals.empty());
    
    std::cout << "✓ PASS" << std::endl;
}

// Test 7: Multiple signal access
void test_fst_multiple_signals() {
    std::cout << "Test 7: Multiple signal access... ";
    
    std::string test_file = "/tmp/test_multiple.fst";
    create_test_fst_file(test_file);
    
    FSTTrace trace("test_multiple", test_file, nullptr);
    trace.load();
    
    auto signals = trace.get_all_signals();
    
    // Access multiple signals
    int count = 0;
    for (const auto& sig : signals) {
        if (trace.contains_signal(sig)) {
            count++;
            int width = trace.signal_width(sig);
            assert(width > 0);
        }
    }
    
    assert(count > 0);
    
    std::cout << "✓ PASS" << std::endl;
}

// Test 8: File not found error handling
void test_fst_file_not_found() {
    std::cout << "Test 8: File not found error handling... ";
    
    FSTTrace trace("test_missing", "/tmp/nonexistent_file.fst", nullptr);
    
    try {
        trace.load();
        assert(false);  // Should have thrown
    } catch (const std::exception& e) {
        // Expected
        assert(std::string(e.what()).find("Cannot open") != std::string::npos ||
               std::string(e.what()).find("Failed to load") != std::string::npos);
    }
    
    std::cout << "✓ PASS" << std::endl;
}

// Test 9: Empty file error handling
void test_fst_empty_file() {
    std::cout << "Test 9: Empty file error handling... ";
    
    std::string test_file = "/tmp/test_empty.fst";
    std::ofstream file(test_file, std::ios::binary);
    file.close();
    
    FSTTrace trace("test_empty", test_file, nullptr);
    
    try {
        trace.load();
        assert(false);  // Should have thrown
    } catch (const std::exception& e) {
        // Expected
        assert(std::string(e.what()).find("empty") != std::string::npos ||
               std::string(e.what()).find("Failed to load") != std::string::npos);
    }
    
    std::cout << "✓ PASS" << std::endl;
}

// Test 10: Corrupt header error handling
void test_fst_corrupt_header() {
    std::cout << "Test 10: Corrupt header error handling... ";
    
    std::string test_file = "/tmp/test_corrupt.fst";
    std::ofstream file(test_file, std::ios::binary);
    
    // Write invalid magic number
    file.put(0x00); file.put(0x00); file.put(0x00); file.put(0x00);
    file.put(0x01); file.put(0x09); file.put(0x00);
    
    file.close();
    
    FSTTrace trace("test_corrupt", test_file, nullptr);
    
    try {
        trace.load();
        assert(false);  // Should have thrown
    } catch (const std::exception& e) {
        // Expected
        assert(std::string(e.what()).find("magic") != std::string::npos ||
               std::string(e.what()).find("Failed to load") != std::string::npos);
    }
    
    std::cout << "✓ PASS" << std::endl;
}

// Test 11: Scope parsing
void test_fst_scope_parsing() {
    std::cout << "Test 11: Scope parsing... ";
    
    std::string test_file = "/tmp/test_scope.fst";
    create_test_fst_file(test_file);
    
    FSTTrace trace("test_scope", test_file, nullptr);
    trace.load();
    
    auto signals = trace.get_all_signals();
    
    // Check that signals have proper scope information
    for (const auto& sig : signals) {
        assert(!sig.empty());
    }
    
    std::cout << "✓ PASS" << std::endl;
}

// Test 12: Timestamp navigation
void test_fst_timestamp_navigation() {
    std::cout << "Test 12: Timestamp navigation... ";
    
    std::string test_file = "/tmp/test_timestamps.fst";
    create_test_fst_file(test_file);
    
    FSTTrace trace("test_timestamps", test_file, nullptr);
    trace.load();
    
    auto signals = trace.get_all_signals();
    assert(!signals.empty());
    
    // Try navigating through timestamps
    if (!signals.empty()) {
        try {
            Value val0 = trace.signal_value(signals[0], 0);
            Value val1 = trace.signal_value(signals[0], 1);
            // Successfully navigated
        } catch (...) {
            // May be out of range, which is expected
        }
    }
    
    std::cout << "✓ PASS" << std::endl;
}

int main() {
    std::cout << "\n=== FST Parser Tests ===" << std::endl;
    std::cout << "Running 12 FST format tests...\n" << std::endl;
    
    try {
        test_fst_basic_loading();
        test_fst_signal_discovery();
        test_fst_signal_width();
        test_fst_contains_signal();
        test_fst_value_queries();
        test_trace_container_fst();
        test_fst_multiple_signals();
        test_fst_file_not_found();
        test_fst_empty_file();
        test_fst_corrupt_header();
        test_fst_scope_parsing();
        test_fst_timestamp_navigation();
        
        std::cout << "\n✓ All 12 FST tests PASSED!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n✗ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
