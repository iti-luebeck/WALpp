#pragma once

#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <vector>
#include <memory>
#include <limits>

namespace wal::operators {

/// StatisticalAnalysisOperators - Statistical and distribution analysis
/// 
/// Operators: variance, stddev, median, quartiles, skewness, kurtosis, histogram
class StatisticalAnalysisOperators {
public:
    static void register_all(OperatorRegistry& registry) {
        // ═══════════════════════════════════════════════════════════════
        // STATISTICAL ANALYSIS (7 operators)
        // ═══════════════════════════════════════════════════════════════

        // variance: Population variance
        registry.register_operator("variance", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("variance expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("variance requires a list");
            }
            
            auto list = signals[0].to_list();
            if (list->empty()) throw EvalError("variance: empty signal");
            
            std::vector<double> values;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    values.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    values.push_back(v.to_float());
                } else {
                    throw EvalError("variance: all elements must be numeric");
                }
            }
            
            double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
            double var = 0;
            for (double v : values) {
                var += (v - mean) * (v - mean);
            }
            var /= values.size();
            
            return Value(var);
        });

        // stddev: Standard deviation
        registry.register_operator("stddev", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("stddev expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("stddev requires a list");
            }
            
            auto list = signals[0].to_list();
            if (list->empty()) throw EvalError("stddev: empty signal");
            
            std::vector<double> values;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    values.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    values.push_back(v.to_float());
                } else {
                    throw EvalError("stddev: all elements must be numeric");
                }
            }
            
            double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
            double var = 0;
            for (double v : values) {
                var += (v - mean) * (v - mean);
            }
            var /= values.size();
            
            return Value(std::sqrt(var));
        });

        // median: Median value
        registry.register_operator("median", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("median expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("median requires a list");
            }
            
            auto list = signals[0].to_list();
            if (list->empty()) throw EvalError("median: empty signal");
            
            std::vector<double> values;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    values.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    values.push_back(v.to_float());
                } else {
                    throw EvalError("median: all elements must be numeric");
                }
            }
            
            std::sort(values.begin(), values.end());
            
            if (values.size() % 2 == 1) {
                return Value(values[values.size() / 2]);
            } else {
                return Value((values[values.size() / 2 - 1] + values[values.size() / 2]) / 2.0);
            }
        });

        // quartiles: Returns list of [Q1, Q2/median, Q3]
        registry.register_operator("quartiles", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("quartiles expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("quartiles requires a list");
            }
            
            auto list = signals[0].to_list();
            if (list->empty()) throw EvalError("quartiles: empty signal");
            
            std::vector<double> values;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    values.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    values.push_back(v.to_float());
                } else {
                    throw EvalError("quartiles: all elements must be numeric");
                }
            }
            
            std::sort(values.begin(), values.end());
            size_t n = values.size();
            
            double q1 = (n >= 4) ? values[n / 4] : values[0];
            double q2 = (n % 2 == 1) ? values[n / 2] : (values[n / 2 - 1] + values[n / 2]) / 2.0;
            double q3 = (n >= 4) ? values[3 * n / 4] : values[n - 1];
            
            auto result = std::make_shared<WList>();
            result->push_back(Value(q1));
            result->push_back(Value(q2));
            result->push_back(Value(q3));
            
            return Value(result);
        });

        // skewness: Distribution skewness
        registry.register_operator("skewness", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("skewness expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("skewness requires a list");
            }
            
            auto list = signals[0].to_list();
            if (list->size() < 3) throw EvalError("skewness: needs at least 3 values");
            
            std::vector<double> values;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    values.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    values.push_back(v.to_float());
                } else {
                    throw EvalError("skewness: all elements must be numeric");
                }
            }
            
            double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
            double m2 = 0, m3 = 0;
            
            for (double v : values) {
                double diff = v - mean;
                m2 += diff * diff;
                m3 += diff * diff * diff;
            }
            
            m2 /= values.size();
            m3 /= values.size();
            
            double std_dev = std::sqrt(m2);
            if (std_dev < 1e-10) throw EvalError("skewness: zero variance");
            
            return Value(m3 / (std_dev * std_dev * std_dev));
        });

        // kurtosis: Distribution kurtosis (excess kurtosis)
        registry.register_operator("kurtosis", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("kurtosis expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("kurtosis requires a list");
            }
            
            auto list = signals[0].to_list();
            if (list->size() < 4) throw EvalError("kurtosis: needs at least 4 values");
            
            std::vector<double> values;
            for (const auto& v : *list) {
                if (v.is_int()) {
                    values.push_back(static_cast<double>(v.to_int()));
                } else if (v.is_float()) {
                    values.push_back(v.to_float());
                } else {
                    throw EvalError("kurtosis: all elements must be numeric");
                }
            }
            
            double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
            double m2 = 0, m4 = 0;
            
            for (double v : values) {
                double diff = v - mean;
                m2 += diff * diff;
                m4 += diff * diff * diff * diff;
            }
            
            m2 /= values.size();
            m4 /= values.size();
            
            if (m2 < 1e-10) throw EvalError("kurtosis: zero variance");
            
            return Value((m4 / (m2 * m2)) - 3.0);
        });

        // histogram: Bin signal values into histogram
        registry.register_operator("histogram", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) throw EvalError("histogram expects 2 arguments");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("histogram: first argument must be a list");
            }
            
            int64_t nbins = 0;
            if (signals[1].is_int()) {
                nbins = signals[1].to_int();
            } else {
                throw EvalError("histogram: bin count must be an integer");
            }
            
            if (nbins <= 0) throw EvalError("histogram: bin count must be positive");
            
            auto list = signals[0].to_list();
            if (list->empty()) throw EvalError("histogram: empty signal");
            
            std::vector<double> values;
            double min_val = std::numeric_limits<double>::max();
            double max_val = -std::numeric_limits<double>::max();
            
            for (const auto& v : *list) {
                double dval = 0;
                if (v.is_int()) {
                    dval = static_cast<double>(v.to_int());
                } else if (v.is_float()) {
                    dval = v.to_float();
                } else {
                    throw EvalError("histogram: all elements must be numeric");
                }
                values.push_back(dval);
                min_val = std::min(min_val, dval);
                max_val = std::max(max_val, dval);
            }
            
            std::vector<int64_t> bins(nbins, 0);
            if (min_val < max_val) {
                double bin_width = (max_val - min_val) / nbins;
                for (double v : values) {
                    int64_t bin_idx = static_cast<int64_t>((v - min_val) / bin_width);
                    if (bin_idx >= nbins) bin_idx = nbins - 1;
                    bins[bin_idx]++;
                }
            }
            
            auto result = std::make_shared<WList>();
            for (int64_t count : bins) {
                result->push_back(Value(count));
            }
            
            return Value(result);
        });
    }
};

}  // namespace wal::operators
