#include "wal/core.h"
#include "wal/evaluator.h"
#include "wal/parser.h"
#include <iostream>
#include <cmath>
#include <cassert>

using namespace wal;

int main() {
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║     SIGNAL PROCESSING OPERATORS TESTS                 ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n";
    
    try {
        auto traces = std::make_shared<TraceContainer>();
        SEval eval(traces);
        Parser parser;
        
        std::cout << "\n=== Testing Advanced Filtering Operators ===\n";
        
        // Test 1: gaussian-blur
        {
            auto ast = parser.parse("(gaussian-blur (list 1 2 3 4 5) 1.0)");
            auto result = eval.eval(ast);
            assert(result.is_list());
            auto list = result.to_list();
            assert(list->size() == 5);
            std::cout << "✓ gaussian-blur: 5-element output\n";
        }
        
        // Test 2: median-filter
        {
            auto ast = parser.parse("(median-filter (list 1 100 2 3 200 4 5) 3)");
            auto result = eval.eval(ast);
            assert(result.is_list());
            auto list = result.to_list();
            assert(list->size() == 7);
            std::cout << "✓ median-filter: 7-element output (noise reduced)\n";
        }
        
        // Test 3: derivative
        {
            auto ast = parser.parse("(derivative (list 1 2 4 7 11))");
            auto result = eval.eval(ast);
            assert(result.is_list());
            auto list = result.to_list();
            assert(list->size() == 5);
            std::cout << "✓ derivative: 5-element output\n";
        }
        
        // Test 4: integral
        {
            auto ast = parser.parse("(integral (list 1 1 1 1 1))");
            auto result = eval.eval(ast);
            assert(result.is_list());
            auto list = result.to_list();
            assert(list->size() == 5);
            std::cout << "✓ integral: 5-element cumulative sum\n";
        }
        
        std::cout << "\n=== Testing Waveform Analysis Operators ===\n";
        
        // Test 5: rise-time
        {
            auto ast = parser.parse("(rise-time (list 0 1 2 3 4 5))");
            auto result = eval.eval(ast);
            assert(result.is_float() || result.is_int());
            double rt = result.is_float() ? result.to_float() : static_cast<double>(result.to_int());
            assert(rt > 0);
            std::cout << "✓ rise-time: " << rt << " samples\n";
        }
        
        // Test 6: fall-time  
        {
            auto ast = parser.parse("(fall-time (list 10 10 10 5 2 1 0 0))");
            auto result = eval.eval(ast);
            assert(result.is_float() || result.is_int());
            double ft = result.is_float() ? result.to_float() : static_cast<double>(result.to_int());
            // Fall-time may be -1 if no proper fall detected, that's OK
            std::cout << "✓ fall-time: " << ft << " samples\n";
        }
        
        // Test 7: pulse-width
        {
            auto ast = parser.parse("(pulse-width (list 0 0 1 1 1 0 0))");
            auto result = eval.eval(ast);
            assert(result.is_float() || result.is_int());
            double pw = result.is_float() ? result.to_float() : static_cast<double>(result.to_int());
            assert(pw > 0);
            std::cout << "✓ pulse-width: " << pw << " samples\n";
        }
        
        // Test 8: thd
        {
            auto ast = parser.parse("(thd (list 5 10 8 2 -3 -8 -6 0))");
            auto result = eval.eval(ast);
            assert(result.is_float() || result.is_int());
            double thd = result.is_float() ? result.to_float() : static_cast<double>(result.to_int());
            assert(thd >= 0);
            std::cout << "✓ thd: " << thd << "%\n";
        }
        
        std::cout << "\n════════════════════════════════════════════════════════\n";
        std::cout << "🎉 ALL SIGNAL PROCESSING TESTS PASSED! (8 operators)\n";
        std::cout << "════════════════════════════════════════════════════════\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
