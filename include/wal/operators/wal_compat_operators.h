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
            if (args.size() != 1) {
                throw EvalError("convert/bin expects 1 argument (integer)");
            }
            Value v = eval.eval(args[0]);
            if (!v.is_int()) {
                throw EvalError("convert/bin: argument must be an integer");
            }
            int64_t n = v.to_int();
            bool negative = n < 0;
            uint64_t magnitude = negative ? static_cast<uint64_t>(-n) : static_cast<uint64_t>(n);
            std::string digits;
            if (magnitude == 0) {
                digits = "0";
            }
            while (magnitude > 0) {
                digits.insert(digits.begin(), char('0' + (magnitude & 1ULL)));
                magnitude >>= 1;
            }
            try {
                int64_t asDecimal = std::stoll(digits);
                return Value(negative ? -asDecimal : asDecimal);
            } catch (const std::exception&) {
                // Beyond ~18 binary digits the decimal reading overflows; the digit
                // string is the useful answer then.
                return Value(negative ? "-" + digits : digits);
            }
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
            if (args.size() != 2) {
                throw EvalError("in expects 2 arguments (element list)");
            }
            auto evaluated = eval.eval_args(args);
            if (!evaluated[1].is_list()) {
                throw EvalError("in: second argument must be a list");
            }
            auto list = evaluated[1].to_list();
            for (size_t i = 0; i < list->size(); ++i) {
                if ((*list)[i] == evaluated[0]) {
                    return Value(true);
                }
            }
            return Value(false);
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
