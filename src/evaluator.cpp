#include "wal/evaluator.h"
#include "wal/parser.h"
#include "wal/signal_resolve.h"
#include "wal/operators/core_math_operators.h"
#include "wal/operators/base_operators.h"
#include "wal/operators/bitwise_operators.h"
#include "wal/operators/list_operators.h"
#include "wal/operators/type_operators.h"
#include "wal/operators/string_operators.h"
#include "wal/operators/math_operators.h"
#include "wal/operators/array_operators.h"
#include "wal/operators/aggregation_operators.h"
#include "wal/operators/comparison_operators.h"
#include "wal/operators/signal_operators.h"
#include "wal/operators/spectrum_analysis_operators.h"
#include "wal/operators/statistical_analysis_operators.h"
#include "wal/operators/signal_processing_operators.h"
#include "wal/operators/data_export_operators.h"
#include "wal/operators/visualization_operators.h"
#include "wal/operators/trace_management_operators.h"
#include "wal/operators/hamming_operators.h"
#include <cmath>
#include <algorithm>

namespace wal {

SEval::SEval(std::shared_ptr<TraceContainer> traces)
    : traces(traces) {
    
    global_environment = std::make_shared<Environment>();
    current_environment = global_environment;
    
    initialize_builtins();
}

void SEval::reset() {
    gensym_counter = 0;
    current_environment = global_environment;
    imports.clear();
    aliases.clear();
    macros.clear();
    current_scope = "";
    current_group = "";
    virtual_signals.clear();
}

std::vector<Value> SEval::eval_args(const std::vector<Value>& args) {
    std::vector<Value> results;
    for (const auto& arg : args) {
        results.push_back(eval(arg));
    }
    return results;
}

Value SEval::eval_closure(std::shared_ptr<Closure> closure, 
                          const std::vector<Value>& args) {
    // Create new environment for closure execution
    auto closure_env = std::make_shared<Environment>(closure->get_environment());
    
    // Bind parameters
    const Value& params = closure->get_args();
    
    if (params.is_symbol()) {
        // Single parameter - rest parameters
        closure_env->define(params.to_symbol()->get_name(), Value(std::make_shared<WList>(args)));
    } else if (params.is_list()) {
        // Multiple parameters
        auto param_list = params.to_list();
        if (param_list->size() != args.size()) {
            throw EvalError("Expected " + std::to_string(param_list->size()) + 
                          " arguments, got " + std::to_string(args.size()));
        }
        for (size_t i = 0; i < param_list->size(); ++i) {
            if (!(*param_list)[i].is_symbol()) {
                throw EvalError("Parameter must be a symbol");
            }
            closure_env->define((*param_list)[i].to_symbol()->get_name(), args[i]);
        }
    } else {
        throw EvalError("Invalid parameter list in closure");
    }
    
    // Evaluate body in new environment
    auto saved_env = current_environment;
    current_environment = closure_env;
    Value result = eval(closure->get_expression());
    current_environment = saved_env;
    
    return result;
}

Value SEval::dispatch_builtin(const std::string& name, const std::vector<Value>& args) {
    auto op = builtin_operators.get_operator(name);
    if (!op.has_value()) {
        throw EvalError("Unknown operator: " + name);
    }
    return op.value()(*this, args);
}

Value SEval::dispatch_user(const std::string& name, const std::vector<Value>& args) {
    auto it = user_operators.find(name);
    if (it == user_operators.end()) {
        throw EvalError("Unknown user operator: " + name);
    }
    return it->second(*this, args);
}

Value SEval::eval(const Value& expr) {
    // Self-evaluating forms
    if (expr.is_nil() || expr.is_bool() || expr.is_int() || 
        expr.is_float() || expr.is_string()) {
        return expr;
    }
    
    // Symbol lookup.
    //
    // A symbol that is not a bound variable may name a signal. Upstream WAL exposes
    // signals as ordinary variables, so `tb.dut.counter` evaluates to that signal's
    // value at the current index, and published WAL programs rely on it. Variables
    // win: a binding always shadows a signal of the same name, so this fallback can
    // never change the meaning of an existing program.
    if (expr.is_symbol()) {
        auto sym = expr.to_symbol();
        const std::string& name = sym->get_name();
        try {
            return current_environment->get(name);
        } catch (const EvalError&) {
            std::string resolved;
            if (resolve_signal_name(traces, name, resolved)) {
                return traces->signal_value(resolved, 0);
            }
            throw;  // not a variable and not a signal: report the original error
        }
    }
    
    // List evaluation - function call or special form
    if (expr.is_list()) {
        auto list = expr.to_list();
        
        if (list->empty()) {
            throw EvalError("Cannot evaluate empty list");
        }
        
        // Get operator name
        const Value& operator_form = (*list)[0];
        
        if (!operator_form.is_symbol()) {
            throw EvalError("Operator must be a symbol");
        }
        
        std::string op_name = operator_form.to_symbol()->get_name();
        
        // Get arguments (unevaluated - operators decide when to evaluate)
        std::vector<Value> args(list->begin() + 1, list->end());
        
        // Check for special forms and macros
        if (op_name == "quote") {
            if (args.size() != 1) {
                throw EvalError("quote requires exactly 1 argument");
            }
            return args[0];
        }
        
        if (op_name == "if") {
            if (args.size() < 2 || args.size() > 3) {
                throw EvalError("if requires 2 or 3 arguments");
            }
            Value condition = eval(args[0]);
            if (condition.is_truthy()) {
                return eval(args[1]);
            } else if (args.size() == 3) {
                return eval(args[2]);
            }
            return Value(nullptr);
        }
        
        if (op_name == "lambda") {
            if (args.size() != 2) {
                throw EvalError("lambda requires exactly 2 arguments");
            }
            auto closure = std::make_shared<Closure>(current_environment, args[0], args[1]);
            return Value(closure);
        }
        
        if (op_name == "define") {
            if (args.size() != 2) {
                throw EvalError("define requires exactly 2 arguments");
            }
            if (!args[0].is_symbol()) {
                throw EvalError("define target must be a symbol");
            }
            Value value = eval(args[1]);
            current_environment->define(args[0].to_symbol()->get_name(), value);
            return Value(nullptr);
        }
        
        if (op_name == "set!") {
            if (args.size() != 2) {
                throw EvalError("set! requires exactly 2 arguments");
            }
            if (!args[0].is_symbol()) {
                throw EvalError("set! target must be a symbol");
            }
            Value value = eval(args[1]);
            current_environment->write(args[0].to_symbol()->get_name(), value);
            return Value(nullptr);
        }
        
        if (op_name == "begin") {
            if (args.empty()) {
                return Value(nullptr);
            }
            Value result = Value(nullptr);
            for (const auto& arg : args) {
                result = eval(arg);
            }
            return result;
        }
        
        // Built-in operator dispatch
        if (builtin_operators.has_operator(op_name)) {
            return dispatch_builtin(op_name, args);
        }
        
        // Function call with application
        Value func = eval(operator_form);
        
        if (func.is_closure()) {
            auto evaluated_args = eval_args(args);
            return eval_closure(func.to_closure(), evaluated_args);
        }
        
        throw EvalError("Operator not found: " + op_name);
    }
    
    // Closures, macros, etc. self-evaluate
    if (expr.is_closure() || expr.is_macro() || expr.is_environment()) {
        return expr;
    }
    
    throw EvalError("Cannot evaluate: " + expr.to_string_repr());
}

std::string SEval::gensym(const std::string& prefix) {
    return prefix + "_" + std::to_string(++gensym_counter);
}

void SEval::initialize_builtins() {
    // Register Phase 1 - Core Math operators
    wal::operators::CoreMathOperators::register_all(builtin_operators);
    
    // Register Phase 2 - Data Structure Operations
    wal::operators::BitwiseOperators::register_all(builtin_operators);
    wal::operators::ListOperators::register_all(builtin_operators);
    wal::operators::TypeOperators::register_all(builtin_operators);
    
    // Register Phase 3 operators
    wal::operators::StringOperators::register_all(builtin_operators);
    wal::operators::MathOperators::register_all(builtin_operators);
    wal::operators::ArrayOperators::register_all(builtin_operators);
    
    // Register Phase 4 operators
    wal::operators::AggregationOperators::register_all(builtin_operators);
    wal::operators::ComparisonOperators::register_all(builtin_operators);
    
    // Register Phase 5 operators
    wal::operators::SignalOperators::register_all(builtin_operators);
    
    // Register Phase 6 - Signal Analysis operators
    wal::operators::SpectrumAnalysisOperators::register_all(builtin_operators);
    wal::operators::HammingOperators::register_all(builtin_operators);
    wal::operators::StatisticalAnalysisOperators::register_all(builtin_operators);
    wal::operators::SignalProcessingOperators::register_all(builtin_operators);
    
    // Register Phase 7 - Data Export and Visualization
    wal::operators::DataExportOperators::register_all(builtin_operators);
    wal::operators::VisualizationOperators::register_all(builtin_operators);
    
    // Register Phase 8 - Trace File Management
    wal::operators::TraceManagementOperators::register_all(builtin_operators);
}

} // namespace wal

