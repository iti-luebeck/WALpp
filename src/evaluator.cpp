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
#include "wal/operators/wal_compat_operators.h"
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

/**
 * @brief Expand a quasiquoted template.
 *
 * Inside `(quasiquote t)` everything is literal except `(unquote x)`, which is
 * replaced by the value of x, and `(unquote-splicing x)`, which splices a list's
 * elements into the surrounding list. Nesting is tracked by @p depth so that a
 * quasiquote inside a quasiquote behaves, as upstream's standard library relies on.
 */
/**
 * @brief Call whatever is callable: a lambda, or a builtin passed as a value.
 *
 * `(map get SIGNALS)` and `(fold + 0 lst)` hand a BUILTIN to a higher-order
 * operator. Those operators used to require a closure, so passing an operator was
 * an error. The arguments here are already evaluated, so they are re-wrapped in
 * `quote` before dispatch, which is how upstream does it too.
 */
Value SEval::apply_callable(const Value& func, const std::vector<Value>& args) {
    if (func.is_closure()) {
        return eval_closure(func.to_closure(), args);
    }
    if (func.is_user_operator()) {
        const std::string& name = func.to_user_operator()->get_name();
        auto op = builtin_operators.get_operator(name);
        if (!op) {
            throw EvalError("Unknown operator: " + name);
        }
        std::vector<Value> quoted;
        quoted.reserve(args.size());
        for (const auto& arg : args) {
            auto wrapper = std::make_shared<WList>();
            wrapper->push_back(Value(std::make_shared<Symbol>("quote")));
            wrapper->push_back(arg);
            quoted.push_back(Value(wrapper));
        }
        return (*op)(*this, quoted);
    }
    throw EvalError("not a function");
}

Value SEval::quasiquote(const Value& tmpl, int depth) {
    if (!tmpl.is_list()) {
        return tmpl;
    }
    auto list = tmpl.to_list();
    if (list->empty()) {
        return tmpl;
    }

    // A bare (unquote x) / (quasiquote x) node changes the depth.
    if ((*list)[0].is_symbol() && list->size() == 2) {
        const std::string& head = (*list)[0].to_symbol()->get_name();
        if (head == "unquote") {
            if (depth == 1) return eval((*list)[1]);
            auto inner = std::make_shared<WList>();
            inner->push_back((*list)[0]);
            inner->push_back(quasiquote((*list)[1], depth - 1));
            return Value(inner);
        }
        if (head == "quasiquote") {
            auto inner = std::make_shared<WList>();
            inner->push_back((*list)[0]);
            inner->push_back(quasiquote((*list)[1], depth + 1));
            return Value(inner);
        }
    }

    auto result = std::make_shared<WList>();
    for (const auto& element : *list) {
        bool spliced = false;
        if (element.is_list()) {
            auto node = element.to_list();
            if (node->size() == 2 && (*node)[0].is_symbol() &&
                (*node)[0].to_symbol()->get_name() == "unquote-splicing" && depth == 1) {
                Value value = eval((*node)[1]);
                if (!value.is_list()) {
                    throw EvalError("unquote-splicing: argument must evaluate to a list");
                }
                auto items = value.to_list();
                for (const auto& item : *items) result->push_back(item);
                spliced = true;
            }
        }
        if (!spliced) {
            result->push_back(quasiquote(element, depth));
        }
    }
    return Value(result);
}

/**
 * @brief Expand a macro call into the form it stands for.
 *
 * The arguments are bound UNEVALUATED, which is what separates a macro from a
 * function: the body receives the source expressions and builds a new expression
 * from them. The caller evaluates the result.
 */
Value SEval::expand_macro(const std::string& name, const std::vector<Value>& args) {
    Value stored = macros.at(name);
    if (!stored.is_macro()) {
        throw EvalError(name + " is registered as a macro but is not one");
    }
    auto macro = stored.to_macro();

    auto scope = std::make_shared<Environment>(macro->get_environment());

    // A SYMBOL where the parameter list would be is upstream's variadic form:
    // (defmacro defun args ...) binds `args` to the whole argument list. Binding it
    // to just the first argument, as a one-element list would, silently truncated
    // every variadic macro.
    if (macro->get_args().is_symbol()) {
        auto all = std::make_shared<WList>();
        for (const auto& arg : args) all->push_back(arg);
        scope->define(macro->get_args().to_symbol()->get_name(), Value(all));
        auto saved_variadic = current_environment;
        current_environment = scope;
        Value expansion;
        try {
            expansion = eval(macro->get_expression());
        } catch (...) {
            current_environment = saved_variadic;
            throw;
        }
        current_environment = saved_variadic;
        return expansion;
    }

    auto parameters = std::make_shared<WList>();
    if (macro->get_args().is_list()) {
        parameters = macro->get_args().to_list();
    }

    for (size_t i = 0; i < parameters->size(); ++i) {
        if (!(*parameters)[i].is_symbol()) {
            throw EvalError(name + ": macro parameters must be symbols");
        }
        const std::string& parameter = (*parameters)[i].to_symbol()->get_name();
        // A trailing &rest-style parameter is not part of upstream's macro syntax,
        // so a missing argument simply binds nil.
        scope->define(parameter, i < args.size() ? args[i] : Value(nullptr));
    }

    auto saved = current_environment;
    current_environment = scope;
    Value expansion;
    try {
        expansion = eval(macro->get_expression());
    } catch (...) {
        current_environment = saved;
        throw;
    }
    current_environment = saved;
    return expansion;
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
            // WAL's special variables (INDEX, MAX-INDEX, TS, SIGNALS, SCOPES, ...)
            Value special;
            if (resolve_special_variable(traces, name, special, current_scope)) {
                return special;
            }
            // An alias stands for a name: (alias clk "tb.clk") then clk. Upstream
            // aliases any symbol, not only a signal path, so (alias abc 'x) has to
            // reach the VARIABLE x as well.
            std::string lookup = name;
            if (has_alias(name)) {
                lookup = aliases.at(name);
                try {
                    return current_environment->get(lookup);
                } catch (const EvalError&) {
                    // not a variable; fall through to the signal lookup below
                }
                Value aliased_special;
                if (resolve_special_variable(traces, lookup, aliased_special, current_scope)) {
                    return aliased_special;
                }
            }
            std::string resolved;
            if (resolve_signal_name(traces, lookup, resolved)) {
                return traces->signal_value(resolved, 0);
            }
            // Finally, a builtin used as a VALUE: `(map get SIGNALS)` passes the
            // operator itself. Checked last, so a variable or a signal of the same
            // name still wins and no existing program changes meaning.
            if (builtin_operators.has_operator(lookup)) {
                return Value(std::make_shared<UserOperator>(lookup));
            }
            throw;  // not a variable, a special, an alias, a signal or an operator
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
        
        // The head of a form is usually a symbol, but it may be any expression that
        // produces a callable, which is how an inline lambda is applied:
        //   ((fn [x] (* x 2)) 21) => 42
        // WAL++ rejected that outright, so a lambda had to be given a name through
        // define before it could be called.
        if (!operator_form.is_symbol()) {
            Value callable = eval(operator_form);
            if (callable.is_closure()) {
                std::vector<Value> call_args(list->begin() + 1, list->end());
                return eval_closure(callable.to_closure(), eval_args(call_args));
            }
            throw EvalError("Operator must be a symbol or an expression producing a "
                            "function, got " + operator_form.to_string_repr());
        }
        
        std::string op_name = operator_form.to_symbol()->get_name();
        
        // Get arguments (unevaluated - operators decide when to evaluate)
        std::vector<Value> args(list->begin() + 1, list->end());
        
        // Check for special forms and macros

        // A macro call expands first and the expansion is then evaluated. This is
        // checked before every other form so a macro can shadow a builtin, which is
        // how upstream's own standard library defines `rising`, `count` and friends.
        if (has_macro(op_name)) {
            return eval(expand_macro(op_name, args));
        }

        if (op_name == "quasiquote") {
            if (args.size() != 1) {
                throw EvalError("quasiquote requires exactly 1 argument");
            }
            return quasiquote(args[0], 1);
        }

        if (op_name == "unquote" || op_name == "unquote-splicing") {
            throw EvalError(op_name + " is only valid inside a quasiquote");
        }

        if (op_name == "defmacro") {
            // (defmacro name [params] body...)
            if (args.size() < 3) {
                throw EvalError("defmacro requires a name, a parameter list and a body");
            }
            if (!args[0].is_symbol()) {
                throw EvalError("defmacro: the name must be a symbol");
            }
            const std::string& macro_name = args[0].to_symbol()->get_name();
            Value body = args[2];
            if (args.size() > 3) {
                // Several body forms become an implicit (begin ...).
                auto wrapped = std::make_shared<WList>();
                wrapped->push_back(Value(std::make_shared<Symbol>("begin")));
                for (size_t i = 2; i < args.size(); ++i) wrapped->push_back(args[i]);
                body = Value(wrapped);
            }
            add_macro(macro_name, Value(std::make_shared<Macro>(
                current_environment, args[1], body, macro_name)));
            return Value(std::make_shared<Symbol>(macro_name));
        }

        if (op_name == "macroexpand") {
            if (args.size() != 1) {
                throw EvalError("macroexpand requires exactly 1 argument");
            }
            // The argument is the call form itself, usually written quoted.
            Value form = args[0];
            if (form.is_list()) {
                auto inner = form.to_list();
                if (!inner->empty() && (*inner)[0].is_symbol() &&
                    (*inner)[0].to_symbol()->get_name() == "quote" && inner->size() == 2) {
                    form = (*inner)[1];
                }
            }
            if (!form.is_list() || form.to_list()->empty()) return form;
            auto call = form.to_list();
            if (!(*call)[0].is_symbol()) return form;
            const std::string& called = (*call)[0].to_symbol()->get_name();
            if (!has_macro(called)) return form;
            std::vector<Value> call_args(call->begin() + 1, call->end());
            return expand_macro(called, call_args);
        }

        if (op_name == "gensym") {
            if (!args.empty()) {
                throw EvalError("gensym expects no arguments");
            }
            static int64_t counter = 0;
            return Value(std::make_shared<Symbol>("g" + std::to_string(counter++)));
        }

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
        
        // `fn` is upstream WAL's spelling of the same form.
        if (op_name == "lambda" || op_name == "fn") {
            if (args.size() < 2) {
                throw EvalError(op_name + " requires parameters and a body");
            }
            // Every parameter must be a symbol. `(fn (x 1) x)` is an error upstream;
            // accepting it bound nothing to the literal and then failed confusingly
            // at call time instead of at the definition.
            if (args[0].is_list()) {
                auto parameters = args[0].to_list();
                for (const auto& parameter : *parameters) {
                    if (!parameter.is_symbol()) {
                        throw EvalError(op_name + ": parameters must be symbols, got " +
                                        parameter.to_string_repr());
                    }
                }
            } else if (!args[0].is_symbol()) {
                throw EvalError(op_name + ": parameters must be a list of symbols");
            }
            // A body of several forms is wrapped in an implicit begin, as upstream
            // allows: (fn [x] (print x) (* x 2)).
            Value body = args[1];
            if (args.size() > 2) {
                std::vector<Value> seq;
                seq.push_back(Value(std::make_shared<Symbol>("begin")));
                for (size_t i = 1; i < args.size(); ++i) {
                    seq.push_back(args[i]);
                }
                body = Value(std::make_shared<WList>(seq));
            }
            auto closure = std::make_shared<Closure>(current_environment, args[0], body);
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
        
        // `do` is upstream WAL's spelling of the same form.
        if (op_name == "begin" || op_name == "do") {
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

    // Upstream WAL compatibility: control flow, predicates, conversions, printf,
    // aliases. Registered last so these names are the ones that win if an earlier
    // set happens to define the same spelling.
    wal::operators::WalCompatOperators::register_all(builtin_operators);
}

} // namespace wal

