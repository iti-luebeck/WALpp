#include <iostream>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include "wal/cli.h"
#include "wal/core.h"
#include "wal/repl.h"

namespace wal {

void print_test_header(const std::string& title) {
    std::cout << "\n╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << title << "\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n\n";
}

void assert_equal(const std::string& expected, const std::string& actual, const std::string& test_name) {
    if (expected == actual) {
        std::cout << "✓ " << test_name << std::endl;
    } else {
        std::cout << "✗ " << test_name << std::endl;
        std::cout << "  Expected: " << expected << std::endl;
        std::cout << "  Actual: " << actual << std::endl;
        assert(false);
    }
}

class CLITester {
public:
    std::shared_ptr<Wal> wal;
    
    CLITester() {
        wal = std::make_shared<Wal>();
    }
    
    void test_wal_evaluation() {
        print_test_header("WAL Core Evaluation");
        
        // Basic arithmetic
        Value result = wal->eval_str("(+ 1 2)");
        assert(result.is_int() && result.to_int() == 3);
        std::cout << "✓ Basic arithmetic: (+ 1 2) = 3\n";
        
        // Variable definition
        wal->eval_str("(define x 42)");
        result = wal->eval_str("x");
        assert(result.is_int() && result.to_int() == 42);
        std::cout << "✓ Variable definition and lookup\n";
        
        // Lambda
        wal->eval_str("(define double (lambda (n) (* n 2)))");
        result = wal->eval_str("(double 21)");
        assert(result.is_int() && result.to_int() == 42);
        std::cout << "✓ Lambda function application\n";
        
        // List operations
        result = wal->eval_str("(length '(1 2 3 4 5))");
        assert(result.is_int() && result.to_int() == 5);
        std::cout << "✓ List operations\n";
        
        // Conditional
        result = wal->eval_str("(if (> 5 3) 10 20)");
        assert(result.is_int() && result.to_int() == 10);
        std::cout << "✓ Conditional expressions\n";
    }
    
    void test_repl_basic() {
        print_test_header("REPL Basic Operations");
        
        auto repl = std::make_shared<REPL>(wal);
        
        // Test execute
        bool success = repl->execute("(+ 10 20)");
        assert(success);
        std::cout << "✓ REPL execute succeeded\n";
        
        // Test environment access
        auto env = repl->get_wal()->get_environment();
        assert(env != nullptr);
        std::cout << "✓ REPL environment accessible\n";
        
        // Test operator registry
        auto& registry = repl->get_wal()->get_operator_registry();
        std::cout << "✓ REPL operator registry accessible\n";
    }
    
    void test_file_loading() {
        print_test_header("File Execution");
        
        // Create a test file
        std::ofstream file("test_temp.wal");
        file << "(define result (+ 100 200))\n";
        file << "(* result 2)\n";
        file.close();
        
        auto repl = std::make_shared<REPL>(wal);
        bool success = repl->load_file("test_temp.wal");
        assert(success);
        std::cout << "✓ File loaded and executed\n";
        
        // Clean up
        std::remove("test_temp.wal");
    }
    
    void test_expression_parsing() {
        print_test_header("Expression Parsing");
        
        // Integer
        Value v = wal->eval_str("42");
        assert(v.is_int() && v.to_int() == 42);
        std::cout << "✓ Integer parsing\n";
        
        // Float
        v = wal->eval_str("3.14");
        assert(v.is_float() && v.to_float() > 3.1 && v.to_float() < 3.2);
        std::cout << "✓ Float parsing\n";
        
        // String
        v = wal->eval_str("\"hello\"");
        assert(v.is_string() && v.to_string() == "hello");
        std::cout << "✓ String parsing\n";
        
        // Symbol
        v = wal->eval_str("'x");
        assert(v.is_symbol());
        std::cout << "✓ Symbol parsing\n";
        
        // List
        v = wal->eval_str("'(1 2 3)");
        assert(v.is_list());
        auto list = v.to_list();
        assert(list->size() == 3);
        std::cout << "✓ List parsing\n";
    }
    
    void test_operators() {
        print_test_header("Operator Tests");
        
        // Arithmetic
        Value result = wal->eval_str("(+ 1 2 3 4 5)");
        assert(result.is_int() && result.to_int() == 15);
        std::cout << "✓ Addition operator\n";
        
        // Comparison
        result = wal->eval_str("(> 10 5)");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ Comparison operator\n";
        
        // Type checking
        result = wal->eval_str("(is-int 42)");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ Type checking operator\n";
        
        // String operations
        result = wal->eval_str("(concat \"hello\" \" \" \"world\")");
        assert(result.is_string() && result.to_string() == "hello world");
        std::cout << "✓ String operations\n";
    }
    
    void test_scoping() {
        print_test_header("Variable Scoping");
        
        // Global scope
        wal->eval_str("(define global-var 100)");
        Value result = wal->eval_str("global-var");
        assert(result.is_int() && result.to_int() == 100);
        std::cout << "✓ Global variable\n";
        
        // Simple lambda
        wal->eval_str("(define square (lambda (x) (* x x)))");
        result = wal->eval_str("(square 7)");
        assert(result.is_int() && result.to_int() == 49);
        std::cout << "✓ Lambda function\n";
        
        // Closure
        wal->eval_str("(define make-multiplier (lambda (n) (lambda (x) (* n x))))");
        wal->eval_str("(define times-3 (make-multiplier 3))");
        result = wal->eval_str("(times-3 5)");
        assert(result.is_int() && result.to_int() == 15);
        std::cout << "✓ Closure capture\n";
    }
    
    void test_error_handling() {
        print_test_header("Error Handling");
        
        auto repl = std::make_shared<REPL>(wal);
        
        // Invalid expression - should not crash
        bool success = repl->execute("(+ invalid-var 1)");
        std::cout << "✓ Invalid variable handled gracefully\n";
        
        // Type mismatch - should not crash  
        success = repl->execute("(+ \"string\" 123)");
        std::cout << "✓ Type mismatch handled gracefully\n";
    }
    
    void run_all_tests() {
        std::cout << "\n";
        std::cout << "████████████████████████████████████████████████████\n";
        std::cout << "        CLI & REPL TEST SUITE\n";
        std::cout << "████████████████████████████████████████████████████\n";
        
        test_wal_evaluation();
        test_repl_basic();
        test_file_loading();
        test_expression_parsing();
        test_operators();
        test_scoping();
        test_error_handling();
        
        std::cout << "\n";
        std::cout << "════════════════════════════════════════════════════════\n";
        std::cout << "🎉 ALL CLI & REPL TESTS PASSED!\n";
        std::cout << "════════════════════════════════════════════════════════\n\n";
    }
};

} // namespace wal

int main() {
    try {
        wal::CLITester tester;
        tester.run_all_tests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
