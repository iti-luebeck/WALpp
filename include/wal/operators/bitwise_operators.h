#pragma once

#include "wal/ast.h"
#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include <functional>
#include <vector>

namespace wal::operators {

/**
 * @brief Bitwise operators
 * 
 * Implements bitwise operations on integers:
 * - band (bitwise AND)
 * - bor (bitwise OR)
 * - bxor (bitwise XOR)
 * - lshift (left shift <<)
 * - rshift (right shift >>)
 * - bnot (bitwise NOT ~)
 */
class BitwiseOperators {
public:
    static void register_all(wal::OperatorRegistry& registry) {
        registry.register_operator("band", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("band requires at least 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            int64_t result = evaluated[0].to_int();
            for (size_t i = 1; i < evaluated.size(); ++i) {
                result &= evaluated[i].to_int();
            }
            
            return Value(result);
        });
        
        registry.register_operator("bor", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("bor requires at least 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            int64_t result = evaluated[0].to_int();
            for (size_t i = 1; i < evaluated.size(); ++i) {
                result |= evaluated[i].to_int();
            }
            
            return Value(result);
        });
        
        registry.register_operator("bxor", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("bxor requires at least 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            int64_t result = evaluated[0].to_int();
            for (size_t i = 1; i < evaluated.size(); ++i) {
                result ^= evaluated[i].to_int();
            }
            
            return Value(result);
        });
        
        auto lshift_func = [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("lshift requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            
            int64_t value = evaluated[0].to_int();
            int64_t shift = evaluated[1].to_int();
            
            if (shift < 0 || shift >= 64) {
                throw EvalError("lshift: shift amount must be between 0 and 63");
            }
            
            return Value(value << shift);
        };
        
        registry.register_operator("lshift", lshift_func);
        registry.register_operator("<<", lshift_func);
        
        auto rshift_func = [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("rshift requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            
            int64_t value = evaluated[0].to_int();
            int64_t shift = evaluated[1].to_int();
            
            if (shift < 0 || shift >= 64) {
                throw EvalError("rshift: shift amount must be between 0 and 63");
            }
            
            return Value(value >> shift);
        };
        
        registry.register_operator("rshift", rshift_func);
        registry.register_operator(">>", rshift_func);
        
        registry.register_operator("bnot", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("bnot requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            return Value(~evaluated[0].to_int());
        });
    }
};

} // namespace wal::operators
