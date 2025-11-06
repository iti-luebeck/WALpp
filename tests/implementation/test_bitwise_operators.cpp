#include <iostream>
#include <cassert>
#include <cmath>
#include "wal/core.h"

using namespace wal;

void test_bitwise_operators() {
    std::cout << "\n=== Testing Bitwise Operators ===" << std::endl;
    
    Wal wal;
    
    // band (bitwise AND)
    auto result = wal.eval_str("(band 12 10)");
    assert(result.is_int() && result.to_int() == 8);
    std::cout << "✓ (band 12 10) => 8" << std::endl;
    
    // bor (bitwise OR)
    result = wal.eval_str("(bor 12 10)");
    assert(result.is_int() && result.to_int() == 14);
    std::cout << "✓ (bor 12 10) => 14" << std::endl;
    
    // bxor (bitwise XOR)
    result = wal.eval_str("(bxor 12 10)");
    assert(result.is_int() && result.to_int() == 6);
    std::cout << "✓ (bxor 12 10) => 6" << std::endl;
    
    // lshift (left shift)
    result = wal.eval_str("(lshift 5 2)");
    assert(result.is_int() && result.to_int() == 20);
    std::cout << "✓ (lshift 5 2) => 20" << std::endl;
    
    // rshift (right shift)
    result = wal.eval_str("(rshift 20 2)");
    assert(result.is_int() && result.to_int() == 5);
    std::cout << "✓ (rshift 20 2) => 5" << std::endl;
    
    // << alias
    result = wal.eval_str("(<< 3 3)");
    assert(result.is_int() && result.to_int() == 24);
    std::cout << "✓ (<< 3 3) => 24" << std::endl;
    
    // >> alias
    result = wal.eval_str("(>> 24 3)");
    assert(result.is_int() && result.to_int() == 3);
    std::cout << "✓ (>> 24 3) => 3" << std::endl;
    
    // bnot (bitwise NOT)
    result = wal.eval_str("(bnot 0)");
    assert(result.is_int() && result.to_int() == -1);
    std::cout << "✓ (bnot 0) => -1" << std::endl;
}

void test_list_operators() {
    std::cout << "\n=== Testing List Operators ===" << std::endl;
    
    Wal wal;
    
    // first (car)
    auto result = wal.eval_str("(first (list 1 2 3))");
    assert(result.is_int() && result.to_int() == 1);
    std::cout << "✓ (first (list 1 2 3)) => 1" << std::endl;
    
    // car alias
    result = wal.eval_str("(car (list 10 20 30))");
    assert(result.is_int() && result.to_int() == 10);
    std::cout << "✓ (car (list 10 20 30)) => 10" << std::endl;
    
    // rest (cdr)
    result = wal.eval_str("(rest (list 1 2 3))");
    assert(result.is_list());
    auto list = result.to_list();
    assert(list->size() == 2 && (*list)[0].to_int() == 2);
    std::cout << "✓ (rest (list 1 2 3)) => (2 3)" << std::endl;
    
    // cdr alias
    result = wal.eval_str("(cdr (list 10 20 30))");
    assert(result.is_list());
    list = result.to_list();
    assert(list->size() == 2 && (*list)[0].to_int() == 20);
    std::cout << "✓ (cdr (list 10 20 30)) => (20 30)" << std::endl;
    
    // length
    result = wal.eval_str("(length (list 1 2 3 4 5))");
    assert(result.is_int() && result.to_int() == 5);
    std::cout << "✓ (length (list 1 2 3 4 5)) => 5" << std::endl;
    
    // append
    result = wal.eval_str("(append (list 1 2) (list 3 4))");
    assert(result.is_list());
    list = result.to_list();
    assert(list->size() == 4);
    std::cout << "✓ (append (list 1 2) (list 3 4)) => (1 2 3 4)" << std::endl;
    
    // reverse
    result = wal.eval_str("(reverse (list 1 2 3))");
    assert(result.is_list());
    list = result.to_list();
    assert((*list)[0].to_int() == 3 && (*list)[2].to_int() == 1);
    std::cout << "✓ (reverse (list 1 2 3)) => (3 2 1)" << std::endl;
    
    // range with one arg
    result = wal.eval_str("(range 3)");
    assert(result.is_list());
    list = result.to_list();
    assert(list->size() == 3 && (*list)[0].to_int() == 0 && (*list)[2].to_int() == 2);
    std::cout << "✓ (range 3) => (0 1 2)" << std::endl;
    
    // range with two args
    result = wal.eval_str("(range 2 5)");
    assert(result.is_list());
    list = result.to_list();
    assert(list->size() == 3 && (*list)[0].to_int() == 2 && (*list)[2].to_int() == 4);
    std::cout << "✓ (range 2 5) => (2 3 4)" << std::endl;
    
    // map
    result = wal.eval_str("(map (lambda (x) (* x 2)) (list 1 2 3))");
    assert(result.is_list());
    list = result.to_list();
    assert(list->size() == 3 && (*list)[0].to_int() == 2 && (*list)[1].to_int() == 4 && (*list)[2].to_int() == 6);
    std::cout << "✓ (map (lambda (x) (* x 2)) (list 1 2 3)) => (2 4 6)" << std::endl;
    
    // fold with addition (using lambda)
    result = wal.eval_str("(fold (lambda (acc x) (+ acc x)) 0 (list 1 2 3 4))");
    assert(result.is_int() && result.to_int() == 10);
    std::cout << "✓ (fold (lambda (acc x) (+ acc x)) 0 (list 1 2 3 4)) => 10" << std::endl;
    
    // filter
    result = wal.eval_str("(filter (lambda (x) (< x 3)) (list 1 2 3 4 5))");
    assert(result.is_list());
    list = result.to_list();
    assert(list->size() == 2 && (*list)[0].to_int() == 1 && (*list)[1].to_int() == 2);
    std::cout << "✓ (filter (lambda (x) (< x 3)) (list 1 2 3 4 5)) => (1 2)" << std::endl;
    
    // zip
    result = wal.eval_str("(zip (list 1 2 3) (list 10 20 30))");
    assert(result.is_list());
    list = result.to_list();
    assert(list->size() == 3);
    auto pair1 = (*list)[0].to_list();
    assert(pair1->size() == 2 && (*pair1)[0].to_int() == 1 && (*pair1)[1].to_int() == 10);
    std::cout << "✓ (zip (list 1 2 3) (list 10 20 30)) => ((1 10) (2 20) (3 30))" << std::endl;
}

void test_type_operators() {
    std::cout << "\n=== Testing Type Operators ===" << std::endl;
    
    Wal wal;
    
    // type function
    auto result = wal.eval_str("(type 42)");
    assert(result.is_symbol() && result.to_symbol()->get_name() == "int");
    std::cout << "✓ (type 42) => int" << std::endl;
    
    result = wal.eval_str("(type 3.14)");
    assert(result.is_symbol() && result.to_symbol()->get_name() == "float");
    std::cout << "✓ (type 3.14) => float" << std::endl;
    
    result = wal.eval_str("(type \"hello\")");
    assert(result.is_symbol() && result.to_symbol()->get_name() == "string");
    std::cout << "✓ (type \"hello\") => string" << std::endl;
    
    result = wal.eval_str("(type (list 1 2))");
    assert(result.is_symbol() && result.to_symbol()->get_name() == "list");
    std::cout << "✓ (type (list 1 2)) => list" << std::endl;
    
    result = wal.eval_str("(type #t)");
    assert(result.is_symbol() && result.to_symbol()->get_name() == "bool");
    std::cout << "✓ (type #t) => bool" << std::endl;
    
    result = wal.eval_str("(type 'foo)");
    assert(result.is_symbol() && result.to_symbol()->get_name() == "symbol");
    std::cout << "✓ (type 'foo) => symbol" << std::endl;
    
    result = wal.eval_str("(type #n)");
    assert(result.is_symbol() && result.to_symbol()->get_name() == "nil");
    std::cout << "✓ (type #n) => nil" << std::endl;
    
    // Type predicates
    result = wal.eval_str("(is-int 42)");
    assert(result.is_bool() && result.to_bool() == true);
    std::cout << "✓ (is-int 42) => #t" << std::endl;
    
    result = wal.eval_str("(is-float 42)");
    assert(result.is_bool() && result.to_bool() == false);
    std::cout << "✓ (is-float 42) => #f" << std::endl;
    
    result = wal.eval_str("(is-string \"hello\")");
    assert(result.is_bool() && result.to_bool() == true);
    std::cout << "✓ (is-string \"hello\") => #t" << std::endl;
    
    result = wal.eval_str("(is-list (list 1 2))");
    assert(result.is_bool() && result.to_bool() == true);
    std::cout << "✓ (is-list (list 1 2)) => #t" << std::endl;
    
    result = wal.eval_str("(is-bool #t)");
    assert(result.is_bool() && result.to_bool() == true);
    std::cout << "✓ (is-bool #t) => #t" << std::endl;
    
    result = wal.eval_str("(is-symbol 'foo)");
    assert(result.is_bool() && result.to_bool() == true);
    std::cout << "✓ (is-symbol 'foo) => #t" << std::endl;
    
    result = wal.eval_str("(is-nil #n)");
    assert(result.is_bool() && result.to_bool() == true);
    std::cout << "✓ (is-nil #n) => #t" << std::endl;
    
    // Type conversions
    result = wal.eval_str("(convert-int 3.14)");
    assert(result.is_int() && result.to_int() == 3);
    std::cout << "✓ (convert-int 3.14) => 3" << std::endl;
    
    result = wal.eval_str("(convert-int \"42\")");
    assert(result.is_int() && result.to_int() == 42);
    std::cout << "✓ (convert-int \"42\") => 42" << std::endl;
    
    result = wal.eval_str("(convert-float 42)");
    assert(result.is_float() && std::abs(result.to_float() - 42.0) < 1e-9);
    std::cout << "✓ (convert-float 42) => 42.0" << std::endl;
    
    result = wal.eval_str("(convert-float \"3.14\")");
    assert(result.is_float() && std::abs(result.to_float() - 3.14) < 1e-9);
    std::cout << "✓ (convert-float \"3.14\") => 3.14" << std::endl;
    
    result = wal.eval_str("(convert-string 42)");
    assert(result.is_string() && result.to_string() == "42");
    std::cout << "✓ (convert-string 42) => \"42\"" << std::endl;
    
    result = wal.eval_str("(convert-string 3.14)");
    assert(result.is_string() && result.to_string() == "3.14");
    std::cout << "✓ (convert-string 3.14) => \"3.14\"" << std::endl;
}

int main() {
    try {
        test_bitwise_operators();
        test_list_operators();
        test_type_operators();
        
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "🎉 ALL PHASE 2 TESTS PASSED! (26-27 operators)" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
