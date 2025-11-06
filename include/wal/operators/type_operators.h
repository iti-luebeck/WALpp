#pragma once

#include "wal/ast.h"
#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include <functional>
#include <vector>
#include <memory>

namespace wal::operators {

/**
 * @brief Type checking and conversion operators
 * 
 * Implements type introspection:
 * - type - get type of value
 * - is-int, is-float, is-string, is-list, is-bool, is-symbol, is-nil
 * - convert-int, convert-float, convert-string
 */
class TypeOperators {
public:
    static void register_all(wal::OperatorRegistry& registry) {
        registry.register_operator("type", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("type requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            const Value& val = evaluated[0];
            
            std::string type_name;
            if (val.is_nil()) type_name = "nil";
            else if (val.is_bool()) type_name = "bool";
            else if (val.is_int()) type_name = "int";
            else if (val.is_float()) type_name = "float";
            else if (val.is_string()) type_name = "string";
            else if (val.is_symbol()) type_name = "symbol";
            else if (val.is_list()) type_name = "list";
            else if (val.is_closure()) type_name = "closure";
            else if (val.is_macro()) type_name = "macro";
            else if (val.is_environment()) type_name = "environment";
            else if (val.is_user_operator()) type_name = "operator";
            else type_name = "unknown";
            
            return Value(std::make_shared<Symbol>(type_name));
        });
        
        registry.register_operator("is-int", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("is-int requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            return Value(evaluated[0].is_int());
        });
        
        registry.register_operator("is-float", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("is-float requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            return Value(evaluated[0].is_float());
        });
        
        registry.register_operator("is-string", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("is-string requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            return Value(evaluated[0].is_string());
        });
        
        registry.register_operator("is-list", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("is-list requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            return Value(evaluated[0].is_list());
        });
        
        registry.register_operator("is-bool", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("is-bool requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            return Value(evaluated[0].is_bool());
        });
        
        registry.register_operator("is-symbol", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("is-symbol requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            return Value(evaluated[0].is_symbol());
        });
        
        registry.register_operator("is-nil", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("is-nil requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            return Value(evaluated[0].is_nil());
        });
        
        registry.register_operator("convert-int", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("convert-int requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            const Value& val = evaluated[0];
            if (val.is_int()) {
                return val;
            } else if (val.is_float()) {
                return Value(static_cast<int64_t>(val.to_float()));
            } else if (val.is_string()) {
                try {
                    size_t idx;
                    int64_t result = std::stoll(val.to_string(), &idx);
                    if (idx != val.to_string().length()) {
                        throw EvalError("convert-int: cannot convert string \"" + val.to_string() + "\"");
                    }
                    return Value(result);
                } catch (const std::exception& e) {
                    throw EvalError("convert-int: cannot convert string \"" + val.to_string() + "\"");
                }
            } else {
                throw EvalError("convert-int: cannot convert " + val.to_string_repr());
            }
        });
        
        registry.register_operator("convert-float", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("convert-float requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            const Value& val = evaluated[0];
            if (val.is_float()) {
                return val;
            } else if (val.is_int()) {
                return Value(static_cast<double>(val.to_int()));
            } else if (val.is_string()) {
                try {
                    size_t idx;
                    double result = std::stod(val.to_string(), &idx);
                    if (idx != val.to_string().length()) {
                        throw EvalError("convert-float: cannot convert string \"" + val.to_string() + "\"");
                    }
                    return Value(result);
                } catch (const std::exception& e) {
                    throw EvalError("convert-float: cannot convert string \"" + val.to_string() + "\"");
                }
            } else {
                throw EvalError("convert-float: cannot convert " + val.to_string_repr());
            }
        });
        
        registry.register_operator("convert-string", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("convert-string requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            return Value(evaluated[0].to_string_repr());
        });
    }
};

} // namespace wal::operators
