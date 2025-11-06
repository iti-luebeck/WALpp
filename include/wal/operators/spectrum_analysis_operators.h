#pragma once

#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <memory>

namespace wal::operators {

/// SpectrumAnalysisOperators - FFT and frequency domain analysis
/// 
/// Operators: frequency-spectrum, magnitude-spectrum, phase-spectrum, peak-frequency
class SpectrumAnalysisOperators {
public:
    static void register_all(OperatorRegistry& registry) {
        // ═══════════════════════════════════════════════════════════════
        // SPECTRUM ANALYSIS (4 operators)
        // ═══════════════════════════════════════════════════════════════

        // frequency-spectrum: FFT magnitude spectrum (decimated for efficiency)
        registry.register_operator("frequency-spectrum", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("frequency-spectrum expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("frequency-spectrum requires a list");
            }
            
            auto list = signals[0].to_list();
            if (list->empty()) throw EvalError("frequency-spectrum: empty signal");
            
            std::vector<double> signal;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    signal.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    signal.push_back(v.to_float());
                } else {
                    throw EvalError("frequency-spectrum: all elements must be numeric");
                }
            }
            
            size_t n = signal.size();
            auto result_list = std::make_shared<WList>();
            
            for (size_t k = 0; k < n / 2; ++k) {
                double real_part = 0, imag_part = 0;
                for (size_t t = 0; t < n; ++t) {
                    double angle = -2.0 * M_PI * k * t / n;
                    real_part += signal[t] * std::cos(angle);
                    imag_part += signal[t] * std::sin(angle);
                }
                double magnitude = std::sqrt(real_part * real_part + imag_part * imag_part) / n;
                result_list->push_back(Value(magnitude));
            }
            
            return Value(result_list);
        });

        // magnitude-spectrum: Normalized magnitude spectrum
        registry.register_operator("magnitude-spectrum", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("magnitude-spectrum expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("magnitude-spectrum requires a list");
            }
            
            auto list = signals[0].to_list();
            if (list->empty()) throw EvalError("magnitude-spectrum: empty signal");
            
            std::vector<double> signal;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    signal.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    signal.push_back(v.to_float());
                } else {
                    throw EvalError("magnitude-spectrum: all elements must be numeric");
                }
            }
            
            size_t n = signal.size();
            std::vector<double> magnitude;
            double max_mag = 0;
            
            for (size_t k = 0; k < n / 2; ++k) {
                double real_part = 0, imag_part = 0;
                for (size_t t = 0; t < n; ++t) {
                    double angle = -2.0 * M_PI * k * t / n;
                    real_part += signal[t] * std::cos(angle);
                    imag_part += signal[t] * std::sin(angle);
                }
                double mag = std::sqrt(real_part * real_part + imag_part * imag_part) / n;
                magnitude.push_back(mag);
                if (mag > max_mag) max_mag = mag;
            }
            
            auto result_list = std::make_shared<WList>();
            if (max_mag > 0) {
                for (double m : magnitude) {
                    result_list->push_back(Value(m / max_mag));
                }
            } else {
                for (double m : magnitude) {
                    result_list->push_back(Value(m));
                }
            }
            
            return Value(result_list);
        });

        // phase-spectrum: Phase spectrum in radians
        registry.register_operator("phase-spectrum", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("phase-spectrum expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("phase-spectrum requires a list");
            }
            
            auto list = signals[0].to_list();
            if (list->empty()) throw EvalError("phase-spectrum: empty signal");
            
            std::vector<double> signal;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    signal.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    signal.push_back(v.to_float());
                } else {
                    throw EvalError("phase-spectrum: all elements must be numeric");
                }
            }
            
            size_t n = signal.size();
            auto result_list = std::make_shared<WList>();
            
            for (size_t k = 0; k < n / 2; ++k) {
                double real_part = 0, imag_part = 0;
                for (size_t t = 0; t < n; ++t) {
                    double angle = -2.0 * M_PI * k * t / n;
                    real_part += signal[t] * std::cos(angle);
                    imag_part += signal[t] * std::sin(angle);
                }
                double ph = std::atan2(imag_part, real_part);
                result_list->push_back(Value(ph));
            }
            
            return Value(result_list);
        });

        // peak-frequency: Dominant frequency from spectrum
        registry.register_operator("peak-frequency", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) throw EvalError("peak-frequency expects 2 arguments");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("peak-frequency: first argument must be a list");
            }
            
            double sr = 0;
            if (signals[1].is_int()) {
                sr = static_cast<double>(signals[1].to_int());
            } else if (signals[1].is_float()) {
                sr = signals[1].to_float();
            } else {
                throw EvalError("peak-frequency: sample rate must be numeric");
            }
            
            if (sr <= 0) throw EvalError("peak-frequency: sample rate must be positive");
            
            auto list = signals[0].to_list();
            if (list->empty()) throw EvalError("peak-frequency: empty signal");
            
            std::vector<double> signal;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    signal.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    signal.push_back(v.to_float());
                } else {
                    throw EvalError("peak-frequency: all elements must be numeric");
                }
            }
            
            size_t n = signal.size();
            double max_mag = 0;
            size_t peak_k = 0;
            
            for (size_t k = 1; k < n / 2; ++k) {
                double real_part = 0, imag_part = 0;
                for (size_t t = 0; t < n; ++t) {
                    double angle = -2.0 * M_PI * k * t / n;
                    real_part += signal[t] * std::cos(angle);
                    imag_part += signal[t] * std::sin(angle);
                }
                double mag = std::sqrt(real_part * real_part + imag_part * imag_part);
                if (mag > max_mag) {
                    max_mag = mag;
                    peak_k = k;
                }
            }
            
            double freq = (static_cast<double>(peak_k) * sr) / n;
            return Value(freq);
        });
    }
};

}  // namespace wal::operators
