#include "wal/core.h"
#include "wal/evaluator.h"
#include "wal/parser.h"
#include <iostream>
#include <cmath>
#include <cassert>

using namespace wal;

int main() {
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║     SPECTRUM ANALYSIS OPERATORS TESTS                 ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n";
    
    try {
        auto traces = std::make_shared<TraceContainer>();
        SEval eval(traces);
        Parser parser;
        
        std::cout << "\n=== Testing Spectrum Analysis Operators ===\n";
        
        // Test 1: frequency-spectrum on simple sine wave
        {
            auto ast = parser.parse("(frequency-spectrum (list 1 2 3 4 5 4 3 2))");
            auto result = eval.eval(ast);
            assert(result.is_list());
            std::cout << "✓ frequency-spectrum produces list output\n";
        }
        
        // Test 2: magnitude-spectrum normalizes to 0-1
        {
            auto ast = parser.parse("(magnitude-spectrum (list 0 1 2 3 4 3 2 1))");
            auto result = eval.eval(ast);
            assert(result.is_list());
            auto list = result.to_list();
            for (const auto& v : *list) {
                if (v.is_float()) {
                    double d = v.to_float();
                    assert(d >= 0 && d <= 1.0);
                }
            }
            std::cout << "✓ magnitude-spectrum normalized to [0,1]\n";
        }
        
        // Test 3: phase-spectrum returns values in radians
        {
            auto ast = parser.parse("(phase-spectrum (list 1 0 -1 0 1 0 -1 0))");
            auto result = eval.eval(ast);
            assert(result.is_list());
            std::cout << "✓ phase-spectrum produces phase values\n";
        }
        
        // Test 4: peak-frequency identifies dominant frequency
        {
            auto ast = parser.parse("(peak-frequency (list 0 1 0 -1 0 1 0 -1) 8)");
            auto result = eval.eval(ast);
            assert(result.is_float() || result.is_int());
            double freq = result.is_float() ? result.to_float() : static_cast<double>(result.to_int());
            assert(freq > 0);
            std::cout << "✓ peak-frequency: " << freq << " Hz\n";
        }
        
        std::cout << "\n════════════════════════════════════════════════════════\n";
        std::cout << "🎉 ALL SPECTRUM ANALYSIS TESTS PASSED! (4 operators)\n";
        std::cout << "════════════════════════════════════════════════════════\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
