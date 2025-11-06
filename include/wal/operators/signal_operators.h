#pragma once

#include "wal/ast.h"
#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include <functional>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <limits>
#include <vector>

namespace wal::operators {

/**
 * @brief Signal processing operators
 * 
 * Implements signal analysis:
 * - moving-average - Smooth signal
 * - zero-crossings - Detect sign changes
 * - rms, peak-to-peak, crest-factor
 * - edges - Detect rising/falling edges
 * - autocorrelation - Correlation analysis
 * - duty-cycle - Estimate duty cycle
 */
class SignalOperators {
public:
    static void register_all(wal::OperatorRegistry& registry) {
        // moving-average - Smooth a signal with moving average window
        registry.register_operator("moving-average", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("moving-average expects 2 arguments (signal window-size), got " + std::to_string(args.size()));
            }

            auto evaluated = eval.eval_args(args);
            if (!evaluated[0].is_list()) {
                throw EvalError("moving-average expects a list as first argument");
            }
            if (!evaluated[1].is_int()) {
                throw EvalError("moving-average expects integer window size");
            }

            const auto& list = evaluated[0].to_list();
            int64_t window_size = evaluated[1].to_int();

            if (window_size <= 0 || window_size > static_cast<int64_t>(list->size())) {
                throw EvalError("moving-average window size must be positive and <= list size");
            }

            std::vector<Value> result;
            for (size_t i = 0; i + window_size <= list->size(); ++i) {
                double sum = 0.0;
                for (int64_t j = 0; j < window_size; ++j) {
                    const auto& elem = (*list)[i + j];
                    sum += elem.is_float() ? elem.to_float() : static_cast<double>(elem.to_int());
                }
                result.push_back(Value(sum / window_size));
            }

            return Value(std::make_shared<WList>(result));
        });

        // zero-crossings - Count transitions from negative to positive or vice versa
        registry.register_operator("zero-crossings", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("zero-crossings expects 1 argument, got " + std::to_string(args.size()));
            }

            const auto& list_val = eval.eval(args[0]);
            if (!list_val.is_list()) {
                throw EvalError("zero-crossings expects a list");
            }

            const auto& list = list_val.to_list();
            if (list->size() < 2) {
                return Value(int64_t(0));
            }

            int64_t crossings = 0;
            double prev = 0.0;
            
            for (const auto& elem : *list) {
                double curr = elem.is_float() ? elem.to_float() : static_cast<double>(elem.to_int());
                
                // Check if sign changed
                if ((prev < 0 && curr > 0) || (prev > 0 && curr < 0)) {
                    crossings++;
                }
                prev = curr;
            }

            return Value(crossings);
        });

        // rms - Calculate root mean square of signal
        registry.register_operator("rms", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("rms expects 1 argument, got " + std::to_string(args.size()));
            }

            const auto& list_val = eval.eval(args[0]);
            if (!list_val.is_list()) {
                throw EvalError("rms expects a list");
            }

            const auto& list = list_val.to_list();
            if (list->empty()) {
                throw EvalError("rms requires non-empty list");
            }

            double sum_sq = 0.0;
            for (const auto& elem : *list) {
                double val = elem.is_float() ? elem.to_float() : static_cast<double>(elem.to_int());
                sum_sq += val * val;
            }

            return Value(std::sqrt(sum_sq / static_cast<double>(list->size())));
        });

        // peak-to-peak - Calculate peak-to-peak amplitude
        registry.register_operator("peak-to-peak", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("peak-to-peak expects 1 argument, got " + std::to_string(args.size()));
            }

            const auto& list_val = eval.eval(args[0]);
            if (!list_val.is_list()) {
                throw EvalError("peak-to-peak expects a list");
            }

            const auto& list = list_val.to_list();
            if (list->empty()) {
                throw EvalError("peak-to-peak requires non-empty list");
            }

            double min_val = std::numeric_limits<double>::infinity();
            double max_val = -std::numeric_limits<double>::infinity();

            for (const auto& elem : *list) {
                double val = elem.is_float() ? elem.to_float() : static_cast<double>(elem.to_int());
                min_val = std::min(min_val, val);
                max_val = std::max(max_val, val);
            }

            return Value(max_val - min_val);
        });

        // crest-factor - Ratio of peak amplitude to RMS value
        registry.register_operator("crest-factor", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("crest-factor expects 1 argument, got " + std::to_string(args.size()));
            }

            const auto& list_val = eval.eval(args[0]);
            if (!list_val.is_list()) {
                throw EvalError("crest-factor expects a list");
            }

            const auto& list = list_val.to_list();
            if (list->empty()) {
                throw EvalError("crest-factor requires non-empty list");
            }

            // Calculate RMS
            double sum_sq = 0.0;
            double peak = 0.0;
            
            for (const auto& elem : *list) {
                double val = elem.is_float() ? elem.to_float() : static_cast<double>(elem.to_int());
                sum_sq += val * val;
                peak = std::max(peak, std::abs(val));
            }

            double rms = std::sqrt(sum_sq / static_cast<double>(list->size()));
            if (rms < 1e-10) {
                throw EvalError("crest-factor: RMS value too small (division by near-zero)");
            }

            return Value(peak / rms);
        });

        // edges - Find rising and falling edges in signal
        registry.register_operator("edges", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) {
                throw EvalError("edges expects 2 arguments (signal threshold), got " + std::to_string(args.size()));
            }

            auto evaluated = eval.eval_args(args);
            if (!evaluated[0].is_list()) {
                throw EvalError("edges expects a list as first argument");
            }

            double threshold = evaluated[1].is_float() ? evaluated[1].to_float() : static_cast<double>(evaluated[1].to_int());
            const auto& list = evaluated[0].to_list();

            std::vector<Value> edges_list;
            bool was_high = false;

            for (size_t i = 0; i < list->size(); ++i) {
                double val = (*list)[i].is_float() ? (*list)[i].to_float() : static_cast<double>((*list)[i].to_int());
                bool is_high = val > threshold;

                if (is_high && !was_high) {
                    // Rising edge
                    std::string rising_str("rising");
                    auto edge = std::make_shared<WList>(std::vector<Value>{
                        Value(std::make_shared<Symbol>(rising_str)),
                        Value(static_cast<int64_t>(i))
                    });
                    edges_list.push_back(Value(edge));
                } else if (!is_high && was_high) {
                    // Falling edge
                    std::string falling_str("falling");
                    auto edge = std::make_shared<WList>(std::vector<Value>{
                        Value(std::make_shared<Symbol>(falling_str)),
                        Value(static_cast<int64_t>(i))
                    });
                    edges_list.push_back(Value(edge));
                }
                was_high = is_high;
            }

            return Value(std::make_shared<WList>(edges_list));
        });

        // autocorrelation - Compute autocorrelation for given lag
        registry.register_operator("autocorrelation", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("autocorrelation expects 2 arguments (signal lag), got " + std::to_string(args.size()));
            }

            auto evaluated = eval.eval_args(args);
            if (!evaluated[0].is_list()) {
                throw EvalError("autocorrelation expects a list as first argument");
            }
            if (!evaluated[1].is_int()) {
                throw EvalError("autocorrelation expects integer lag");
            }

            const auto& list = evaluated[0].to_list();
            int64_t lag = evaluated[1].to_int();

            if (lag < 0 || lag >= static_cast<int64_t>(list->size())) {
                throw EvalError("autocorrelation lag must be non-negative and < list size");
            }

            // Calculate mean
            double sum = 0.0;
            for (const auto& elem : *list) {
                sum += elem.is_float() ? elem.to_float() : static_cast<double>(elem.to_int());
            }
            double mean = sum / static_cast<double>(list->size());

            // Calculate autocorrelation at given lag
            double numerator = 0.0;
            double denominator = 0.0;

            for (size_t i = 0; i + lag < list->size(); ++i) {
                double x1 = (*list)[i].is_float() ? (*list)[i].to_float() : static_cast<double>((*list)[i].to_int());
                double x2 = (*list)[i + lag].is_float() ? (*list)[i + lag].to_float() : static_cast<double>((*list)[i + lag].to_int());
                
                numerator += (x1 - mean) * (x2 - mean);
                denominator += (x1 - mean) * (x1 - mean);
            }

            if (std::abs(denominator) < 1e-10) {
                throw EvalError("autocorrelation: signal variance too small");
            }

            return Value(numerator / denominator);
        });

        // duty-cycle - Estimate duty cycle for periodic signal above threshold
        registry.register_operator("duty-cycle", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) {
                throw EvalError("duty-cycle expects 2 arguments (signal threshold), got " + std::to_string(args.size()));
            }

            auto evaluated = eval.eval_args(args);
            if (!evaluated[0].is_list()) {
                throw EvalError("duty-cycle expects a list as first argument");
            }

            double threshold = evaluated[1].is_float() ? evaluated[1].to_float() : static_cast<double>(evaluated[1].to_int());
            const auto& list = evaluated[0].to_list();

            if (list->empty()) {
                throw EvalError("duty-cycle requires non-empty list");
            }

            int64_t high_count = 0;
            for (const auto& elem : *list) {
                double val = elem.is_float() ? elem.to_float() : static_cast<double>(elem.to_int());
                if (val > threshold) {
                    high_count++;
                }
            }

            double duty = (100.0 * high_count) / static_cast<double>(list->size());
            return Value(duty);
        });
    }
};

} // namespace wal::operators

