#pragma once

#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include "wal/signal_resolve.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace wal::operators {

/**
 * @brief Operators that upstream WAL (ics-jku/wal) has and WAL++ lacked.
 *
 * Semantics were pinned by running each form against upstream 0.9.1 rather than
 * inferred from its source, and the conformance run in tests/conformance/ keeps them
 * honest.
 *
 * Several of these are renames rather than new capability: `mod` is `%`, `average` is
 * `mean`, `int?` is `is-int`. They are registered as their own operators rather than
 * aliases in a table, so each one's argument checking and error message reads the same
 * as the rest of the codebase.
 *
 * Operators receive their arguments unevaluated and call eval() themselves, so the
 * control-flow forms below work without touching the evaluator's special-form dispatch.
 * `do` and `fn` are the exceptions: they are spellings of `begin` and `lambda` and are
 * handled there.
 */
class WalCompatOperators {
public:
    static void register_all(OperatorRegistry& registry) {
        register_control_flow(registry);
        register_predicates(registry);
        register_conversions(registry);
        register_list_and_math(registry);
        register_output_and_aliases(registry);
        register_arrays(registry);
        register_variables(registry);
        register_trace_analysis(registry);
    }

private:
    /// Evaluate args[from..] in order and return the last value (nil if none).
    static Value eval_body(SEval& eval, const std::vector<Value>& args, size_t from) {
        Value result = Value(nullptr);
        for (size_t i = from; i < args.size(); ++i) {
            result = eval.eval(args[i]);
        }
        return result;
    }

    static void register_control_flow(OperatorRegistry& registry) {
        // (when cond body...) - body only if cond is true, else nil
        registry.register_operator("when", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("when expects at least 1 argument (condition [body...])");
            }
            if (!eval.eval(args[0]).is_truthy()) {
                return Value(nullptr);
            }
            return eval_body(eval, args, 1);
        });

        // (unless cond body...) - the inverse of when
        registry.register_operator("unless", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("unless expects at least 1 argument (condition [body...])");
            }
            if (eval.eval(args[0]).is_truthy()) {
                return Value(nullptr);
            }
            return eval_body(eval, args, 1);
        });

        // (while cond body...) - the standard WAL trace-iteration form, usually
        // (while (step 1) ...). It depends on `step` reporting that it could not
        // advance; before that was fixed this loop could not terminate.
        registry.register_operator("while", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("while expects at least 1 argument (condition [body...])");
            }
            Value result = Value(nullptr);
            while (eval.eval(args[0]).is_truthy()) {
                result = eval_body(eval, args, 1);
            }
            return result;
        });

        // (cond (test body...) ...) - first clause whose test is true
        registry.register_operator("cond", [](SEval& eval, const std::vector<Value>& args) -> Value {
            for (const auto& clause : args) {
                if (!clause.is_list()) {
                    throw EvalError("cond: each clause must be a list (test body...)");
                }
                auto parts = clause.to_list();
                if (parts->empty()) {
                    throw EvalError("cond: a clause must have a test");
                }
                // `default` or `else` as the test always matches, as upstream allows.
                const Value& test = (*parts)[0];
                bool matches;
                if (test.is_symbol()) {
                    const std::string& n = test.to_symbol()->get_name();
                    matches = (n == "default" || n == "else") ? true
                                                              : eval.eval(test).is_truthy();
                } else {
                    matches = eval.eval(test).is_truthy();
                }
                if (matches) {
                    Value result = Value(nullptr);
                    for (size_t i = 1; i < parts->size(); ++i) {
                        result = eval.eval((*parts)[i]);
                    }
                    return result;
                }
            }
            return Value(nullptr);
        });

        // (case key (value body...) ... (default body...))
        registry.register_operator("case", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("case expects at least 1 argument (key [clauses...])");
            }
            Value key = eval.eval(args[0]);
            for (size_t c = 1; c < args.size(); ++c) {
                if (!args[c].is_list()) {
                    throw EvalError("case: each clause must be a list (value body...)");
                }
                auto parts = args[c].to_list();
                if (parts->empty()) {
                    throw EvalError("case: a clause must have a value");
                }
                const Value& label = (*parts)[0];
                bool matches = false;
                if (label.is_symbol() && label.to_symbol()->get_name() == "default") {
                    matches = true;
                } else {
                    // Labels are literals, not expressions, so they are not evaluated.
                    matches = (key == label);
                }
                if (matches) {
                    Value result = Value(nullptr);
                    for (size_t i = 1; i < parts->size(); ++i) {
                        result = eval.eval((*parts)[i]);
                    }
                    return result;
                }
            }
            return Value(nullptr);
        });

        // (let ([name value] ...) body...) - bindings visible only in the body
        registry.register_operator("let", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("let expects at least 1 argument (bindings [body...])");
            }
            if (!args[0].is_list()) {
                throw EvalError("let: bindings must be a list of [name value] pairs");
            }

            auto scope = std::make_shared<Environment>(eval.get_environment());
            auto bindings = args[0].to_list();
            for (size_t i = 0; i < bindings->size(); ++i) {
                const Value& binding = (*bindings)[i];
                if (!binding.is_list()) {
                    throw EvalError("let: each binding must be a list [name value]");
                }
                auto pair = binding.to_list();
                if (pair->size() != 2 || !(*pair)[0].is_symbol()) {
                    throw EvalError("let: each binding must be [name value]");
                }
                // Values are evaluated in the outer scope, so bindings cannot see
                // each other. That matches let rather than let*.
                scope->define((*pair)[0].to_symbol()->get_name(), eval.eval((*pair)[1]));
            }

            auto saved = eval.get_environment();
            eval.set_environment(scope);
            try {
                Value result = eval_body(eval, args, 1);
                eval.set_environment(saved);
                return result;
            } catch (...) {
                eval.set_environment(saved);  // do not leak the scope on an error
                throw;
            }
        });

        // (exit [code])
        registry.register_operator("exit", [](SEval& eval, const std::vector<Value>& args) -> Value {
            int code = 0;
            if (!args.empty()) {
                Value v = eval.eval(args[0]);
                if (!v.is_int()) {
                    throw EvalError("exit: code must be an integer");
                }
                code = static_cast<int>(v.to_int());
            }
            std::exit(code);
        });
    }

    static void register_predicates(OperatorRegistry& registry) {
        auto one = [](const char* name, bool (*test)(const Value&)) {
            return [name, test](SEval& eval, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw EvalError(std::string(name) + " expects 1 argument, got " +
                                    std::to_string(args.size()));
                }
                return Value(test(eval.eval(args[0])));
            };
        };

        // Upstream's spellings of predicates WAL++ already had as is-*.
        registry.register_operator("int?", one("int?", [](const Value& v) { return v.is_int(); }));
        registry.register_operator("float?", one("float?", [](const Value& v) { return v.is_float(); }));
        registry.register_operator("string?", one("string?", [](const Value& v) { return v.is_string(); }));
        registry.register_operator("symbol?", one("symbol?", [](const Value& v) { return v.is_symbol(); }));
        registry.register_operator("list?", one("list?", [](const Value& v) { return v.is_list(); }));
        registry.register_operator("bool?", one("bool?", [](const Value& v) { return v.is_bool(); }));
        registry.register_operator("nil?", one("nil?", [](const Value& v) { return v.is_nil(); }));
        // An atom is anything that is not a list.
        registry.register_operator("atom?", one("atom?", [](const Value& v) { return !v.is_list(); }));

        // (defined? name) - whether the symbol is bound. The name is NOT evaluated,
        // since evaluating an undefined symbol is exactly what this asks about.
        registry.register_operator("defined?", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("defined? expects 1 argument (symbol)");
            }
            if (!args[0].is_symbol()) {
                throw EvalError("defined?: argument must be a symbol");
            }
            try {
                eval.get_environment()->get(args[0].to_symbol()->get_name());
                return Value(true);
            } catch (const EvalError&) {
                return Value(false);
            }
        });
    }

    static void register_conversions(OperatorRegistry& registry) {
        registry.register_operator("string->int", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("string->int expects 1 argument (string)");
            }
            Value v = eval.eval(args[0]);
            if (!v.is_string()) {
                throw EvalError("string->int: argument must be a string");
            }
            try {
                return Value(static_cast<int64_t>(std::stoll(v.to_string(), nullptr, 0)));
            } catch (const std::exception&) {
                throw EvalError("string->int: not an integer: " + v.to_string());
            }
        });

        registry.register_operator("int->string", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("int->string expects 1 argument (integer)");
            }
            Value v = eval.eval(args[0]);
            if (!v.is_int()) {
                throw EvalError("int->string: argument must be an integer");
            }
            return Value(std::to_string(v.to_int()));
        });

        registry.register_operator("symbol->string", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("symbol->string expects 1 argument (symbol)");
            }
            Value v = eval.eval(args[0]);
            if (!v.is_symbol()) {
                throw EvalError("symbol->string: argument must be a symbol");
            }
            return Value(v.to_symbol()->get_name());
        });

        registry.register_operator("string->symbol", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("string->symbol expects 1 argument (string)");
            }
            Value v = eval.eval(args[0]);
            if (!v.is_string()) {
                throw EvalError("string->symbol: argument must be a string");
            }
            return Value(std::make_shared<Symbol>(v.to_string()));
        });

        // (convert/bin n) - the binary digits of n, read back as a decimal integer,
        // which is what upstream returns: (convert/bin 5) is 101.
        registry.register_operator("convert/bin", [](SEval& eval, const std::vector<Value>& args) -> Value {
            // Upstream: (convert/bin value [width]) -- width zero-pads the digits.
            if (args.empty() || args.size() > 2) {
                throw EvalError("convert/bin expects (convert/bin expr:int [width:int])");
            }
            Value v = eval.eval(args[0]);
            if (!v.is_int()) {
                throw EvalError("convert/bin: argument must be an integer");
            }
            size_t width = 0;
            if (args.size() == 2) {
                Value w = eval.eval(args[1]);
                if (!w.is_int()) {
                    throw EvalError("convert/bin: second argument must evaluate to int");
                }
                if (w.to_int() > 0) width = static_cast<size_t>(w.to_int());
            }
            // Upstream always returns the binary digits as a STRING, including for 0
            // ("0") and for negatives ("-101"). An earlier version here reinterpreted
            // the digits as a decimal integer, which disagreed with upstream for every
            // input and silently lost the leading-zero/width information that makes
            // this operator useful on bit vectors.
            int64_t n = v.to_int();
            bool negative = n < 0;
            uint64_t magnitude = negative ? (~static_cast<uint64_t>(n) + 1ULL)
                                          : static_cast<uint64_t>(n);
            std::string digits;
            if (magnitude == 0) {
                digits = "0";
            }
            while (magnitude > 0) {
                digits.insert(digits.begin(), char('0' + (magnitude & 1ULL)));
                magnitude >>= 1;
            }
            while (digits.size() < width) {
                digits.insert(digits.begin(), '0');
            }
            return Value(negative ? "-" + digits : digits);
        });
    }

    static void register_list_and_math(OperatorRegistry& registry) {
        // (second list) and (last list)
        registry.register_operator("second", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("second expects 1 argument (list)");
            }
            Value v = eval.eval(args[0]);
            if (!v.is_list()) {
                throw EvalError("second: argument must be a list");
            }
            auto list = v.to_list();
            if (list->size() < 2) {
                throw EvalError("second: list has fewer than 2 elements");
            }
            return (*list)[1];
        });

        registry.register_operator("last", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("last expects 1 argument (list)");
            }
            Value v = eval.eval(args[0]);
            if (!v.is_list()) {
                throw EvalError("last: argument must be a list");
            }
            auto list = v.to_list();
            if (list->empty()) {
                throw EvalError("last: list is empty");
            }
            return (*list)[list->size() - 1];
        });

        // (in element list) - note the argument order
        registry.register_operator("in", [](SEval& eval, const std::vector<Value>& args) -> Value {
            // Upstream: (in value... container) is variadic and true only when EVERY
            // value is present. The binary-only form here rejected `(in 1 2 lst)`.
            if (args.size() < 2) {
                throw EvalError("in expects at least 2 arguments (in value* [list|string])");
            }
            auto evaluated = eval.eval_args(args);
            const Value& container = evaluated.back();

            auto contains = [&container](const Value& needle) -> bool {
                if (container.is_list()) {
                    auto list = container.to_list();
                    for (size_t i = 0; i < list->size(); ++i) {
                        if ((*list)[i] == needle) return true;
                    }
                    return false;
                }
                if (container.is_string()) {
                    if (!needle.is_string()) return false;
                    return container.to_string().find(needle.to_string()) != std::string::npos;
                }
                throw EvalError("in: last argument must be a list or a string");
            };

            for (size_t i = 0; i + 1 < evaluated.size(); ++i) {
                if (!contains(evaluated[i])) return Value(false);
            }
            return Value(true);
        });

        // (mod a b) - upstream's spelling of %
        registry.register_operator("mod", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("mod expects 2 arguments, got " + std::to_string(args.size()));
            }
            auto evaluated = eval.eval_args(args);
            if (!evaluated[0].is_int() || !evaluated[1].is_int()) {
                throw EvalError("mod: both arguments must be integers");
            }
            int64_t divisor = evaluated[1].to_int();
            if (divisor == 0) {
                throw EvalError("mod: division by zero");
            }
            return Value(evaluated[0].to_int() % divisor);
        });

        // (average list) - upstream's spelling of mean
        registry.register_operator("average", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("average expects 1 argument (list)");
            }
            Value v = eval.eval(args[0]);
            if (!v.is_list()) {
                throw EvalError("average: argument must be a list");
            }
            auto list = v.to_list();
            if (list->empty()) {
                throw EvalError("average: list is empty");
            }
            double total = 0.0;
            for (size_t i = 0; i < list->size(); ++i) {
                const Value& item = (*list)[i];
                if (!item.is_int() && !item.is_float()) {
                    throw EvalError("average: list must contain only numbers");
                }
                total += item.to_float();
            }
            return Value(total / static_cast<double>(list->size()));
        });
    }

    /// Upstream's `set` and `get`, which are not WAL++'s `set!` / `get-signal`.
    static void register_variables(OperatorRegistry& registry) {
        // (set (key expr) (key expr) ...) -- assigns to ALREADY DEFINED symbols only,
        // which is what distinguishes it from define. Unlike set!, it takes pairs and
        // can assign several at once.
        registry.register_operator("set", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("set: expects at least one (key expr) pair");
            }
            Value result = Value(nullptr);
            for (const auto& arg : args) {
                if (!arg.is_list()) {
                    throw EvalError("set: arguments must be (key expr) pairs");
                }
                auto pair = arg.to_list();
                if (pair->size() != 2) {
                    throw EvalError("set: arguments must be (key expr) pairs");
                }
                if (!(*pair)[0].is_symbol()) {
                    throw EvalError("set: key must be a symbol");
                }
                const std::string& name = (*pair)[0].to_symbol()->get_name();
                result = eval.eval((*pair)[1]);
                // write() walks the scope chain and throws if undefined, which is
                // upstream's "Write to undefined symbol" behavior.
                eval.get_environment()->write(name, result);
            }
            return result;
        });

        // (get "name") -- resolve a name computed at runtime, as a variable or signal.
        registry.register_operator("get", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("get: expects exactly one argument");
            }
            Value name = eval.eval(args[0]);
            std::string symbol_name;
            if (name.is_string()) {
                symbol_name = name.to_string();
            } else if (name.is_symbol()) {
                symbol_name = name.to_symbol()->get_name();
            } else {
                return Value(nullptr);
            }
            return eval.eval(Value(std::make_shared<Symbol>(symbol_name)));
        });

        // (symbol-add 'a "b" 5) => 'ab5 -- concatenate parts into one symbol.
        registry.register_operator("symbol-add", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("symbol-add: expects at least one argument");
            }
            auto evaluated = eval.eval_args(args);
            std::string name;
            for (const auto& part : evaluated) {
                if (part.is_symbol()) {
                    name += part.to_symbol()->get_name();
                } else if (part.is_string()) {
                    name += part.to_string();
                } else {
                    name += part.to_string_repr();
                }
            }
            return Value(std::make_shared<Symbol>(name));
        });
    }

    /**
     * Operators that sweep the trace rather than reading one index.
     *
     * Each saves every trace's index, walks to the end, and restores the indices, so
     * the caller's position is unchanged -- upstream does the same, and scripts rely
     * on being able to call these mid-analysis.
     */
    static void register_trace_analysis(OperatorRegistry& registry) {
        // (find condition) -> list of indices where condition held.
        registry.register_operator("find", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("find: expects exactly one argument (find condition)");
            }
            if (!eval.get_traces() || eval.get_traces()->empty()) {
                throw EvalError("find: no trace loaded");
            }
            auto saved = eval.get_traces()->indices();
            auto found = std::make_shared<WList>();
            try {
                bool ended = false;
                while (!ended) {
                    if (eval.eval(args[0]).is_truthy()) {
                        auto ids = eval.get_traces()->get_trace_ids();
                        auto trace = eval.get_traces()->get_trace(ids.front());
                        found->push_back(Value(static_cast<int64_t>(trace->get_index())));
                    }
                    ended = !eval.get_traces()->step(1).empty();
                }
            } catch (...) {
                restore(eval, saved);
                throw;
            }
            restore(eval, saved);
            return Value(found);
        });

        // (whenever condition body...) -> value of the last body run.
        registry.register_operator("whenever", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) {
                throw EvalError("whenever: expects at least two arguments (whenever condition body)");
            }
            if (!eval.get_traces() || eval.get_traces()->empty()) {
                throw EvalError("whenever: no trace loaded");
            }
            auto saved = eval.get_traces()->indices();
            Value result = Value(nullptr);
            try {
                bool ended = false;
                while (!ended) {
                    if (eval.eval(args[0]).is_truthy()) {
                        result = eval_body(eval, args, 1);
                    }
                    ended = !eval.get_traces()->step(1).empty();
                }
            } catch (...) {
                restore(eval, saved);
                throw;
            }
            restore(eval, saved);
            return result;
        });

        // (fold/signal f acc stop signal) -- fold over a signal's values until stop.
        registry.register_operator("fold/signal", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 4) {
                throw EvalError("fold/signal: expects 4 arguments (fold/signal f acc stop signal)");
            }
            Value func = eval.eval(args[0]);
            if (!func.is_closure()) {
                throw EvalError("fold/signal: first argument must be a function");
            }
            Value acc = eval.eval(args[1]);
            const Value& stop = args[2];
            Value signal = eval.eval(args[3]);
            if (!signal.is_symbol()) {
                throw EvalError("fold/signal: last argument must be a signal");
            }
            if (!eval.get_traces() || eval.get_traces()->empty()) {
                throw EvalError("fold/signal: no trace loaded");
            }
            const std::string& signal_name = signal.to_symbol()->get_name();
            auto saved = eval.get_traces()->indices();
            try {
                bool ended = false;
                while (!eval.eval(stop).is_truthy() && !ended) {
                    Value current = eval.get_traces()->signal_value(signal_name, 0);
                    std::vector<Value> call_args{acc, current};
                    acc = eval.eval_closure(func.to_closure(), call_args);
                    ended = !eval.get_traces()->step(1).empty();
                }
            } catch (...) {
                restore(eval, saved);
                throw;
            }
            restore(eval, saved);
            return acc;
        });
    }

    /// Put every trace back where it was before a sweep.
    static void restore(SEval& eval, const std::unordered_map<std::string, int>& saved) {
        for (const auto& [tid, index] : saved) {
            eval.get_traces()->set_index(index, tid);
        }
    }

    /**
     * Upstream's arrays: string-keyed, insertion-ordered, MUTABLE maps.
     *
     * These are not WAL++'s existing `array-ref`/`array-set` list helpers, which index
     * a list positionally. Upstream's `array` is a dict, shared by reference, so
     * `(seta a ...)` is visible through every binding holding the same array.
     */
    static void register_arrays(OperatorRegistry& registry) {
        // (array (key expr) (key expr) ...) -- each argument is a two-element pair.
        registry.register_operator("array", [](SEval& eval, const std::vector<Value>& args) -> Value {
            auto array = std::make_shared<WArray>();
            for (const auto& arg : args) {
                if (!arg.is_list()) {
                    throw EvalError("array: arguments must be (key expr) pairs");
                }
                auto pair = arg.to_list();
                if (pair->size() != 2) {
                    throw EvalError("array: arguments must be (key expr) pairs");
                }
                Value key = eval.eval((*pair)[0]);
                Value value = eval.eval((*pair)[1]);
                array->set(WArray::key_of(key), value);
            }
            return Value(array);
        });

        // (seta array key value) -- mutates in place and returns the array.
        registry.register_operator("seta", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 3) {
                throw EvalError("seta: requires three arguments (seta array key value)");
            }
            Value target = eval.eval(args[0]);
            if (!target.is_array()) {
                throw EvalError("seta: must be applied on an array");
            }
            Value key = eval.eval(args[1]);
            Value value = eval.eval(args[2]);
            target.to_array()->set(WArray::key_of(key), value);
            return target;
        });

        // (geta array key) -- missing keys are an error upstream, not nil.
        registry.register_operator("geta", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("geta: requires two arguments (geta array key)");
            }
            Value target = eval.eval(args[0]);
            if (!target.is_array()) {
                throw EvalError("geta: first argument must be an array");
            }
            std::string key = WArray::key_of(eval.eval(args[1]));
            const Value* found = target.to_array()->find(key);
            if (!found) {
                throw EvalError("geta: key " + key + " not found");
            }
            return *found;
        });

        // (dela array key) -- mutates in place and returns the array.
        registry.register_operator("dela", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("dela: requires two arguments (dela array key)");
            }
            Value target = eval.eval(args[0]);
            if (!target.is_array()) {
                throw EvalError("dela: first argument must be an array");
            }
            std::string key = WArray::key_of(eval.eval(args[1]));
            if (!target.to_array()->erase(key)) {
                throw EvalError("dela: key " + key + " not found");
            }
            return target;
        });

        // (mapa fn array) -- fn receives (key value); returns the list of results.
        registry.register_operator("mapa", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("mapa: requires two arguments (mapa function array)");
            }
            Value func = eval.eval(args[0]);
            if (!func.is_closure()) {
                throw EvalError("mapa: first argument must be a function");
            }
            Value target = eval.eval(args[1]);
            if (!target.is_array()) {
                throw EvalError("mapa: second argument must be an array");
            }
            auto results = std::make_shared<WList>();
            // Iterate over a copy: upstream's closure may seta/dela the same array,
            // and mutating the vector we are walking would invalidate the iteration.
            auto snapshot = target.to_array()->entries();
            for (const auto& [key, value] : snapshot) {
                std::vector<Value> call_args{Value(key), value};
                results->push_back(eval.eval_closure(func.to_closure(), call_args));
            }
            return Value(results);
        });
    }

    static void register_output_and_aliases(OperatorRegistry& registry) {
        // (printf fmt args...) - C-style formatting, as upstream.
        // Only the conversions that make sense for WAL values are handled, and each
        // is applied to one argument so a mismatched format cannot read past the end.
        registry.register_operator("printf", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("printf expects at least 1 argument (format [args...])");
            }
            auto evaluated = eval.eval_args(args);
            if (!evaluated[0].is_string()) {
                throw EvalError("printf: format must be a string");
            }
            const std::string fmt = evaluated[0].to_string();

            std::string out;
            size_t next_arg = 1;
            for (size_t i = 0; i < fmt.length(); ++i) {
                if (fmt[i] != '%') {
                    out += fmt[i];
                    continue;
                }
                if (i + 1 >= fmt.length()) {
                    throw EvalError("printf: format ends with a lone '%'");
                }
                // Accept a full C conversion spec, %[flags][width][.precision]conv,
                // so formats like %.2f and %5d work as they do upstream.
                ++i;  // step past the '%' before reading the spec
                std::string spec = "%";
                while (i < fmt.length() &&
                       (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == ' ' ||
                        fmt[i] == '#' || fmt[i] == '0')) {
                    spec += fmt[i++];
                }
                while (i < fmt.length() && std::isdigit(static_cast<unsigned char>(fmt[i]))) {
                    spec += fmt[i++];
                }
                if (i < fmt.length() && fmt[i] == '.') {
                    spec += fmt[i++];
                    while (i < fmt.length() && std::isdigit(static_cast<unsigned char>(fmt[i]))) {
                        spec += fmt[i++];
                    }
                }
                if (i >= fmt.length()) {
                    throw EvalError("printf: format ends inside a conversion");
                }
                char conv = fmt[i];

                if (conv == '%') {
                    out += '%';
                    continue;
                }
                if (next_arg >= evaluated.size()) {
                    throw EvalError("printf: not enough arguments for the format string");
                }
                const Value& v = evaluated[next_arg++];
                char buffer[256];
                switch (conv) {
                    case 'd': case 'i':
                        if (!v.is_int() && !v.is_float()) {
                            throw EvalError("printf: %d needs a number");
                        }
                        std::snprintf(buffer, sizeof(buffer), (spec + "lld").c_str(),
                                      static_cast<long long>(v.to_int()));
                        out += buffer;
                        break;
                    case 'f': case 'F': case 'g': case 'G': case 'e': case 'E':
                        if (!v.is_int() && !v.is_float()) {
                            throw EvalError("printf: %f needs a number");
                        }
                        std::snprintf(buffer, sizeof(buffer), (spec + conv).c_str(),
                                      v.to_float());
                        out += buffer;
                        break;
                    case 'x': case 'X': case 'o': case 'u':
                        if (!v.is_int()) {
                            throw EvalError(std::string("printf: %") + conv +
                                            " needs an integer");
                        }
                        std::snprintf(buffer, sizeof(buffer),
                                      (spec + "ll" + conv).c_str(),
                                      static_cast<long long>(v.to_int()));
                        out += buffer;
                        break;
                    case 'c':
                        if (!v.is_int()) {
                            throw EvalError("printf: %c needs an integer");
                        }
                        out += static_cast<char>(v.to_int());
                        break;
                    case 's': {
                        std::string text = v.is_string() ? v.to_string() : v.to_string_repr();
                        if (spec == "%") {
                            out += text;
                        } else {
                            std::snprintf(buffer, sizeof(buffer), (spec + "s").c_str(),
                                          text.c_str());
                            out += buffer;
                        }
                        break;
                    }
                    default:
                        throw EvalError(std::string("printf: unsupported conversion '%") +
                                        conv + "'");
                }
            }
            std::cout << out;
            return Value(nullptr);
        });

        // (alias short "full.signal.name") - a short name for a signal path.
        // The name is not evaluated; the target is, so it can be computed.
        registry.register_operator("alias", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("alias expects 2 arguments (name target)");
            }
            if (!args[0].is_symbol()) {
                throw EvalError("alias: name must be a symbol");
            }
            Value target = eval.eval(args[1]);
            std::string path;
            if (target.is_string()) {
                path = target.to_string();
            } else if (target.is_symbol()) {
                path = target.to_symbol()->get_name();
            } else {
                throw EvalError("alias: target must be a string or symbol");
            }
            eval.add_alias(args[0].to_symbol()->get_name(), path);
            return Value(nullptr);
        });

        registry.register_operator("unalias", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("unalias expects 1 argument (name)");
            }
            if (!args[0].is_symbol()) {
                throw EvalError("unalias: name must be a symbol");
            }
            eval.remove_alias(args[0].to_symbol()->get_name());
            return Value(nullptr);
        });
    }
};

} // namespace wal::operators
