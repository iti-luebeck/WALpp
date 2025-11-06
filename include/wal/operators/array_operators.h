#pragma once

#include "wal/ast.h"
#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include <functional>
#include <algorithm>
#include <vector>

namespace wal::operators {

/**
 * @brief Array/List manipulation operators
 * 
 * Implements advanced list operations:
 * - array-ref - get element at index
 * - array-set - set element at index (returns new list)
 * - slice - extract sublist
 * - flatten - flatten nested lists
 * - sort - sort list
 * - reverse - already in list_operators but included for completeness
 * - unique - remove duplicates
 * - take - take first n elements
 * - drop - skip first n elements
 * - nth - get nth element
 */
class ArrayOperators {
public:
    static void register_all(wal::OperatorRegistry& registry) {
        registry.register_operator("array-ref", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("array-ref requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_list()) {
                throw EvalError("array-ref: first argument must be a list");
            }
            if (!evaluated[1].is_int()) {
                throw EvalError("array-ref: second argument must be an integer");
            }
            
            auto list = evaluated[0].to_list();
            int64_t index = evaluated[1].to_int();
            
            if (index < 0 || index >= static_cast<int64_t>(list->size())) {
                throw EvalError("array-ref: index out of bounds");
            }
            
            return (*list)[index];
        });
        
        registry.register_operator("array-set", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 3) {
                throw EvalError("array-set requires exactly 3 arguments");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_list()) {
                throw EvalError("array-set: first argument must be a list");
            }
            if (!evaluated[1].is_int()) {
                throw EvalError("array-set: second argument must be an integer");
            }
            
            auto list = *evaluated[0].to_list();
            int64_t index = evaluated[1].to_int();
            const Value& new_value = evaluated[2];
            
            if (index < 0 || index >= static_cast<int64_t>(list.size())) {
                throw EvalError("array-set: index out of bounds");
            }
            
            list[index] = new_value;
            return Value(std::make_shared<WList>(list));
        });
        
        registry.register_operator("slice", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() < 2 || args.size() > 3) {
                throw EvalError("slice requires 2 or 3 arguments");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_list()) {
                throw EvalError("slice: first argument must be a list");
            }
            if (!evaluated[1].is_int()) {
                throw EvalError("slice: second argument must be an integer");
            }
            if (args.size() == 3 && !evaluated[2].is_int()) {
                throw EvalError("slice: third argument must be an integer");
            }
            
            auto list = evaluated[0].to_list();
            int64_t start = evaluated[1].to_int();
            int64_t end = (args.size() == 3) ? evaluated[2].to_int() : list->size();
            
            if (start < 0) start = 0;
            if (end > static_cast<int64_t>(list->size())) end = list->size();
            if (start > end) start = end;
            
            std::vector<Value> result(list->begin() + start, list->begin() + end);
            return Value(std::make_shared<WList>(result));
        });
        
        registry.register_operator("flatten", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("flatten requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_list()) {
                throw EvalError("flatten: argument must be a list");
            }
            
            std::vector<Value> result;
            std::function<void(const std::shared_ptr<WList>&)> flatten_recursive = 
                [&](const std::shared_ptr<WList>& list) {
                    for (const auto& elem : *list) {
                        if (elem.is_list()) {
                            flatten_recursive(elem.to_list());
                        } else {
                            result.push_back(elem);
                        }
                    }
                };
            
            flatten_recursive(evaluated[0].to_list());
            return Value(std::make_shared<WList>(result));
        });
        
        registry.register_operator("sort", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("sort requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_list()) {
                throw EvalError("sort: argument must be a list");
            }
            
            auto list = *evaluated[0].to_list();
            
            // Sort only works on homogeneous lists of comparable types
            if (!list.empty()) {
                const auto& first = list[0];
                
                if (first.is_int()) {
                    std::sort(list.begin(), list.end(),
                             [](const Value& a, const Value& b) {
                                 return a.to_int() < b.to_int();
                             });
                } else if (first.is_float()) {
                    std::sort(list.begin(), list.end(),
                             [](const Value& a, const Value& b) {
                                 return a.to_float() < b.to_float();
                             });
                } else if (first.is_string()) {
                    std::sort(list.begin(), list.end(),
                             [](const Value& a, const Value& b) {
                                 return a.to_string() < b.to_string();
                             });
                } else {
                    throw EvalError("sort: list elements must be sortable (int, float, or string)");
                }
            }
            
            return Value(std::make_shared<WList>(list));
        });
        
        registry.register_operator("unique", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("unique requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_list()) {
                throw EvalError("unique: argument must be a list");
            }
            
            auto list = *evaluated[0].to_list();
            
            // Sort first, then remove adjacent duplicates
            if (!list.empty()) {
                const auto& first = list[0];
                
                if (first.is_int()) {
                    std::sort(list.begin(), list.end(),
                             [](const Value& a, const Value& b) {
                                 return a.to_int() < b.to_int();
                             });
                    list.erase(std::unique(list.begin(), list.end(),
                                          [](const Value& a, const Value& b) {
                                              return a.to_int() == b.to_int();
                                          }), list.end());
                } else if (first.is_float()) {
                    std::sort(list.begin(), list.end(),
                             [](const Value& a, const Value& b) {
                                 return a.to_float() < b.to_float();
                             });
                    list.erase(std::unique(list.begin(), list.end(),
                                          [](const Value& a, const Value& b) {
                                              return std::abs(a.to_float() - b.to_float()) < 1e-10;
                                          }), list.end());
                } else if (first.is_string()) {
                    std::sort(list.begin(), list.end(),
                             [](const Value& a, const Value& b) {
                                 return a.to_string() < b.to_string();
                             });
                    list.erase(std::unique(list.begin(), list.end(),
                                          [](const Value& a, const Value& b) {
                                              return a.to_string() == b.to_string();
                                          }), list.end());
                } else {
                    throw EvalError("unique: list elements must be comparable (int, float, or string)");
                }
            }
            
            return Value(std::make_shared<WList>(list));
        });
        
        registry.register_operator("take", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("take requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_list()) {
                throw EvalError("take: first argument must be a list");
            }
            if (!evaluated[1].is_int()) {
                throw EvalError("take: second argument must be an integer");
            }
            
            auto list = evaluated[0].to_list();
            int64_t n = evaluated[1].to_int();
            
            if (n < 0) {
                throw EvalError("take: count cannot be negative");
            }
            
            n = std::min(n, static_cast<int64_t>(list->size()));
            std::vector<Value> result(list->begin(), list->begin() + n);
            
            return Value(std::make_shared<WList>(result));
        });
        
        registry.register_operator("drop", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("drop requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_list()) {
                throw EvalError("drop: first argument must be a list");
            }
            if (!evaluated[1].is_int()) {
                throw EvalError("drop: second argument must be an integer");
            }
            
            auto list = evaluated[0].to_list();
            int64_t n = evaluated[1].to_int();
            
            if (n < 0) {
                throw EvalError("drop: count cannot be negative");
            }
            
            n = std::min(n, static_cast<int64_t>(list->size()));
            std::vector<Value> result(list->begin() + n, list->end());
            
            return Value(std::make_shared<WList>(result));
        });
        
        registry.register_operator("nth", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("nth requires exactly 2 arguments");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_int()) {
                throw EvalError("nth: first argument must be an integer");
            }
            if (!evaluated[1].is_list()) {
                throw EvalError("nth: second argument must be a list");
            }
            
            int64_t index = evaluated[0].to_int();
            auto list = evaluated[1].to_list();
            
            if (index < 0 || index >= static_cast<int64_t>(list->size())) {
                throw EvalError("nth: index out of bounds");
            }
            
            return (*list)[index];
        });
    }
};

} // namespace wal::operators

