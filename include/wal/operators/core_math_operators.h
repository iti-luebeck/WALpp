#pragma once

#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include <cmath>
#include <iostream>

namespace wal::operators {

/**
 * @brief Core foundation operators (Phase 1)
 * 
 * Implements fundamental operators:
 * - Arithmetic: +, -, *, /, **, %, floor, ceil, round, abs, min, max
 * - Comparison: =, !=, <, >, <=, >=
 * - Logic: &&, ||, !
 * - List: list
 * - I/O: print
 * 
 * All implementations are header-only for compiler optimization.
 */
class CoreMathOperators {
public:
    /**
     * @brief Register all core math operators
     */
    static void register_all(OperatorRegistry& registry) {
        // ═══════════════════════════════════════════════════════════════
        // ARITHMETIC OPERATORS
        // ═══════════════════════════════════════════════════════════════
        
        registry.register_operator("+", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) return Value(0);
            auto evaluated = eval.eval_args(args);
            
            double result = 0.0;
            bool is_int = true;
            
            for (const auto& val : evaluated) {
                if (val.is_float()) is_int = false;
                result += val.to_float();
            }
            
            return is_int ? Value(static_cast<int64_t>(result)) : Value(result);
        });
        
        registry.register_operator("-", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("- requires at least 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (evaluated.size() == 1) {
                if (evaluated[0].is_float()) {
                    return Value(-evaluated[0].to_float());
                } else {
                    return Value(-evaluated[0].to_int());
                }
            }
            
            double result = evaluated[0].to_float();
            bool is_int = evaluated[0].is_int();
            
            for (size_t i = 1; i < evaluated.size(); ++i) {
                if (evaluated[i].is_float()) is_int = false;
                result -= evaluated[i].to_float();
            }
            
            return is_int ? Value(static_cast<int64_t>(result)) : Value(result);
        });
        
        registry.register_operator("*", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) return Value(1);
            auto evaluated = eval.eval_args(args);
            
            double result = 1.0;
            bool is_int = true;
            
            for (const auto& val : evaluated) {
                if (val.is_float()) is_int = false;
                result *= val.to_float();
            }
            
            return is_int ? Value(static_cast<int64_t>(result)) : Value(result);
        });
        
        registry.register_operator("/", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("/ requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            
            double divisor = evaluated[1].to_float();
            if (std::abs(divisor) < 1e-10) {
                throw EvalError("Division by zero");
            }
            
            double result = evaluated[0].to_float() / divisor;
            return Value(result);
        });
        
        registry.register_operator("**", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("** requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            double base = evaluated[0].to_float();
            double exponent = evaluated[1].to_float();
            double result = std::pow(base, exponent);
            return Value(result);
        });
        
        registry.register_operator("%", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("% requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            int64_t a = evaluated[0].to_int();
            int64_t b = evaluated[1].to_int();
            if (b == 0) throw EvalError("Division by zero in %");
            return Value(a % b);
        });
        
        registry.register_operator("floor", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("floor requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            return Value(static_cast<int64_t>(std::floor(evaluated[0].to_float())));
        });
        
        registry.register_operator("ceil", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("ceil requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            return Value(static_cast<int64_t>(std::ceil(evaluated[0].to_float())));
        });
        
        registry.register_operator("round", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("round requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            return Value(static_cast<int64_t>(std::round(evaluated[0].to_float())));
        });
        
        registry.register_operator("abs", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("abs requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            if (evaluated[0].is_int()) {
                return Value(std::abs(evaluated[0].to_int()));
            } else {
                return Value(std::abs(evaluated[0].to_float()));
            }
        });
        
        registry.register_operator("min", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("min requires at least 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            double min_val = evaluated[0].to_float();
            for (size_t i = 1; i < evaluated.size(); ++i) {
                min_val = std::min(min_val, evaluated[i].to_float());
            }
            return Value(min_val);
        });
        
        registry.register_operator("max", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("max requires at least 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            double max_val = evaluated[0].to_float();
            for (size_t i = 1; i < evaluated.size(); ++i) {
                max_val = std::max(max_val, evaluated[i].to_float());
            }
            return Value(max_val);
        });
        
        // ═══════════════════════════════════════════════════════════════
        // COMPARISON OPERATORS
        // ═══════════════════════════════════════════════════════════════
        
        registry.register_operator("=", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) {
                throw EvalError("= requires at least 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            for (size_t i = 1; i < evaluated.size(); ++i) {
                if (!(evaluated[i-1] == evaluated[i])) {
                    return Value(false);
                }
            }
            return Value(true);
        });
        
        registry.register_operator("!=", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("!= requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            return Value(evaluated[0] != evaluated[1]);
        });
        
        registry.register_operator("<", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("< requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            return Value(evaluated[0].to_float() < evaluated[1].to_float());
        });
        
        registry.register_operator(">", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("> requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            return Value(evaluated[0].to_float() > evaluated[1].to_float());
        });
        
        registry.register_operator("<=", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("<= requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            return Value(evaluated[0].to_float() <= evaluated[1].to_float());
        });
        
        registry.register_operator(">=", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError(">= requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            return Value(evaluated[0].to_float() >= evaluated[1].to_float());
        });
        
        // ═══════════════════════════════════════════════════════════════
        // LOGICAL OPERATORS
        // ═══════════════════════════════════════════════════════════════
        
        registry.register_operator("&&", [](SEval& eval, const std::vector<Value>& args) -> Value {
            for (const auto& arg : args) {
                if (!eval.eval(arg).is_truthy()) {
                    return Value(false);
                }
            }
            return Value(true);
        });
        
        registry.register_operator("||", [](SEval& eval, const std::vector<Value>& args) -> Value {
            for (const auto& arg : args) {
                if (eval.eval(arg).is_truthy()) {
                    return Value(true);
                }
            }
            return Value(false);
        });
        
        registry.register_operator("!", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("! requires exactly 1 argument");
            }
            return Value(!eval.eval(args[0]).is_truthy());
        });
        
        // ═══════════════════════════════════════════════════════════════
        // LIST & UTILITY OPERATORS
        // ═══════════════════════════════════════════════════════════════
        
        registry.register_operator("list", [](SEval& eval, const std::vector<Value>& args) -> Value {
            auto evaluated = eval.eval_args(args);
            return Value(std::make_shared<WList>(evaluated));
        });
        
        registry.register_operator("print", [](SEval& eval, const std::vector<Value>& args) -> Value {
            auto evaluated = eval.eval_args(args);
            for (size_t i = 0; i < evaluated.size(); ++i) {
                if (i > 0) std::cout << " ";
                std::cout << evaluated[i].to_string_repr();
            }
            std::cout << "\n";
            return Value(nullptr);
        });
    }
};

} // namespace wal::operators
