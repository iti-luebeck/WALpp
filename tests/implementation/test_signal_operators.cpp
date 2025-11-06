#include "wal/core.h"
#include <iostream>
#include <cassert>
#include <cmath>

void test_signal_filtering() {
    wal::Wal evaluator;
    
    std::cout << "\n=== Testing Signal Filtering Operators ===" << std::endl;

    // moving-average
    auto result = evaluator.eval_str("(moving-average (list 1 2 3 4 5) 2)");
    assert(result.is_list());
    auto list = result.to_list();
    assert(list->size() == 4);
    std::cout << "✓ (moving-average (list 1 2 3 4 5) 2) => 4-element list" << std::endl;

    // moving-average with larger window
    result = evaluator.eval_str("(moving-average (list 10 20 30 40 50) 3)");
    assert(result.is_list());
    list = result.to_list();
    assert(list->size() == 3);
    assert(list->at(0).is_float());
    double avg1 = list->at(0).to_float();
    assert(std::abs(avg1 - 20.0) < 1e-9);  // (10+20+30)/3 = 20
    std::cout << "✓ (moving-average (list 10 20 30 40 50) 3) => correct values" << std::endl;

    // zero-crossings
    result = evaluator.eval_str("(zero-crossings (list 1 2 -1 -2 1 2))");
    assert(result.is_int() && result.to_int() == 2);
    std::cout << "✓ (zero-crossings (list 1 2 -1 -2 1 2)) => 2" << std::endl;

    // zero-crossings with no crossings
    result = evaluator.eval_str("(zero-crossings (list 1 2 3 4 5))");
    assert(result.is_int() && result.to_int() == 0);
    std::cout << "✓ (zero-crossings (list 1 2 3 4 5)) => 0 (no crossings)" << std::endl;

    // rms
    result = evaluator.eval_str("(rms (list 3 4))");
    assert(result.is_float());
    double rms_val = result.to_float();
    assert(std::abs(rms_val - 3.5355339) < 0.01);  // sqrt((9+16)/2) ≈ 3.536
    std::cout << "✓ (rms (list 3 4)) => ~3.536" << std::endl;

    // peak-to-peak
    result = evaluator.eval_str("(peak-to-peak (list 1 5 3 -2 4))");
    assert(result.is_float());
    double ptp = result.to_float();
    assert(std::abs(ptp - 7.0) < 1e-9);  // max=5, min=-2, diff=7
    std::cout << "✓ (peak-to-peak (list 1 5 3 -2 4)) => 7.0" << std::endl;

    // crest-factor
    result = evaluator.eval_str("(crest-factor (list 1 1 1 1))");
    assert(result.is_float());
    double cf = result.to_float();
    assert(std::abs(cf - 1.0) < 1e-9);  // peak=1, rms=1, cf=1
    std::cout << "✓ (crest-factor (list 1 1 1 1)) => 1.0" << std::endl;

    // duty-cycle
    result = evaluator.eval_str("(duty-cycle (list 1 1 0 0 1 1) 0.5)");
    assert(result.is_float());
    double duty = result.to_float();
    assert(std::abs(duty - 66.6666666) < 0.1);  // 4 out of 6 values > 0.5
    std::cout << "✓ (duty-cycle (list 1 1 0 0 1 1) 0.5) => ~66.67%" << std::endl;

    // autocorrelation
    result = evaluator.eval_str("(autocorrelation (list 1 2 3 4 5) 0)");
    assert(result.is_float() && result.to_float() > 0.99 && result.to_float() < 1.01);
    std::cout << "✓ (autocorrelation (list 1 2 3 4 5) 0) => ~1.0 (lag 0)" << std::endl;
}

void test_signal_analysis() {
    wal::Wal evaluator;
    
    std::cout << "\n=== Testing Signal Analysis Operators ===" << std::endl;

    // edges - find transitions
    auto result = evaluator.eval_str("(edges (list 0 0 1 1 0 0) 0.5)");
    assert(result.is_list());
    auto list = result.to_list();
    assert(list->size() == 2);  // One rising edge at index 2, one falling edge at index 4
    std::cout << "✓ (edges (list 0 0 1 1 0 0) 0.5) => 2 edges found" << std::endl;

    // edges - rising edge
    result = evaluator.eval_str("(edges (list 0 0 0 1 1 1) 0.5)");
    assert(result.is_list());
    list = result.to_list();
    assert(list->size() == 1);
    // Check first edge is rising
    auto edge = list->at(0).to_list();
    assert(edge->at(0).is_symbol() && edge->at(0).to_symbol()->get_name() == "rising");
    std::cout << "✓ (edges (list 0 0 0 1 1 1) 0.5) => 1 rising edge" << std::endl;

    // edges - falling edge
    result = evaluator.eval_str("(edges (list 1 1 1 0 0 0) 0.5)");
    assert(result.is_list());
    list = result.to_list();
    // Starting at 1, we should see one falling edge at index 3
    assert(list->size() >= 1);
    // Find the falling edge
    bool found_falling = false;
    for (size_t i = 0; i < list->size(); ++i) {
        edge = list->at(i).to_list();
        if (edge->at(0).is_symbol() && edge->at(0).to_symbol()->get_name() == "falling") {
            found_falling = true;
            break;
        }
    }
    assert(found_falling);
    std::cout << "✓ (edges (list 1 1 1 0 0 0) 0.5) => falling edge detected" << std::endl;

    // Zero signal
    result = evaluator.eval_str("(zero-crossings (list 0 0 0 0))");
    assert(result.is_int() && result.to_int() == 0);
    std::cout << "✓ (zero-crossings (list 0 0 0 0)) => 0 (no crossings in flat signal)" << std::endl;

    // RMS with single value
    result = evaluator.eval_str("(rms (list 5))");
    assert(result.is_float() && std::abs(result.to_float() - 5.0) < 1e-9);
    std::cout << "✓ (rms (list 5)) => 5.0 (single element)" << std::endl;

    // Peak-to-peak with all same values
    result = evaluator.eval_str("(peak-to-peak (list 3 3 3 3))");
    assert(result.is_float() && std::abs(result.to_float()) < 1e-9);
    std::cout << "✓ (peak-to-peak (list 3 3 3 3)) => 0.0 (flat signal)" << std::endl;

    // Duty cycle - all high
    result = evaluator.eval_str("(duty-cycle (list 1 1 1 1) 0.5)");
    assert(result.is_float() && std::abs(result.to_float() - 100.0) < 1e-9);
    std::cout << "✓ (duty-cycle (list 1 1 1 1) 0.5) => 100.0 (all high)" << std::endl;

    // Duty cycle - all low
    result = evaluator.eval_str("(duty-cycle (list 0 0 0 0) 0.5)");
    assert(result.is_float() && std::abs(result.to_float()) < 1e-9);
    std::cout << "✓ (duty-cycle (list 0 0 0 0) 0.5) => 0.0 (all low)" << std::endl;

    // Autocorrelation with lag 1
    result = evaluator.eval_str("(autocorrelation (list 1 2 3 4 5) 1)");
    assert(result.is_float());
    double ac = result.to_float();
    assert(ac > 0.5 && ac < 1.1);  // Strong positive correlation
    std::cout << "✓ (autocorrelation (list 1 2 3 4 5) 1) => strong correlation" << std::endl;
}

int main() {
    try {
        test_signal_filtering();
        test_signal_analysis();

        std::cout << "\n" << std::string(58, '=') << std::endl;
        std::cout << "🎉 ALL PHASE 5 TESTS PASSED! (8 operators)" << std::endl;
        std::cout << std::string(58, '=') << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
