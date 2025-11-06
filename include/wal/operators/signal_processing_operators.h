#pragma once

#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <memory>

namespace wal::operators {

/// SignalProcessingOperators - Filtering and signal manipulation
/// 
/// Operators: gaussian-blur, median-filter, derivative, integral, rise-time, fall-time, pulse-width, thd
class SignalProcessingOperators {
public:
    static void register_all(OperatorRegistry& registry) {
        // ═══════════════════════════════════════════════════════════════
        // ADVANCED FILTERING (4 operators)
        // ═══════════════════════════════════════════════════════════════

        // gaussian-blur: Gaussian smoothing filter
        registry.register_operator("gaussian-blur", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) throw EvalError("gaussian-blur expects 2 arguments");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("gaussian-blur requires a list");
            }
            
            double sigma = 0;
            if (signals[1].is_int()) {
                sigma = static_cast<double>(signals[1].to_int());
            } else if (signals[1].is_float()) {
                sigma = signals[1].to_float();
            } else {
                throw EvalError("gaussian-blur: sigma must be numeric");
            }
            
            if (sigma <= 0) throw EvalError("gaussian-blur: sigma must be positive");
            
            auto list = signals[0].to_list();
            if (list->empty()) throw EvalError("gaussian-blur: empty signal");
            
            std::vector<double> signal;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    signal.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    signal.push_back(v.to_float());
                } else {
                    throw EvalError("gaussian-blur: all elements must be numeric");
                }
            }
            
            size_t kernel_size = static_cast<size_t>(std::ceil(6 * sigma));
            if (kernel_size % 2 == 0) kernel_size++;
            if (kernel_size > signal.size()) kernel_size = signal.size();
            
            std::vector<double> kernel(kernel_size);
            double sum = 0;
            int center = kernel_size / 2;
            
            for (size_t i = 0; i < kernel_size; ++i) {
                int x = static_cast<int>(i) - center;
                kernel[i] = std::exp(-0.5 * x * x / (sigma * sigma));
                sum += kernel[i];
            }
            
            for (auto& k : kernel) k /= sum;
            
            std::vector<double> result(signal.size(), 0);
            int offset = center;
            
            for (size_t i = 0; i < signal.size(); ++i) {
                for (size_t j = 0; j < kernel_size; ++j) {
                    int idx = static_cast<int>(i) - offset + static_cast<int>(j);
                    if (idx >= 0 && idx < static_cast<int>(signal.size())) {
                        result[i] += signal[idx] * kernel[j];
                    }
                }
            }
            
            auto result_list = std::make_shared<WList>();
            for (double r : result) {
                result_list->push_back(Value(r));
            }
            
            return Value(result_list);
        });

        // median-filter: Median filtering (nonlinear smoothing)
        registry.register_operator("median-filter", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) throw EvalError("median-filter expects 2 arguments");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("median-filter requires a list");
            }
            
            int64_t window_size = 0;
            if (signals[1].is_int()) {
                window_size = signals[1].to_int();
            } else {
                throw EvalError("median-filter: window size must be an integer");
            }
            
            if (window_size <= 0) throw EvalError("median-filter: window size must be positive");
            if (window_size % 2 == 0) window_size++;
            
            auto list = signals[0].to_list();
            if (list->empty()) throw EvalError("median-filter: empty signal");
            
            std::vector<double> signal;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    signal.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    signal.push_back(v.to_float());
                } else {
                    throw EvalError("median-filter: all elements must be numeric");
                }
            }
            
            std::vector<double> result;
            int64_t half_window = window_size / 2;
            
            for (size_t i = 0; i < signal.size(); ++i) {
                std::vector<double> window;
                for (int64_t j = -half_window; j <= half_window; ++j) {
                    int64_t idx = static_cast<int64_t>(i) + j;
                    if (idx >= 0 && idx < static_cast<int64_t>(signal.size())) {
                        window.push_back(signal[idx]);
                    }
                }
                std::sort(window.begin(), window.end());
                result.push_back(window[window.size() / 2]);
            }
            
            auto result_list = std::make_shared<WList>();
            for (double r : result) {
                result_list->push_back(Value(r));
            }
            
            return Value(result_list);
        });

        // derivative: Numerical derivative (first difference)
        registry.register_operator("derivative", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("derivative expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("derivative requires a list");
            }
            
            auto list = signals[0].to_list();
            if (list->size() < 2) throw EvalError("derivative: need at least 2 points");
            
            std::vector<double> signal;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    signal.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    signal.push_back(v.to_float());
                } else {
                    throw EvalError("derivative: all elements must be numeric");
                }
            }
            
            auto result_list = std::make_shared<WList>();
            result_list->push_back(Value(signal[1] - signal[0]));
            
            for (size_t i = 1; i < signal.size() - 1; ++i) {
                result_list->push_back(Value((signal[i + 1] - signal[i - 1]) / 2.0));
            }
            
            result_list->push_back(Value(signal[signal.size() - 1] - signal[signal.size() - 2]));
            
            return Value(result_list);
        });

        // integral: Numerical integration (cumulative sum)
        registry.register_operator("integral", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("integral expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("integral requires a list");
            }
            
            auto list = signals[0].to_list();
            if (list->empty()) throw EvalError("integral: empty signal");
            
            std::vector<double> signal;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    signal.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    signal.push_back(v.to_float());
                } else {
                    throw EvalError("integral: all elements must be numeric");
                }
            }
            
            auto result_list = std::make_shared<WList>();
            double cumsum = signal[0];
            result_list->push_back(Value(cumsum));
            
            for (size_t i = 1; i < signal.size(); ++i) {
                cumsum += signal[i];
                result_list->push_back(Value(cumsum));
            }
            
            return Value(result_list);
        });

        // ═══════════════════════════════════════════════════════════════
        // WAVEFORM ANALYSIS (5 operators)
        // ═══════════════════════════════════════════════════════════════

        // rise-time: Time to go from 10% to 90% of max change
        registry.register_operator("rise-time", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("rise-time expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("rise-time requires a list");
            }
            
            auto list = signals[0].to_list();
            if (list->size() < 2) throw EvalError("rise-time: need at least 2 points");
            
            std::vector<double> signal;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    signal.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    signal.push_back(v.to_float());
                } else {
                    throw EvalError("rise-time: all elements must be numeric");
                }
            }
            
            double min_val = *std::min_element(signal.begin(), signal.end());
            double max_val = *std::max_element(signal.begin(), signal.end());
            double range = max_val - min_val;
            
            if (range < 1e-10) throw EvalError("rise-time: zero range");
            
            double level_10 = min_val + 0.1 * range;
            double level_90 = min_val + 0.9 * range;
            
            int idx_10 = -1, idx_90 = -1;
            for (size_t i = 0; i < signal.size(); ++i) {
                if (idx_10 < 0 && signal[i] >= level_10) idx_10 = i;
                if (signal[i] >= level_90) idx_90 = i;
            }
            
            if (idx_10 < 0 || idx_90 < 0) return Value(-1.0);
            
            return Value(static_cast<double>(idx_90 - idx_10));
        });

        // fall-time: Time to go from 90% to 10% of max change
        registry.register_operator("fall-time", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("fall-time expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("fall-time requires a list");
            }
            
            auto list = signals[0].to_list();
            if (list->size() < 2) throw EvalError("fall-time: need at least 2 points");
            
            std::vector<double> signal;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    signal.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    signal.push_back(v.to_float());
                } else {
                    throw EvalError("fall-time: all elements must be numeric");
                }
            }
            
            double min_val = *std::min_element(signal.begin(), signal.end());
            double max_val = *std::max_element(signal.begin(), signal.end());
            double range = max_val - min_val;
            
            if (range < 1e-10) throw EvalError("fall-time: zero range");
            
            double level_90 = min_val + 0.9 * range;
            double level_10 = min_val + 0.1 * range;
            
            int idx_90 = -1, idx_10 = -1;
            for (int i = static_cast<int>(signal.size()) - 1; i >= 0; --i) {
                if (idx_10 < 0 && signal[i] <= level_10) idx_10 = i;
                if (signal[i] <= level_90) idx_90 = i;
            }
            
            if (idx_90 < 0 || idx_10 < 0) return Value(-1.0);
            
            return Value(static_cast<double>(idx_90 - idx_10));
        });

        // pulse-width: Width of high pulse (samples above 50% threshold)
        registry.register_operator("pulse-width", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("pulse-width expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("pulse-width requires a list");
            }
            
            auto list = signals[0].to_list();
            if (list->empty()) throw EvalError("pulse-width: empty signal");
            
            std::vector<double> signal;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    signal.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    signal.push_back(v.to_float());
                } else {
                    throw EvalError("pulse-width: all elements must be numeric");
                }
            }
            
            double min_val = *std::min_element(signal.begin(), signal.end());
            double max_val = *std::max_element(signal.begin(), signal.end());
            double threshold = (min_val + max_val) / 2.0;
            
            int width = 0;
            bool in_pulse = false;
            
            for (double s : signal) {
                if (s >= threshold && !in_pulse) {
                    in_pulse = true;
                    width = 1;
                } else if (s >= threshold && in_pulse) {
                    width++;
                } else if (s < threshold && in_pulse) {
                    in_pulse = false;
                }
            }
            
            return Value(static_cast<double>(width));
        });

        // thd: Total Harmonic Distortion
        registry.register_operator("thd", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("thd expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("thd requires a list");
            }
            
            auto list = signals[0].to_list();
            if (list->size() < 3) throw EvalError("thd: need at least 3 points");
            
            std::vector<double> signal;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    signal.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    signal.push_back(v.to_float());
                } else {
                    throw EvalError("thd: all elements must be numeric");
                }
            }
            
            size_t n = signal.size();
            
            double f1_real = 0, f1_imag = 0;
            for (size_t t = 0; t < n; ++t) {
                double angle = -2.0 * M_PI * t / n;
                f1_real += signal[t] * std::cos(angle);
                f1_imag += signal[t] * std::sin(angle);
            }
            double f1_mag = std::sqrt(f1_real * f1_real + f1_imag * f1_imag);
            
            double harmonics_power = 0;
            for (size_t k = 2; k < n / 2; ++k) {
                double real_part = 0, imag_part = 0;
                for (size_t t = 0; t < n; ++t) {
                    double angle = -2.0 * M_PI * k * t / n;
                    real_part += signal[t] * std::cos(angle);
                    imag_part += signal[t] * std::sin(angle);
                }
                double mag = std::sqrt(real_part * real_part + imag_part * imag_part);
                harmonics_power += mag * mag;
            }
            
            if (f1_mag < 1e-10) throw EvalError("thd: fundamental too small");
            
            double thd_val = std::sqrt(harmonics_power) / f1_mag * 100.0;
            return Value(thd_val);
        });
    }
};

}  // namespace wal::operators
