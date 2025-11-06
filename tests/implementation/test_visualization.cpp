#include "wal/core.h"
#include "wal/evaluator.h"
#include "wal/parser.h"
#include <iostream>
#include <cassert>

using namespace wal;

int main() {
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║     VISUALIZATION OPERATORS TESTS                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n";
    
    try {
        auto traces = std::make_shared<TraceContainer>();
        SEval eval(traces);
        Parser parser;
        
        std::cout << "\n=== Testing Plotting Operators ===\n";
        
        // Test 1: ascii-plot basic
        {
            auto ast = parser.parse("(ascii-plot (list 1 2 3 4 5) 5)");
            auto result = eval.eval(ast);
            assert(result.is_string());
            std::string plot = result.to_string();
            assert(plot.find("█") != std::string::npos || plot.find("─") != std::string::npos);
            std::cout << "✓ ascii-plot generates ASCII visualization\n";
        }
        
        // Test 2: gnuplot-script generation
        {
            auto ast = parser.parse("(gnuplot-script (list 1 2 3 4 5) \"Test Signal\")");
            auto result = eval.eval(ast);
            assert(result.is_string());
            std::string script = result.to_string();
            assert(script.find("gnuplot") != std::string::npos);
            assert(script.find("Test Signal") != std::string::npos);
            std::cout << "✓ gnuplot-script generates valid gnuplot script\n";
        }
        
        // Test 3: export-tsv
        {
            auto ast = parser.parse("(export-tsv (list 10 20 30))");
            auto result = eval.eval(ast);
            assert(result.is_string());
            std::string tsv = result.to_string();
            assert(tsv.find("Index") != std::string::npos);
            assert(tsv.find("10") != std::string::npos);
            std::cout << "✓ export-tsv generates tab-separated output\n";
        }
        
        // Test 4: html-plot
        {
            auto ast = parser.parse("(html-plot (list 1 2 3 4 5) \"Signal\")");
            auto result = eval.eval(ast);
            assert(result.is_string());
            std::string html = result.to_string();
            assert(html.find("<svg") != std::string::npos);
            assert(html.find("Signal") != std::string::npos);
            std::cout << "✓ html-plot generates SVG plot\n";
        }
        
        std::cout << "\n════════════════════════════════════════════════════════\n";
        std::cout << "🎉 ALL VISUALIZATION TESTS PASSED! (4 operators)\n";
        std::cout << "════════════════════════════════════════════════════════\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
