#include "wal/core.h"
#include "wal/evaluator.h"
#include "wal/parser.h"
#include <iostream>
#include <cmath>
#include <cassert>

using namespace wal;

int main() {
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║    STATISTICAL ANALYSIS OPERATORS TESTS               ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n";
    
    try {
        auto traces = std::make_shared<TraceContainer>();
        SEval eval(traces);
        Parser parser;
        
        std::cout << "\n=== Testing Statistical Operators ===\n";
        
        // Test 1: variance
        {
            auto ast = parser.parse("(variance (list 1 2 3 4 5))");
            auto result = eval.eval(ast);
            assert(result.is_float() || result.is_int());
            double var = result.is_float() ? result.to_float() : static_cast<double>(result.to_int());
            assert(var > 0);
            double expected = 2.0;
            assert(std::abs(var - expected) < 0.01);
            std::cout << "✓ variance: " << var << " (expected ~2.0)\n";
        }
        
        // Test 2: stddev
        {
            auto ast = parser.parse("(stddev (list 2 4 6 8))");
            auto result = eval.eval(ast);
            assert(result.is_float() || result.is_int());
            double sd = result.is_float() ? result.to_float() : static_cast<double>(result.to_int());
            assert(sd > 0);
            std::cout << "✓ stddev: " << sd << "\n";
        }
        
        // Test 3: median
        {
            auto ast = parser.parse("(median (list 1 2 3 4 5))");
            auto result = eval.eval(ast);
            assert(result.is_float() || result.is_int());
            double med = result.is_float() ? result.to_float() : static_cast<double>(result.to_int());
            assert(std::abs(med - 3.0) < 0.01);
            std::cout << "✓ median: " << med << " (expected 3.0)\n";
        }
        
        // Test 4: median with even count
        {
            auto ast = parser.parse("(median (list 1 2 3 4))");
            auto result = eval.eval(ast);
            assert(result.is_float() || result.is_int());
            double med = result.is_float() ? result.to_float() : static_cast<double>(result.to_int());
            assert(std::abs(med - 2.5) < 0.01);
            std::cout << "✓ median (even): " << med << " (expected 2.5)\n";
        }
        
        // Test 5: quartiles
        {
            auto ast = parser.parse("(quartiles (list 1 2 3 4 5 6 7 8))");
            auto result = eval.eval(ast);
            assert(result.is_list());
            auto list = result.to_list();
            assert(list->size() == 3);
            std::cout << "✓ quartiles: 3-element output\n";
        }
        
        // Test 6: skewness
        {
            auto ast = parser.parse("(skewness (list 1 2 3 4 5))");
            auto result = eval.eval(ast);
            assert(result.is_float() || result.is_int());
            double sk = result.is_float() ? result.to_float() : static_cast<double>(result.to_int());
            assert(std::abs(sk) < 0.1);
            std::cout << "✓ skewness: " << sk << " (symmetric ~0)\n";
        }
        
        // Test 7: kurtosis
        {
            auto ast = parser.parse("(kurtosis (list 1 2 3 4 5))");
            auto result = eval.eval(ast);
            assert(result.is_float() || result.is_int());
            double kurt = result.is_float() ? result.to_float() : static_cast<double>(result.to_int());
            std::cout << "✓ kurtosis: " << kurt << "\n";
        }
        
        // Test 8: histogram
        {
            auto ast = parser.parse("(histogram (list 1 2 3 4 5) 5)");
            auto result = eval.eval(ast);
            assert(result.is_list());
            auto list = result.to_list();
            assert(list->size() == 5);
            std::cout << "✓ histogram: 5-bin output\n";
        }
        
        std::cout << "\n════════════════════════════════════════════════════════\n";
        std::cout << "🎉 ALL STATISTICAL ANALYSIS TESTS PASSED! (8 operators)\n";
        std::cout << "════════════════════════════════════════════════════════\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
