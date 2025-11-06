#pragma once

#include "wal/ast.h"
#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include <functional>
#include <numeric>
#include <algorithm>
#include <vector>

namespace wal::operators {

/**
 * @brief Aggregation/statistical operators
 * 
 * Implements aggregation functions:
 * - sum, product - numeric aggregation
 * - mean - average value
 * - min-val, max-val - extreme values
 */
class AggregationOperators {
public:
    static void register_all(wal::OperatorRegistry& registry) {
        // sum - Sum all numeric values in a list
        registry.register_operator("sum", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("sum expects 1 argument, got " + std::to_string(args.size()));
            }

            const auto& list_val = eval.eval(args[0]);
            if (!list_val.is_list()) {
                throw EvalError("sum expects a list");
            }

            const auto& list = list_val.to_list();
            double sum = 0.0;
            bool has_float = false;
            
            for (const auto& elem : *list) {
                if (elem.is_float()) {
                    has_float = true;
                    sum += elem.to_float();
                } else if (elem.is_int()) {
                    sum += static_cast<double>(elem.to_int());
                } else {
                    throw EvalError("sum expects numeric values");
                }
            }
            
            if (has_float) {
                return Value(sum);
            }
            return Value(static_cast<int64_t>(sum));
        });

        // product - Multiply all numeric values in a list
        registry.register_operator("product", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("product expects 1 argument, got " + std::to_string(args.size()));
            }

            const auto& list_val = eval.eval(args[0]);
            if (!list_val.is_list()) {
                throw EvalError("product expects a list");
            }

            const auto& list = list_val.to_list();
            double product = 1.0;
            bool has_float = false;
            
            for (const auto& elem : *list) {
                if (elem.is_float()) {
                    has_float = true;
                    product *= elem.to_float();
                } else if (elem.is_int()) {
                    product *= static_cast<double>(elem.to_int());
                } else {
                    throw EvalError("product expects numeric values");
                }
            }
            
            if (has_float) {
                return Value(product);
            }
            return Value(static_cast<int64_t>(product));
        });

        // mean - Calculate average of numeric values
        registry.register_operator("mean", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("mean expects 1 argument, got " + std::to_string(args.size()));
            }

            const auto& list_val = eval.eval(args[0]);
            if (!list_val.is_list()) {
                throw EvalError("mean expects a list");
            }

            const auto& list = list_val.to_list();
            if (list->empty()) {
                throw EvalError("mean requires non-empty list");
            }

            double sum = 0.0;
            for (const auto& elem : *list) {
                if (elem.is_float()) {
                    sum += elem.to_float();
                } else if (elem.is_int()) {
                    sum += static_cast<double>(elem.to_int());
                } else {
                    throw EvalError("mean expects numeric values");
                }
            }
            
            return Value(sum / static_cast<double>(list->size()));
        });

        // min-val - Find minimum value in list  
        registry.register_operator("min-val", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("min-val expects 1 argument, got " + std::to_string(args.size()));
            }

            const auto& list_val = eval.eval(args[0]);
            if (!list_val.is_list()) {
                throw EvalError("min-val expects a list");
            }

            const auto& list = list_val.to_list();
            if (list->empty()) {
                throw EvalError("min-val requires non-empty list");
            }

            Value min_val = (*list)[0];
            for (size_t i = 1; i < list->size(); ++i) {
                const auto& elem = (*list)[i];
                if (elem.is_float() && min_val.is_float()) {
                    if (elem.to_float() < min_val.to_float()) min_val = elem;
                } else {
                    double a = elem.is_float() ? elem.to_float() : static_cast<double>(elem.to_int());
                    double b = min_val.is_float() ? min_val.to_float() : static_cast<double>(min_val.to_int());
                    if (a < b) min_val = elem;
                }
            }
            return min_val;
        });

        // max-val - Find maximum value in list
        registry.register_operator("max-val", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("max-val expects 1 argument, got " + std::to_string(args.size()));
            }

            const auto& list_val = eval.eval(args[0]);
            if (!list_val.is_list()) {
                throw EvalError("max-val expects a list");
            }

            const auto& list = list_val.to_list();
            if (list->empty()) {
                throw EvalError("max-val requires non-empty list");
            }

            Value max_val = (*list)[0];
            for (size_t i = 1; i < list->size(); ++i) {
                const auto& elem = (*list)[i];
                if (elem.is_float() && max_val.is_float()) {
                    if (elem.to_float() > max_val.to_float()) max_val = elem;
                } else {
                    double a = elem.is_float() ? elem.to_float() : static_cast<double>(elem.to_int());
                    double b = max_val.is_float() ? max_val.to_float() : static_cast<double>(max_val.to_int());
                    if (a > b) max_val = elem;
                }
            }
            return max_val;
        });
    }
};

} // namespace wal::operators

