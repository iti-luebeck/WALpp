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
namespace bigint_detail {

/// Exact arithmetic when any operand is a bigint. Mixing with a float is still done
/// in double, since a float has already lost the precision a bigint is there to keep.
inline bool any_bigint(const std::vector<Value>& vals) {
    for (const auto& v : vals) {
        if (v.is_bigint()) return true;
    }
    return false;
}

inline BigInt as_bigint(const Value& v) {
    if (v.is_bigint()) return *v.to_bigint();
    if (v.is_int()) return BigInt(v.to_int());
    throw EvalError("Expected integer, got " + v.to_string_repr());
}

/// Give back a plain int when the result fits, so a bigint does not spread through a
/// computation that no longer needs one.
inline Value narrow(const BigInt& b) {
    if (b.fits_int64()) return Value(b.to_int64());
    return Value(std::make_shared<BigInt>(b));
}

} // namespace bigint_detail

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

            // Upstream's + is polymorphic. When the FIRST operand is a list it builds a
            // list: another list is concatenated, anything else is appended as one
            // element. The standard library's `append`, `reverse` and `filter` are all
            // written in terms of this, so without it they cannot run.
            //
            // Only the list half is adopted. Upstream also concatenates when an operand
            // is a STRING, so `(+ 1 "hi")` is "1hi" there and an error here. That stays
            // an error deliberately: the VCD reader represents a signal wider than 64
            // bits as a bit STRING, so string-coercing + would silently turn
            // `(+ wide 1)` into concatenation of a bit pattern instead of failing. A
            // list is never mistakable for a number, so this half carries no such risk.
            if (bigint_detail::any_bigint(evaluated)) {
                BigInt total = bigint_detail::as_bigint(evaluated[0]);
                for (size_t i = 1; i < evaluated.size(); ++i) {
                    total = total + bigint_detail::as_bigint(evaluated[i]);
                }
                return bigint_detail::narrow(total);
            }

            if (evaluated[0].is_list()) {
                auto result = std::make_shared<WList>(*evaluated[0].to_list());
                for (size_t i = 1; i < evaluated.size(); ++i) {
                    if (evaluated[i].is_list()) {
                        auto other = evaluated[i].to_list();
                        result->insert(result->end(), other->begin(), other->end());
                    } else {
                        result->push_back(evaluated[i]);
                    }
                }
                return Value(result);
            }

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
            
            if (bigint_detail::any_bigint(evaluated)) {
                BigInt total = bigint_detail::as_bigint(evaluated[0]);
                if (evaluated.size() == 1) return bigint_detail::narrow(-total);
                for (size_t i = 1; i < evaluated.size(); ++i) {
                    total = total - bigint_detail::as_bigint(evaluated[i]);
                }
                return bigint_detail::narrow(total);
            }

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

            if (bigint_detail::any_bigint(evaluated)) {
                BigInt total = bigint_detail::as_bigint(evaluated[0]);
                for (size_t i = 1; i < evaluated.size(); ++i) {
                    total = total * bigint_detail::as_bigint(evaluated[i]);
                }
                return bigint_detail::narrow(total);
            }

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
            // Upstream's != is n-ary and means "not all equal": (!= 3 3 3) is false,
            // (!= 3 3 "a") is true. The binary-only form rejected both.
            if (args.size() < 2) {
                throw EvalError("!= requires at least 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            for (size_t i = 1; i < evaluated.size(); ++i) {
                if (!(evaluated[i] == evaluated[0])) {
                    return Value(true);
                }
            }
            return Value(false);
        });
        
        registry.register_operator("<", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("< requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            // Comparing through double would be wrong past 2^53, where distinct
            // bigints round to the same float.
            if (bigint_detail::any_bigint(evaluated)) {
                return Value(bigint_detail::as_bigint(evaluated[0]).compare(bigint_detail::as_bigint(evaluated[1])) < 0);
            }
            return Value(evaluated[0].to_float() < evaluated[1].to_float());
        });
        
        registry.register_operator(">", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("> requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            // Comparing through double would be wrong past 2^53, where distinct
            // bigints round to the same float.
            if (bigint_detail::any_bigint(evaluated)) {
                return Value(bigint_detail::as_bigint(evaluated[0]).compare(bigint_detail::as_bigint(evaluated[1])) > 0);
            }
            return Value(evaluated[0].to_float() > evaluated[1].to_float());
        });
        
        registry.register_operator("<=", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("<= requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            // Comparing through double would be wrong past 2^53, where distinct
            // bigints round to the same float.
            if (bigint_detail::any_bigint(evaluated)) {
                return Value(bigint_detail::as_bigint(evaluated[0]).compare(bigint_detail::as_bigint(evaluated[1])) <= 0);
            }
            return Value(evaluated[0].to_float() <= evaluated[1].to_float());
        });
        
        registry.register_operator(">=", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError(">= requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            // Comparing through double would be wrong past 2^53, where distinct
            // bigints round to the same float.
            if (bigint_detail::any_bigint(evaluated)) {
                return Value(bigint_detail::as_bigint(evaluated[0]).compare(bigint_detail::as_bigint(evaluated[1])) >= 0);
            }
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
