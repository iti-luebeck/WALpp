#include <iostream>
#include <cassert>
#include "wal/core.h"

namespace wal {

void print_section(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

} // namespace wal

int main() {
    try {
        auto wal = std::make_shared<wal::Wal>();
        
        std::cout << "╔════════════════════════════════════════════════════════╗\n";
        std::cout << "║        PHASE 8 TRACE FILE SUPPORT OPERATORS TESTS      ║\n";
        std::cout << "╚════════════════════════════════════════════════════════╝\n";
        
        // Test trace query operators (without actual trace files)
        wal::print_section("Trace Query Operators");
        
        // get-traces - Should return empty list initially
        wal::Value result = wal->eval_str("(get-traces)");
        assert(result.is_list());
        assert(result.to_list()->size() == 0);
        std::cout << "✓ (get-traces) returns empty list initially\n";
        
        // signal? - Should return false for non-existent signal
        result = wal->eval_str("(signal? \"nonexistent\")");
        assert(result.is_bool() && !result.to_bool());
        std::cout << "✓ (signal? \"nonexistent\") = false\n";
        
        // signal-names - Should return empty list initially
        result = wal->eval_str("(signal-names)");
        assert(result.is_list());
        assert(result.to_list()->size() == 0);
        std::cout << "✓ (signal-names) returns empty list\n";
        
        wal::print_section("Index Management");
        
        // get-index - Should return empty list when no traces loaded
        result = wal->eval_str("(get-index)");
        assert(result.is_list());
        std::cout << "✓ (get-index) works when no traces loaded\n";
        
        wal::print_section("Trace Stepping (Stub)");
        
        // step - Should work even without traces (stub)
        result = wal->eval_str("(step 1)");
        assert(result.is_int() && result.to_int() == 1);
        std::cout << "✓ (step 1) = 1 (stub mode)\n";
        
        result = wal->eval_str("(step 5)");
        assert(result.is_int() && result.to_int() == 5);
        std::cout << "✓ (step 5) = 5 (stub mode)\n";
        
        result = wal->eval_str("(step -3)");
        assert(result.is_int() && result.to_int() == -3);
        std::cout << "✓ (step -3) = -3 (negative stepping)\n";
        
        wal::print_section("Operator Integration");
        
        // Test that operators integrate well with existing operators
        result = wal->eval_str("(+ 1 (length (get-traces)))");
        assert(result.is_int() && result.to_int() == 1);
        std::cout << "✓ (+ 1 (length (get-traces))) integrates correctly\n";
        
        result = wal->eval_str("(if (signal? \"test\") 1 0)");
        assert(result.is_int() && result.to_int() == 0);
        std::cout << "✓ (if (signal? \"test\") 1 0) = 0\n";
        
        wal::print_section("Complex Expressions");
        
        // Test composition
        result = wal->eval_str("(list (step 1) (step 2) (step 3))");
        assert(result.is_list());
        auto list = result.to_list();
        assert(list->size() == 3);
        assert((*list)[0].to_int() == 1);
        assert((*list)[1].to_int() == 2);
        assert((*list)[2].to_int() == 3);
        std::cout << "✓ (list (step 1) (step 2) (step 3)) creates list\n";
        
        // Test with conditionals
        result = wal->eval_str("(if (>= (step 5) 3) \"large-step\" \"small-step\")");
        assert(result.is_string() && result.to_string() == "large-step");
        std::cout << "✓ Conditional with step operator works\n";
        
        // Test set-index (stub)
        result = wal->eval_str("(set-index 42)");
        assert(result.is_int() && result.to_int() == 42);
        std::cout << "✓ (set-index 42) = 42\n";
        
        result = wal->eval_str("(set-index 0)");
        assert(result.is_int() && result.to_int() == 0);
        std::cout << "✓ (set-index 0) = 0 (reset index)\n";
        
        wal::print_section("Error Handling");
        
        // Test error cases
        try {
            wal->eval_str("(signal?)");
            assert(false && "Should have thrown error for no arguments");
        } catch (const std::exception&) {
            std::cout << "✓ (signal?) with no args throws error\n";
        }
        
        try {
            wal->eval_str("(step \"not-a-number\")");
            assert(false && "Should have thrown error for non-integer step");
        } catch (const std::exception&) {
            std::cout << "✓ (step \"not-a-number\") throws error\n";
        }
        
        try {
            wal->eval_str("(get-signal)");
            assert(false && "Should have thrown error for no arguments");
        } catch (const std::exception&) {
            std::cout << "✓ (get-signal) with no args throws error\n";
        }
        
        try {
            wal->eval_str("(signal-width)");
            assert(false && "Should have thrown error for no arguments");
        } catch (const std::exception&) {
            std::cout << "✓ (signal-width) with no args throws error\n";
        }
        
        wal::print_section("Operator Composition");
        
        // Test filtering empty traces
        result = wal->eval_str("(if (= (length (get-traces)) 0) \"no-traces\" \"has-traces\")");
        assert(result.is_string() && result.to_string() == "no-traces");
        std::cout << "✓ Empty trace check works\n";
        
        // Test multiple operators in sequence
        result = wal->eval_str("(begin (step 1) (step 2) (get-traces))");
        assert(result.is_list());
        std::cout << "✓ Multiple operators in sequence work\n";
        
        wal::print_section("Lambda Integration");
        
        // Test with lambda
        wal->eval_str("(define make-step (lambda (n) (step n)))");
        result = wal->eval_str("(make-step 7)");
        assert(result.is_int() && result.to_int() == 7);
        std::cout << "✓ Lambda with step operator works\n";
        
        // Test with map
        result = wal->eval_str("(map (lambda (x) (step x)) (list 1 2 3))");
        assert(result.is_list());
        list = result.to_list();
        assert(list->size() == 3);
        std::cout << "✓ map with step operator works\n";
        
        std::cout << "\n════════════════════════════════════════════════════════\n";
        std::cout << "🎉 ALL PHASE 8 TESTS PASSED! (10 operators tested)\n";
        std::cout << "════════════════════════════════════════════════════════\n\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
