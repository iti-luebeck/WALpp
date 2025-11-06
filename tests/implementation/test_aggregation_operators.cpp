#include "wal/core.h"
#include <iostream>
#include <cassert>
#include <cmath>

void test_aggregation_operators() {
    wal::Wal evaluator;
    
    std::cout << "\n=== Testing Aggregation Operators ===" << std::endl;

    // sum
    auto result = evaluator.eval_str("(sum (list 1 2 3 4 5))");
    assert(result.is_int() && result.to_int() == 15);
    std::cout << "✓ (sum (list 1 2 3 4 5)) => 15" << std::endl;

    // sum with floats
    result = evaluator.eval_str("(sum (list 1.5 2.5 3.0))");
    assert(result.is_float() && std::abs(result.to_float() - 7.0) < 1e-9);
    std::cout << "✓ (sum (list 1.5 2.5 3.0)) => 7.0" << std::endl;

    // product
    result = evaluator.eval_str("(product (list 2 3 4))");
    assert(result.is_int() && result.to_int() == 24);
    std::cout << "✓ (product (list 2 3 4)) => 24" << std::endl;

    // product with floats
    result = evaluator.eval_str("(product (list 2.0 3.0 4.0))");
    assert(result.is_float() && std::abs(result.to_float() - 24.0) < 1e-9);
    std::cout << "✓ (product (list 2.0 3.0 4.0)) => 24.0" << std::endl;

    // mean
    result = evaluator.eval_str("(mean (list 2 4 6 8))");
    assert(result.is_float() && std::abs(result.to_float() - 5.0) < 1e-9);
    std::cout << "✓ (mean (list 2 4 6 8)) => 5.0" << std::endl;

    // min-val
    result = evaluator.eval_str("(min-val (list 5 2 8 1 9))");
    assert(result.is_int() && result.to_int() == 1);
    std::cout << "✓ (min-val (list 5 2 8 1 9)) => 1" << std::endl;

    // max-val
    result = evaluator.eval_str("(max-val (list 5 2 8 1 9))");
    assert(result.is_int() && result.to_int() == 9);
    std::cout << "✓ (max-val (list 5 2 8 1 9)) => 9" << std::endl;

    // min-val with floats
    result = evaluator.eval_str("(min-val (list 5.5 2.1 8.9 1.2))");
    assert(result.is_float() && std::abs(result.to_float() - 1.2) < 1e-9);
    std::cout << "✓ (min-val (list 5.5 2.1 8.9 1.2)) => 1.2" << std::endl;

    // max-val with floats
    result = evaluator.eval_str("(max-val (list 5.5 2.1 8.9 1.2))");
    assert(result.is_float() && std::abs(result.to_float() - 8.9) < 1e-9);
    std::cout << "✓ (max-val (list 5.5 2.1 8.9 1.2)) => 8.9" << std::endl;
}

void test_comparison_operators() {
    wal::Wal evaluator;
    
    std::cout << "\n=== Testing Comparison Operators ===" << std::endl;

    // between
    auto result = evaluator.eval_str("(between 5 1 10)");
    assert(result.is_bool() && result.to_bool() == true);
    std::cout << "✓ (between 5 1 10) => #t" << std::endl;

    result = evaluator.eval_str("(between 15 1 10)");
    assert(result.is_bool() && result.to_bool() == false);
    std::cout << "✓ (between 15 1 10) => #f" << std::endl;

    result = evaluator.eval_str("(between 1 1 10)");
    assert(result.is_bool() && result.to_bool() == true);
    std::cout << "✓ (between 1 1 10) => #t (lower bound inclusive)" << std::endl;

    result = evaluator.eval_str("(between 10 1 10)");
    assert(result.is_bool() && result.to_bool() == true);
    std::cout << "✓ (between 10 1 10) => #t (upper bound inclusive)" << std::endl;

    // clamp
    result = evaluator.eval_str("(clamp 5 1 10)");
    assert(result.is_int() && result.to_int() == 5);
    std::cout << "✓ (clamp 5 1 10) => 5" << std::endl;

    result = evaluator.eval_str("(clamp 15 1 10)");
    assert(result.is_int() && result.to_int() == 10);
    std::cout << "✓ (clamp 15 1 10) => 10 (clamped to upper)" << std::endl;

    result = evaluator.eval_str("(clamp -5 1 10)");
    assert(result.is_int() && result.to_int() == 1);
    std::cout << "✓ (clamp -5 1 10) => 1 (clamped to lower)" << std::endl;

    // in?
    result = evaluator.eval_str("(in? 3 (list 1 2 3 4 5))");
    assert(result.is_bool() && result.to_bool() == true);
    std::cout << "✓ (in? 3 (list 1 2 3 4 5)) => #t" << std::endl;

    result = evaluator.eval_str("(in? 10 (list 1 2 3 4 5))");
    assert(result.is_bool() && result.to_bool() == false);
    std::cout << "✓ (in? 10 (list 1 2 3 4 5)) => #f" << std::endl;

    result = evaluator.eval_str("(in? \"hello\" (list \"hello\" \"world\"))");
    assert(result.is_bool() && result.to_bool() == true);
    std::cout << "✓ (in? \"hello\" (list \"hello\" \"world\")) => #t (string)" << std::endl;

    // mixed types in between
    result = evaluator.eval_str("(between 5.5 1 10)");
    assert(result.is_bool() && result.to_bool() == true);
    std::cout << "✓ (between 5.5 1 10) => #t (mixed types)" << std::endl;

    // mixed types in clamp
    result = evaluator.eval_str("(clamp 3.7 1 10)");
    assert(result.is_float() && std::abs(result.to_float() - 3.7) < 1e-9);
    std::cout << "✓ (clamp 3.7 1 10) => 3.7 (mixed types)" << std::endl;
}

int main() {
    try {
        test_aggregation_operators();
        test_comparison_operators();

        std::cout << "\n" << std::string(58, '=') << std::endl;
        std::cout << "🎉 ALL PHASE 4 TESTS PASSED! (16 operators)" << std::endl;
        std::cout << std::string(58, '=') << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
