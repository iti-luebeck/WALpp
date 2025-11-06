#include <iostream>
#include <cassert>
#include <cmath>
#include "wal/core.h"

using namespace wal;

void test_string_operators() {
    std::cout << "\n=== Testing String Operators ===" << std::endl;
    
    Wal wal;
    
    // string-length
    auto result = wal.eval_str("(string-length \"hello\")");
    assert(result.is_int() && result.to_int() == 5);
    std::cout << "✓ (string-length \"hello\") => 5" << std::endl;
    
    // substring
    result = wal.eval_str("(substring \"hello\" 1 3)");
    assert(result.is_string() && result.to_string() == "ell");
    std::cout << "✓ (substring \"hello\" 1 3) => \"ell\"" << std::endl;
    
    // concat
    result = wal.eval_str("(concat \"hello\" \" \" \"world\")");
    assert(result.is_string() && result.to_string() == "hello world");
    std::cout << "✓ (concat \"hello\" \" \" \"world\") => \"hello world\"" << std::endl;
    
    // split
    result = wal.eval_str("(split \"a,b,c\" \",\")");
    assert(result.is_list());
    auto list = result.to_list();
    assert(list->size() == 3 && (*list)[0].to_string() == "a");
    std::cout << "✓ (split \"a,b,c\" \",\") => (\"a\" \"b\" \"c\")" << std::endl;
    
    // join
    result = wal.eval_str("(join (list \"a\" \"b\" \"c\") \",\")");
    assert(result.is_string() && result.to_string() == "a,b,c");
    std::cout << "✓ (join (list \"a\" \"b\" \"c\") \",\") => \"a,b,c\"" << std::endl;
    
    // uppercase
    result = wal.eval_str("(uppercase \"hello\")");
    assert(result.is_string() && result.to_string() == "HELLO");
    std::cout << "✓ (uppercase \"hello\") => \"HELLO\"" << std::endl;
    
    // lowercase
    result = wal.eval_str("(lowercase \"HELLO\")");
    assert(result.is_string() && result.to_string() == "hello");
    std::cout << "✓ (lowercase \"HELLO\") => \"hello\"" << std::endl;
    
    // trim
    result = wal.eval_str("(trim \"  hello  \")");
    assert(result.is_string() && result.to_string() == "hello");
    std::cout << "✓ (trim \"  hello  \") => \"hello\"" << std::endl;
    
    // startswith
    result = wal.eval_str("(startswith \"hello\" \"hel\")");
    assert(result.is_bool() && result.to_bool() == true);
    std::cout << "✓ (startswith \"hello\" \"hel\") => #t" << std::endl;
    
    // endswith
    result = wal.eval_str("(endswith \"hello\" \"lo\")");
    assert(result.is_bool() && result.to_bool() == true);
    std::cout << "✓ (endswith \"hello\" \"lo\") => #t" << std::endl;
    
    // replace
    result = wal.eval_str("(replace \"hello world\" \"world\" \"C++\")");
    assert(result.is_string() && result.to_string() == "hello C++");
    std::cout << "✓ (replace \"hello world\" \"world\" \"C++\") => \"hello C++\"" << std::endl;
    
    // string-index
    result = wal.eval_str("(string-index \"hello\" \"ll\")");
    assert(result.is_int() && result.to_int() == 2);
    std::cout << "✓ (string-index \"hello\" \"ll\") => 2" << std::endl;
    
    // contains
    result = wal.eval_str("(contains \"hello\" \"ell\")");
    assert(result.is_bool() && result.to_bool() == true);
    std::cout << "✓ (contains \"hello\" \"ell\") => #t" << std::endl;
}

void test_math_operators() {
    std::cout << "\n=== Testing Math Operators ===" << std::endl;
    
    Wal wal;
    
    // sin
    auto result = wal.eval_str("(sin 0)");
    assert(result.is_float() && std::abs(result.to_float()) < 1e-9);
    std::cout << "✓ (sin 0) => ~0.0" << std::endl;
    
    // cos
    result = wal.eval_str("(cos 0)");
    assert(result.is_float() && std::abs(result.to_float() - 1.0) < 1e-9);
    std::cout << "✓ (cos 0) => ~1.0" << std::endl;
    
    // sqrt
    result = wal.eval_str("(sqrt 16)");
    assert(result.is_float() && std::abs(result.to_float() - 4.0) < 1e-9);
    std::cout << "✓ (sqrt 16) => ~4.0" << std::endl;
    
    // exp
    result = wal.eval_str("(exp 0)");
    assert(result.is_float() && std::abs(result.to_float() - 1.0) < 1e-9);
    std::cout << "✓ (exp 0) => ~1.0" << std::endl;
    
    // log
    result = wal.eval_str("(log 1)");
    assert(result.is_float() && std::abs(result.to_float()) < 1e-9);
    std::cout << "✓ (log 1) => ~0.0" << std::endl;
    
    // abs with negative
    result = wal.eval_str("(abs -5)");
    assert(result.is_int() && result.to_int() == 5);
    std::cout << "✓ (abs -5) => 5" << std::endl;
    
    // abs with float
    result = wal.eval_str("(abs -3.14)");
    assert(result.is_float() && std::abs(result.to_float() - 3.14) < 1e-9);
    std::cout << "✓ (abs -3.14) => ~3.14" << std::endl;
    
    // min
    result = wal.eval_str("(min 5 2 8 1 9)");
    assert(result.is_int() && result.to_int() == 1);
    std::cout << "✓ (min 5 2 8 1 9) => 1" << std::endl;
    
    // max
    result = wal.eval_str("(max 5 2 8 1 9)");
    assert(result.is_int() && result.to_int() == 9);
    std::cout << "✓ (max 5 2 8 1 9) => 9" << std::endl;
    
    // min with floats
    result = wal.eval_str("(min 2.5 1.1 3.2)");
    assert(result.is_float() && std::abs(result.to_float() - 1.1) < 1e-9);
    std::cout << "✓ (min 2.5 1.1 3.2) => ~1.1" << std::endl;
}

void test_array_operators() {
    std::cout << "\n=== Testing Array Operators ===" << std::endl;
    
    Wal wal;
    
    // array-ref
    auto result = wal.eval_str("(array-ref (list 10 20 30) 1)");
    assert(result.is_int() && result.to_int() == 20);
    std::cout << "✓ (array-ref (list 10 20 30) 1) => 20" << std::endl;
    
    // array-set
    result = wal.eval_str("(array-set (list 10 20 30) 1 99)");
    assert(result.is_list());
    auto list = result.to_list();
    assert(list->size() == 3 && (*list)[1].to_int() == 99);
    std::cout << "✓ (array-set (list 10 20 30) 1 99) => (10 99 30)" << std::endl;
    
    // slice
    result = wal.eval_str("(slice (list 1 2 3 4 5) 1 4)");
    assert(result.is_list());
    list = result.to_list();
    assert(list->size() == 3 && (*list)[0].to_int() == 2);
    std::cout << "✓ (slice (list 1 2 3 4 5) 1 4) => (2 3 4)" << std::endl;
    
    // flatten
    result = wal.eval_str("(flatten (list (list 1 2) (list 3 4)))");
    assert(result.is_list());
    list = result.to_list();
    assert(list->size() == 4 && (*list)[0].to_int() == 1);
    std::cout << "✓ (flatten (list (list 1 2) (list 3 4))) => (1 2 3 4)" << std::endl;
    
    // sort
    result = wal.eval_str("(sort (list 3 1 4 1 5))");
    assert(result.is_list());
    list = result.to_list();
    assert(list->size() == 5 && (*list)[0].to_int() == 1 && (*list)[4].to_int() == 5);
    std::cout << "✓ (sort (list 3 1 4 1 5)) => (1 1 3 4 5)" << std::endl;
    
    // unique
    result = wal.eval_str("(unique (list 3 1 4 1 5 3 2))");
    assert(result.is_list());
    list = result.to_list();
    assert(list->size() == 5);
    std::cout << "✓ (unique (list 3 1 4 1 5 3 2)) => (1 2 3 4 5)" << std::endl;
    
    // take
    result = wal.eval_str("(take (list 1 2 3 4 5) 3)");
    assert(result.is_list());
    list = result.to_list();
    assert(list->size() == 3 && (*list)[2].to_int() == 3);
    std::cout << "✓ (take (list 1 2 3 4 5) 3) => (1 2 3)" << std::endl;
    
    // drop
    result = wal.eval_str("(drop (list 1 2 3 4 5) 2)");
    assert(result.is_list());
    list = result.to_list();
    assert(list->size() == 3 && (*list)[0].to_int() == 3);
    std::cout << "✓ (drop (list 1 2 3 4 5) 2) => (3 4 5)" << std::endl;
    
    // nth
    result = wal.eval_str("(nth 2 (list 10 20 30 40))");
    assert(result.is_int() && result.to_int() == 30);
    std::cout << "✓ (nth 2 (list 10 20 30 40)) => 30" << std::endl;
}

int main() {
    try {
        test_string_operators();
        test_math_operators();
        test_array_operators();
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "🎉 ALL PHASE 3 TESTS PASSED! (32-35 operators)" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
