#include <iostream>
#include <cassert>
#include "wal/core.h"

namespace wal {

void print_section(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

void assert_value(const Value& val, bool expected_int, bool expected_bool, 
                  int64_t int_val = 0, bool bool_val = false, const std::string& test = "") {
    if (expected_int) {
        assert(val.is_int());
        assert(val.to_int() == int_val);
    } else if (expected_bool) {
        assert(val.is_bool());
        assert(val.to_bool() == bool_val);
    }
    if (!test.empty()) {
        std::cout << "✓ " << test << "\n";
    }
}

} // namespace wal

int main() {
    try {
        auto wal = std::make_shared<wal::Wal>();
        
        std::cout << "╔════════════════════════════════════════════════════════╗\n";
        std::cout << "║           PHASE 1 FOUNDATION OPERATORS TESTS          ║\n";
        std::cout << "╚════════════════════════════════════════════════════════╝\n";
        
        // Test arithmetic operators
        wal::print_section("Arithmetic Operators");
        
        // Addition
        wal::Value result = wal->eval_str("(+ 1 2)");
        assert(result.is_int() && result.to_int() == 3);
        std::cout << "✓ (+ 1 2) = 3\n";
        
        result = wal->eval_str("(+ 1 2 3 4 5)");
        assert(result.is_int() && result.to_int() == 15);
        std::cout << "✓ (+ 1 2 3 4 5) = 15\n";
        
        result = wal->eval_str("(+ 1.5 2.5)");
        assert(result.is_float() && result.to_float() > 3.9 && result.to_float() < 4.1);
        std::cout << "✓ (+ 1.5 2.5) ≈ 4.0\n";
        
        // Subtraction
        result = wal->eval_str("(- 10 3)");
        assert(result.is_int() && result.to_int() == 7);
        std::cout << "✓ (- 10 3) = 7\n";
        
        result = wal->eval_str("(- 5)");
        assert(result.is_int() && result.to_int() == -5);
        std::cout << "✓ (- 5) = -5\n";
        
        // Multiplication
        result = wal->eval_str("(* 3 4)");
        assert(result.is_int() && result.to_int() == 12);
        std::cout << "✓ (* 3 4) = 12\n";
        
        result = wal->eval_str("(* 2 3 4)");
        assert(result.is_int() && result.to_int() == 24);
        std::cout << "✓ (* 2 3 4) = 24\n";
        
        // Division
        result = wal->eval_str("(/ 20 4)");
        assert(result.is_float() && result.to_float() == 5.0);
        std::cout << "✓ (/ 20 4) = 5.0\n";
        
        // Modulo
        result = wal->eval_str("(% 17 5)");
        assert(result.is_int() && result.to_int() == 2);
        std::cout << "✓ (% 17 5) = 2\n";
        
        // Power
        result = wal->eval_str("(** 2 3)");
        assert(result.is_float() && result.to_float() == 8.0);
        std::cout << "✓ (** 2 3) = 8.0\n";
        
        // Rounding functions
        result = wal->eval_str("(floor 3.7)");
        assert(result.is_int() && result.to_int() == 3);
        std::cout << "✓ (floor 3.7) = 3\n";
        
        result = wal->eval_str("(ceil 3.2)");
        assert(result.is_int() && result.to_int() == 4);
        std::cout << "✓ (ceil 3.2) = 4\n";
        
        result = wal->eval_str("(round 3.5)");
        assert(result.is_int() && result.to_int() == 4);
        std::cout << "✓ (round 3.5) = 4\n";
        
        // Absolute value
        result = wal->eval_str("(abs -5)");
        assert(result.is_int() && result.to_int() == 5);
        std::cout << "✓ (abs -5) = 5\n";
        
        result = wal->eval_str("(abs -3.5)");
        assert(result.is_float() && result.to_float() > 3.4 && result.to_float() < 3.6);
        std::cout << "✓ (abs -3.5) ≈ 3.5\n";
        
        // Min/Max
        result = wal->eval_str("(min 5 3 8 1)");
        assert(result.is_float() && result.to_float() == 1.0);
        std::cout << "✓ (min 5 3 8 1) = 1\n";
        
        result = wal->eval_str("(max 5 3 8 1)");
        assert(result.is_float() && result.to_float() == 8.0);
        std::cout << "✓ (max 5 3 8 1) = 8\n";
        
        // Test comparison operators
        wal::print_section("Comparison Operators");
        
        result = wal->eval_str("(< 5 10)");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (< 5 10) = true\n";
        
        result = wal->eval_str("(> 5 10)");
        assert(result.is_bool() && !result.to_bool());
        std::cout << "✓ (> 5 10) = false\n";
        
        result = wal->eval_str("(<= 5 5)");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (<= 5 5) = true\n";
        
        result = wal->eval_str("(>= 5 5)");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (>= 5 5) = true\n";
        
        result = wal->eval_str("(= 5 5)");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (= 5 5) = true\n";
        
        result = wal->eval_str("(!= 5 5)");
        assert(result.is_bool() && !result.to_bool());
        std::cout << "✓ (!= 5 5) = false\n";
        
        result = wal->eval_str("(= 5 5 5)");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (= 5 5 5) = true\n";
        
        // Test logical operators
        wal::print_section("Logical Operators");
        
        result = wal->eval_str("(&& (= 5 5) (= 3 3))");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (&& (= 5 5) (= 3 3)) = true\n";
        
        result = wal->eval_str("(&& (= 5 5) (= 3 5))");
        assert(result.is_bool() && !result.to_bool());
        std::cout << "✓ (&& (= 5 5) (= 3 5)) = false\n";
        
        result = wal->eval_str("(|| (= 5 10) (= 3 3))");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (|| (= 5 10) (= 3 3)) = true\n";
        
        result = wal->eval_str("(|| (= 5 10) (= 3 5))");
        assert(result.is_bool() && !result.to_bool());
        std::cout << "✓ (|| (= 5 10) (= 3 5)) = false\n";
        
        result = wal->eval_str("(! (= 5 10))");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (! (= 5 10)) = true\n";
        
        result = wal->eval_str("(! (= 5 5))");
        assert(result.is_bool() && !result.to_bool());
        std::cout << "✓ (! (= 5 5)) = false\n";
        
        // Test conditionals
        wal::print_section("Conditional Operators");
        
        result = wal->eval_str("(if (> 5 3) 10 20)");
        assert(result.is_int() && result.to_int() == 10);
        std::cout << "✓ (if (> 5 3) 10 20) = 10\n";
        
        result = wal->eval_str("(if (< 5 3) 10 20)");
        assert(result.is_int() && result.to_int() == 20);
        std::cout << "✓ (if (< 5 3) 10 20) = 20\n";
        
        // Test variables
        wal::print_section("Variables");
        
        wal->eval_str("(define x 42)");
        result = wal->eval_str("x");
        assert(result.is_int() && result.to_int() == 42);
        std::cout << "✓ (define x 42); x = 42\n";
        
        wal->eval_str("(define y (+ x 8))");
        result = wal->eval_str("y");
        assert(result.is_int() && result.to_int() == 50);
        std::cout << "✓ (define y (+ x 8)); y = 50\n";
        
        // Test lambda
        wal::print_section("Lambda & Function Calls");
        
        wal->eval_str("(define square (lambda (x) (* x x)))");
        result = wal->eval_str("(square 5)");
        assert(result.is_int() && result.to_int() == 25);
        std::cout << "✓ (define square (lambda (x) (* x x))); (square 5) = 25\n";
        
        wal->eval_str("(define add (lambda (a b) (+ a b)))");
        result = wal->eval_str("(add 10 20)");
        assert(result.is_int() && result.to_int() == 30);
        std::cout << "✓ (define add (lambda (a b) (+ a b))); (add 10 20) = 30\n";
        
        // Test list operations
        wal::print_section("List Operations");
        
        result = wal->eval_str("(list 1 2 3)");
        assert(result.is_list());
        auto list = result.to_list();
        assert(list->size() == 3);
        std::cout << "✓ (list 1 2 3) creates list of 3 elements\n";
        
        result = wal->eval_str("'(1 2 3)");
        assert(result.is_list());
        std::cout << "✓ '(1 2 3) is a list\n";
        
        // Test quote
        result = wal->eval_str("(quote x)");
        assert(result.is_symbol());
        std::cout << "✓ (quote x) = symbol x\n";
        
        // Test car/cdr
        result = wal->eval_str("(car '(1 2 3))");
        assert(result.is_int() && result.to_int() == 1);
        std::cout << "✓ (car '(1 2 3)) = 1\n";
        
        result = wal->eval_str("(cdr '(1 2 3))");
        assert(result.is_list());
        auto cdr_list = result.to_list();
        assert(cdr_list->size() == 2);
        std::cout << "✓ (cdr '(1 2 3)) is list of 2 elements\n";
        
        // Test length
        result = wal->eval_str("(length '(1 2 3 4 5))");
        assert(result.is_int() && result.to_int() == 5);
        std::cout << "✓ (length '(1 2 3 4 5)) = 5\n";
        
        // Test append
        result = wal->eval_str("(append '(1 2) '(3 4))");
        assert(result.is_list());
        auto append_list = result.to_list();
        assert(append_list->size() == 4);
        std::cout << "✓ (append '(1 2) '(3 4)) creates list of 4 elements\n";
        
        // Test reverse
        result = wal->eval_str("(reverse '(1 2 3))");
        assert(result.is_list());
        auto rev_list = result.to_list();
        assert((*rev_list)[0].to_int() == 3);
        assert((*rev_list)[2].to_int() == 1);
        std::cout << "✓ (reverse '(1 2 3)) = (3 2 1)\n";
        
        // Test type checking
        wal::print_section("Type Checking");
        
        result = wal->eval_str("(is-int 42)");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (is-int 42) = true\n";
        
        result = wal->eval_str("(is-float 3.14)");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (is-float 3.14) = true\n";
        
        result = wal->eval_str("(is-string \"hello\")");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (is-string \"hello\") = true\n";
        
        result = wal->eval_str("(is-symbol 'x)");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (is-symbol 'x) = true\n";
        
        result = wal->eval_str("(is-list '(1 2 3))");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (is-list '(1 2 3)) = true\n";
        
        result = wal->eval_str("(is-bool (= 1 1))");
        assert(result.is_bool() && result.to_bool());
        std::cout << "✓ (is-bool (= 1 1)) = true\n";
        
        // Test complex expressions
        wal::print_section("Complex Expressions");
        
        result = wal->eval_str("(+ (* 2 3) (* 4 5))");
        assert(result.is_int() && result.to_int() == 26);
        std::cout << "✓ (+ (* 2 3) (* 4 5)) = 26\n";
        
        result = wal->eval_str("(if (&& (> 5 3) (< 10 20)) (+ 1 2) (* 3 4))");
        assert(result.is_int() && result.to_int() == 3);
        std::cout << "✓ (if (&& (> 5 3) (< 10 20)) (+ 1 2) (* 3 4)) = 3\n";
        
        wal->eval_str("(define fact (lambda (n) (if (<= n 1) 1 (* n (fact (- n 1))))))");
        result = wal->eval_str("(fact 5)");
        assert(result.is_int() && result.to_int() == 120);
        std::cout << "✓ (fact 5) = 120\n";
        
        std::cout << "\n════════════════════════════════════════════════════════\n";
        std::cout << "🎉 ALL PHASE 1 TESTS PASSED! (50+ operators tested)\n";
        std::cout << "════════════════════════════════════════════════════════\n\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
