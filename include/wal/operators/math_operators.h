#pragma once

#include "wal/ast.h"
#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include <functional>
#include <cmath>
#include <limits>
#include <vector>

namespace wal::operators {

/**
 * @brief Advanced mathematical operators
 * 
 * Implements trigonometric and mathematical functions:
 * - sin, cos, tan, asin, acos, atan
 * - sqrt, log, log10, exp
 * - abs, min, max
 */
class MathOperators {
public:
    static void register_all(wal::OperatorRegistry& registry) {
        registry.register_operator("sin", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("sin requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_number()) {
                throw EvalError("sin: argument must be a number");
            }
            
            double x = evaluated[0].is_float() ? evaluated[0].to_float() : 
                       static_cast<double>(evaluated[0].to_int());
            return Value(std::sin(x));
        });
        
        registry.register_operator("cos", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("cos requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_number()) {
                throw EvalError("cos: argument must be a number");
            }
            
            double x = evaluated[0].is_float() ? evaluated[0].to_float() : 
                       static_cast<double>(evaluated[0].to_int());
            return Value(std::cos(x));
        });
        
        registry.register_operator("tan", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("tan requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_number()) {
                throw EvalError("tan: argument must be a number");
            }
            
            double x = evaluated[0].is_float() ? evaluated[0].to_float() : 
                       static_cast<double>(evaluated[0].to_int());
            return Value(std::tan(x));
        });
        
        registry.register_operator("asin", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("asin requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_number()) {
                throw EvalError("asin: argument must be a number");
            }
            
            double x = evaluated[0].is_float() ? evaluated[0].to_float() : 
                       static_cast<double>(evaluated[0].to_int());
            
            if (x < -1.0 || x > 1.0) {
                throw EvalError("asin: argument must be in range [-1, 1]");
            }
            
            return Value(std::asin(x));
        });
        
        registry.register_operator("acos", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("acos requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_number()) {
                throw EvalError("acos: argument must be a number");
            }
            
            double x = evaluated[0].is_float() ? evaluated[0].to_float() : 
                       static_cast<double>(evaluated[0].to_int());
            
            if (x < -1.0 || x > 1.0) {
                throw EvalError("acos: argument must be in range [-1, 1]");
            }
            
            return Value(std::acos(x));
        });
        
        registry.register_operator("atan", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1 && args.size() != 2) {
                throw EvalError("atan requires 1 or 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            
            if (args.size() == 1) {
                if (!evaluated[0].is_number()) {
                    throw EvalError("atan: argument must be a number");
                }
                double x = evaluated[0].is_float() ? evaluated[0].to_float() : 
                           static_cast<double>(evaluated[0].to_int());
                return Value(std::atan(x));
            } else {
                // atan2(y, x)
                if (!evaluated[0].is_number() || !evaluated[1].is_number()) {
                    throw EvalError("atan: arguments must be numbers");
                }
                double y = evaluated[0].is_float() ? evaluated[0].to_float() : 
                           static_cast<double>(evaluated[0].to_int());
                double x = evaluated[1].is_float() ? evaluated[1].to_float() : 
                           static_cast<double>(evaluated[1].to_int());
                return Value(std::atan2(y, x));
            }
        });
        
        registry.register_operator("sqrt", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("sqrt requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_number()) {
                throw EvalError("sqrt: argument must be a number");
            }
            
            double x = evaluated[0].is_float() ? evaluated[0].to_float() : 
                       static_cast<double>(evaluated[0].to_int());
            
            if (x < 0.0) {
                throw EvalError("sqrt: argument cannot be negative");
            }
            
            return Value(std::sqrt(x));
        });
        
        registry.register_operator("log", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("log requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_number()) {
                throw EvalError("log: argument must be a number");
            }
            
            double x = evaluated[0].is_float() ? evaluated[0].to_float() : 
                       static_cast<double>(evaluated[0].to_int());
            
            if (x <= 0.0) {
                throw EvalError("log: argument must be positive");
            }
            
            return Value(std::log(x));
        });
        
        registry.register_operator("log10", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("log10 requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_number()) {
                throw EvalError("log10: argument must be a number");
            }
            
            double x = evaluated[0].is_float() ? evaluated[0].to_float() : 
                       static_cast<double>(evaluated[0].to_int());
            
            if (x <= 0.0) {
                throw EvalError("log10: argument must be positive");
            }
            
            return Value(std::log10(x));
        });
        
        registry.register_operator("exp", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("exp requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_number()) {
                throw EvalError("exp: argument must be a number");
            }
            
            double x = evaluated[0].is_float() ? evaluated[0].to_float() : 
                       static_cast<double>(evaluated[0].to_int());
            return Value(std::exp(x));
        });
        
        registry.register_operator("abs", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("abs requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_number()) {
                throw EvalError("abs: argument must be a number");
            }
            
            if (evaluated[0].is_float()) {
                return Value(std::abs(evaluated[0].to_float()));
            } else {
                return Value(std::abs(evaluated[0].to_int()));
            }
        });
        
        registry.register_operator("min", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("min requires at least 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            // Check all arguments are numbers
            for (const auto& val : evaluated) {
                if (!val.is_number()) {
                    throw EvalError("min: all arguments must be numbers");
                }
            }
            
            // Convert all to doubles for comparison
            double min_val = std::numeric_limits<double>::infinity();
            bool is_float_result = false;
            
            for (const auto& val : evaluated) {
                double d = val.is_float() ? val.to_float() : static_cast<double>(val.to_int());
                if (val.is_float()) is_float_result = true;
                if (d < min_val) min_val = d;
            }
            
            if (is_float_result) {
                return Value(min_val);
            } else {
                return Value(static_cast<int64_t>(min_val));
            }
        });
        
        registry.register_operator("max", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("max requires at least 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            // Check all arguments are numbers
            for (const auto& val : evaluated) {
                if (!val.is_number()) {
                    throw EvalError("max: all arguments must be numbers");
                }
            }
            
            // Convert all to doubles for comparison
            double max_val = -std::numeric_limits<double>::infinity();
            bool is_float_result = false;
            
            for (const auto& val : evaluated) {
                double d = val.is_float() ? val.to_float() : static_cast<double>(val.to_int());
                if (val.is_float()) is_float_result = true;
                if (d > max_val) max_val = d;
            }
            
            if (is_float_result) {
                return Value(max_val);
            } else {
                return Value(static_cast<int64_t>(max_val));
            }
        });
    }
};

} // namespace wal::operators
