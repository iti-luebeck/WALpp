#pragma once

#include "wal/ast.h"
#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include <functional>
#include <algorithm>
#include <vector>
#include <limits>

namespace wal::operators {

/**
 * @brief Comparison and constraint operators
 * 
 * Implements comparison and range operations:
 * - between - Check if value is in range
 * - clamp - Constrain value to range
 * - in? - Check membership in list
 */
class ComparisonOperators {
public:
    static void register_all(wal::OperatorRegistry& registry) {
        // between - Check if value is between two bounds (inclusive)
        registry.register_operator("between", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 3) {
                throw EvalError("between expects 3 arguments, got " + std::to_string(args.size()));
            }

            auto evaluated = eval.eval_args(args);
            const auto& value = evaluated[0];
            const auto& lower = evaluated[1];
            const auto& upper = evaluated[2];

            double v = value.is_float() ? value.to_float() : static_cast<double>(value.to_int());
            double l = lower.is_float() ? lower.to_float() : static_cast<double>(lower.to_int());
            double u = upper.is_float() ? upper.to_float() : static_cast<double>(upper.to_int());

            return Value((v >= l) && (v <= u));
        });

        // clamp - Clamp value to be within bounds
        registry.register_operator("clamp", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 3) {
                throw EvalError("clamp expects 3 arguments, got " + std::to_string(args.size()));
            }

            auto evaluated = eval.eval_args(args);
            const auto& value = evaluated[0];
            const auto& lower = evaluated[1];
            const auto& upper = evaluated[2];

            double v = value.is_float() ? value.to_float() : static_cast<double>(value.to_int());
            double l = lower.is_float() ? lower.to_float() : static_cast<double>(lower.to_int());
            double u = upper.is_float() ? upper.to_float() : static_cast<double>(upper.to_int());

            if (v < l) return lower;
            if (v > u) return upper;
            return value;
        });

        // in? - Check if value is in a list
        registry.register_operator("in?", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("in? expects 2 arguments, got " + std::to_string(args.size()));
            }

            auto evaluated = eval.eval_args(args);
            const auto& value = evaluated[0];
            const auto& list_val = evaluated[1];
            
            if (!list_val.is_list()) {
                throw EvalError("in? expects a list as second argument");
            }

            const auto& list = list_val.to_list();
            for (const auto& elem : *list) {
                if (value == elem) {
                    return Value(true);
                }
            }
            return Value(false);
        });
    }
};

} // namespace wal::operators

