#include "wal/core.h"
#include "wal/evaluator.h"
#include "wal/parser.h"
#include <iostream>
#include <cassert>
#include <sstream>

using namespace wal;

int main() {
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║       DATA EXPORT OPERATORS TESTS                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n";
    
    try {
        auto traces = std::make_shared<TraceContainer>();
        SEval eval(traces);
        Parser parser;
        
        std::cout << "\n=== Testing JSON Serialization ===\n";
        
        // Test 1: Simple number to JSON
        {
            auto ast = parser.parse("(to-json 42)");
            auto result = eval.eval(ast);
            assert(result.is_string());
            assert(result.to_string() == "42");
            std::cout << "✓ to-json(42) => \"42\"\n";
        }
        
        // Test 2: List to JSON
        {
            auto ast = parser.parse("(to-json (list 1 2 3))");
            auto result = eval.eval(ast);
            assert(result.is_string());
            std::string json = result.to_string();
            assert(json.find("[") != std::string::npos);
            assert(json.find("1") != std::string::npos);
            std::cout << "✓ to-json(list) => JSON array\n";
        }
        
        // Test 3: from-json parsing
        {
            auto ast = parser.parse("(from-json \"[1,2,3]\")");
            auto result = eval.eval(ast);
            assert(result.is_list());
            auto list = result.to_list();
            assert(list->size() == 3);
            std::cout << "✓ from-json(\"[1,2,3]\") => list\n";
        }
        
        // Test 4: from-json with strings
        {
            auto ast = parser.parse("(from-json \"[\\\"hello\\\",\\\"world\\\"]\")");
            auto result = eval.eval(ast);
            assert(result.is_list());
            auto list = result.to_list();
            assert(list->size() == 2);
            std::cout << "✓ from-json with strings => list of strings\n";
        }
        
        std::cout << "\n=== Testing CSV Serialization ===\n";
        
        // Test 5: to-csv basic
        {
            auto ast = parser.parse("(to-csv (list (list 1 2) (list 3 4)))");
            auto result = eval.eval(ast);
            assert(result.is_string());
            std::string csv = result.to_string();
            assert(csv.find("1") != std::string::npos);
            std::cout << "✓ to-csv generates CSV output\n";
        }
        
        // Test 6: to-csv with custom delimiter
        {
            auto ast = parser.parse("(to-csv (list (list 1 2) (list 3 4)) \";\")");
            auto result = eval.eval(ast);
            assert(result.is_string());
            std::string csv = result.to_string();
            assert(csv.find(";") != std::string::npos);
            std::cout << "✓ to-csv with custom delimiter\n";
        }
        
        // Test 7: from-csv basic
        {
            auto ast = parser.parse("(from-csv \"1,2\\n3,4\")");
            auto result = eval.eval(ast);
            assert(result.is_list());
            auto rows = result.to_list();
            assert(rows->size() >= 1);
            std::cout << "✓ from-csv parses CSV input\n";
        }
        
        std::cout << "\n=== Testing Matrix Operations ===\n";
        
        // Test 8: matrix-transpose
        {
            auto ast = parser.parse("(matrix-transpose (list (list 1 2 3) (list 4 5 6)))");
            auto result = eval.eval(ast);
            assert(result.is_list());
            auto transposed = result.to_list();
            assert(transposed->size() == 3);
            auto first_row = (*transposed)[0].to_list();
            assert(first_row->size() == 2);
            std::cout << "✓ matrix-transpose: 2x3 -> 3x2\n";
        }
        
        // Test 9: json-array
        {
            auto ast = parser.parse("(json-array (list 1 2 3))");
            auto result = eval.eval(ast);
            assert(result.is_string());
            std::string json = result.to_string();
            assert(json.find("[") != std::string::npos);
            assert(json.find("1") != std::string::npos);
            std::cout << "✓ json-array produces JSON array format\n";
        }
        
        std::cout << "\n════════════════════════════════════════════════════════\n";
        std::cout << "🎉 ALL DATA EXPORT TESTS PASSED! (6 operators)\n";
        std::cout << "════════════════════════════════════════════════════════\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
