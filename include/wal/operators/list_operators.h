#pragma once

#include "wal/ast.h"
#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include <functional>
#include <limits>
#include <algorithm>
#include <vector>
#include <memory>

namespace wal::operators {

/**
 * @brief List manipulation operators
 * 
 * Implements list operations:
 * - first (car) - get first element
 * - rest (cdr) - get rest of list
 * - append - concatenate lists
 * - length - get list length
 * - reverse - reverse a list
 * - map - apply function to each element
 * - fold - reduce list to single value
 * - filter - select matching elements
 * - zip - combine multiple lists
 * - range - generate sequence
 */
class ListOperators {
public:
    static void register_all(wal::OperatorRegistry& registry) {
        registry.register_operator("first", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("first requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_list()) {
                throw EvalError("first: argument must be a list");
            }
            
            auto list = evaluated[0].to_list();
            if (list->empty()) {
                throw EvalError("first: cannot get first element of empty list");
            }
            
            return (*list)[0];
        });
        
        registry.register_operator("car", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("car requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_list()) {
                throw EvalError("car: argument must be a list");
            }
            
            auto list = evaluated[0].to_list();
            if (list->empty()) {
                throw EvalError("car: cannot get first element of empty list");
            }
            
            return (*list)[0];
        });
        
        registry.register_operator("rest", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("rest requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_list()) {
                throw EvalError("rest: argument must be a list");
            }
            
            auto list = evaluated[0].to_list();
            if (list->empty()) {
                return Value(std::make_shared<WList>());
            }
            
            std::vector<Value> rest_elements(list->begin() + 1, list->end());
            return Value(std::make_shared<WList>(rest_elements));
        });
        
        registry.register_operator("cdr", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("cdr requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_list()) {
                throw EvalError("cdr: argument must be a list");
            }
            
            auto list = evaluated[0].to_list();
            if (list->empty()) {
                return Value(std::make_shared<WList>());
            }
            
            std::vector<Value> rest_elements(list->begin() + 1, list->end());
            return Value(std::make_shared<WList>(rest_elements));
        });
        
        registry.register_operator("append", [](SEval& eval, const std::vector<Value>& args) -> Value {
            auto evaluated = eval.eval_args(args);
            
            std::vector<Value> result;
            for (const auto& val : evaluated) {
                if (!val.is_list()) {
                    throw EvalError("append: all arguments must be lists");
                }
                auto list = val.to_list();
                result.insert(result.end(), list->begin(), list->end());
            }
            
            return Value(std::make_shared<WList>(result));
        });
        
        registry.register_operator("length", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("length requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_list()) {
                throw EvalError("length: argument must be a list");
            }
            
            return Value(static_cast<int64_t>(evaluated[0].to_list()->size()));
        });
        
        registry.register_operator("reverse", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("reverse requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_list()) {
                throw EvalError("reverse: argument must be a list");
            }
            
            auto list = evaluated[0].to_list();
            std::vector<Value> reversed(list->rbegin(), list->rend());
            
            return Value(std::make_shared<WList>(reversed));
        });
        
        registry.register_operator("map", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("map requires exactly 2 arguments");
            }
            
            // Evaluate the list argument first
            std::vector<Value> eval_args_list = {args[1]};
            auto evaluated_list = eval.eval_args(eval_args_list);
            
            if (!evaluated_list[0].is_list()) {
                throw EvalError("map: second argument must be a list");
            }
            
            // The function stays unevaluated - we need to eval it for each element
            Value func = eval.eval(args[0]);
            
            if (!func.is_closure() && !func.is_user_operator()) {
                throw EvalError("map: first argument must be a function");
            }
            
            auto list = evaluated_list[0].to_list();
            std::vector<Value> results;
            
            for (const auto& elem : *list) {
                std::vector<Value> func_args = {elem};
                results.push_back(eval.apply_callable(func, func_args));
            }
            
            return Value(std::make_shared<WList>(results));
        });
        
        registry.register_operator("fold", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 3) {
                throw EvalError("fold requires exactly 3 arguments");
            }
            
            // Evaluate the initial value and list
            std::vector<Value> eval_args_vals = {args[1], args[2]};
            auto evaluated = eval.eval_args(eval_args_vals);
            
            if (!evaluated[1].is_list()) {
                throw EvalError("fold: third argument must be a list");
            }
            
            // The function stays unevaluated
            Value func = eval.eval(args[0]);
            
            if (!func.is_closure() && !func.is_user_operator()) {
                throw EvalError("fold: first argument must be a function");
            }
            
            auto list = evaluated[1].to_list();
            Value accumulator = evaluated[0];
            
            for (const auto& elem : *list) {
                std::vector<Value> func_args = {accumulator, elem};
                accumulator = eval.apply_callable(func, func_args);
            }
            
            return accumulator;
        });
        
        registry.register_operator("filter", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("filter requires exactly 2 arguments");
            }
            
            // Evaluate the list argument first
            std::vector<Value> eval_args_list = {args[1]};
            auto evaluated_list = eval.eval_args(eval_args_list);
            
            if (!evaluated_list[0].is_list()) {
                throw EvalError("filter: second argument must be a list");
            }
            
            // The predicate stays unevaluated
            Value predicate = eval.eval(args[0]);
            
            if (!predicate.is_closure() && !predicate.is_user_operator()) {
                throw EvalError("filter: first argument must be a function");
            }
            
            auto list = evaluated_list[0].to_list();
            std::vector<Value> results;
            
            for (const auto& elem : *list) {
                std::vector<Value> pred_args = {elem};
                Value result = eval.apply_callable(predicate, pred_args);
                if (result.is_truthy()) {
                    results.push_back(elem);
                }
            }
            
            return Value(std::make_shared<WList>(results));
        });
        
        registry.register_operator("zip", [](SEval& eval, const std::vector<Value>& args) -> Value {
            auto evaluated = eval.eval_args(args);
            
            if (evaluated.empty()) {
                return Value(std::make_shared<WList>());
            }
            
            // Find minimum list length
            size_t min_length = std::numeric_limits<size_t>::max();
            for (const auto& val : evaluated) {
                if (!val.is_list()) {
                    throw EvalError("zip: all arguments must be lists");
                }
                min_length = std::min(min_length, val.to_list()->size());
            }
            
            std::vector<Value> result;
            for (size_t i = 0; i < min_length; ++i) {
                std::vector<Value> tuple_elements;
                for (const auto& val : evaluated) {
                    tuple_elements.push_back((*val.to_list())[i]);
                }
                result.push_back(Value(std::make_shared<WList>(tuple_elements)));
            }
            
            return Value(std::make_shared<WList>(result));
        });
        
        registry.register_operator("range", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty() || args.size() > 3) {
                throw EvalError("range requires 1 to 3 arguments");
            }
            
            auto evaluated = eval.eval_args(args);
            
            int64_t start = 0;
            int64_t end = 0;
            int64_t step = 1;
            
            if (args.size() == 1) {
                // (range n) -> 0 to n-1
                end = evaluated[0].to_int();
            } else if (args.size() == 2) {
                // (range start end) -> start to end-1
                start = evaluated[0].to_int();
                end = evaluated[1].to_int();
            } else {
                // (range start end step)
                start = evaluated[0].to_int();
                end = evaluated[1].to_int();
                step = evaluated[2].to_int();
            }
            
            if (step == 0) {
                throw EvalError("range: step cannot be 0");
            }
            
            std::vector<Value> result;
            
            if (step > 0) {
                for (int64_t i = start; i < end; i += step) {
                    result.push_back(Value(i));
                }
            } else {
                for (int64_t i = start; i > end; i += step) {
                    result.push_back(Value(i));
                }
            }
            
            return Value(std::make_shared<WList>(result));
        });
    }
};

} // namespace wal::operators
