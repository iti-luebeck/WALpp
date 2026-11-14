#pragma once

#include "../ast.h"
#include "../evaluator.h"
#include "../parser.h"
#include "../operator_registry.h"
#include "../signal_resolve.h"
#include "../trace/trace_container.h"
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace wal {
namespace operators {

/**
 * @brief The last of upstream WAL's operators.
 *
 * These are the ones left over once the language core, the analysis vocabulary and
 * the standard library were in place. They fall into four groups:
 *
 *  - running data as code: `eval`, `parse`, `eval-file`, `require`
 *  - trace queries: `loaded-traces`, `find/g`
 *  - trace authoring: `new-trace`, `defsig`, `dump-trace`, `trim-trace`, `sample-at`
 *  - scope as a statement: `set-scope`, `unset-scope`
 *  - conversion: `bits->sint`
 *
 * `import` and `call` are upstream's bridge to the CPython interpreter it runs
 * inside. There is no equivalent in a C++ program, so rather than pretend, they are
 * registered and fail with a message that says exactly that. A script reaching for
 * them gets a clear answer instead of "Undefined variable".
 */
class WalRemainingOperators {
public:
    static void register_all(OperatorRegistry& registry) {
        register_code_as_data(registry);
        register_trace_queries(registry);
        register_trace_authoring(registry);
        register_scope_statements(registry);
        register_conversions(registry);
        register_python_bridge(registry);
    }

private:
    static std::string name_of(const Value& v, const char* who) {
        if (v.is_symbol()) return v.to_symbol()->get_name();
        if (v.is_string()) return v.to_string();
        throw EvalError(std::string(who) + ": expected a symbol or a string");
    }

    /// Put every trace back where it was before a sweep.
    static void restore(SEval& eval, const std::unordered_map<std::string, int>& saved) {
        for (const auto& [tid, index] : saved) {
            eval.get_traces()->set_index(index, tid);
        }
    }

    // ---------------------------------------------------------------------
    // Running data as code
    // ---------------------------------------------------------------------

    static void register_code_as_data(OperatorRegistry& registry) {
        // (eval expr) -- evaluate the VALUE of expr as an expression. The argument is
        // evaluated once to obtain the form, then that form is evaluated.
        registry.register_operator("eval", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("eval: expects exactly one argument");
            }
            return eval.eval(eval.eval(args[0]));
        });

        // (parse "..." ...) -- read source text into an expression without running it.
        // Several arguments are wrapped in a single `do`, as upstream does.
        registry.register_operator("parse", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("parse: expects at least one argument (parse string+)");
            }
            auto evaluated = eval.eval_args(args);
            std::vector<Value> forms;
            Parser parser;
            for (const auto& arg : evaluated) {
                if (!arg.is_string()) {
                    throw EvalError("parse: all arguments must be strings (parse string+)");
                }
                for (const auto& form : parser.parse_program(arg.to_string())) {
                    forms.push_back(form);
                }
            }
            if (forms.size() == 1) {
                return forms[0];
            }
            auto sequence = std::make_shared<WList>();
            sequence->push_back(Value(std::make_shared<Symbol>("do")));
            for (const auto& form : forms) sequence->push_back(form);
            return Value(sequence);
        });

        // (eval-file "path") -- read a WAL file and run every form in it.
        registry.register_operator("eval-file", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("eval-file: expects exactly one argument (eval-file path:str)");
            }
            Value path = eval.eval(args[0]);
            if (!path.is_string()) {
                throw EvalError("eval-file: argument must evaluate to a string");
            }
            return run_file(eval, path.to_string(), "eval-file");
        });

        // (require module ...) -- run a module file once, so its definitions become
        // available. Upstream resolves a module through its own module system; here
        // the name is taken as a path, with a `.wal` suffix added when it has none,
        // and a module already required is not run again.
        registry.register_operator("require", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                throw EvalError("require: expects at least one module");
            }
            Value result = Value(nullptr);
            for (const auto& arg : args) {
                std::string name = arg.is_list() ? name_of(eval.eval(arg), "require")
                                                 : name_of(arg, "require");
                std::string path = name;
                if (path.size() < 4 || path.compare(path.size() - 4, 4, ".wal") != 0) {
                    path += ".wal";
                }
                if (!required().insert(path).second) {
                    continue;   // already required
                }
                result = run_file(eval, path, "require");
            }
            return result;
        });
    }

    /// Modules already run by `require`, so a second require is a no-op.
    static std::unordered_set<std::string>& required() {
        static std::unordered_set<std::string> seen;
        return seen;
    }

    static Value run_file(SEval& eval, const std::string& path, const char* who) {
        std::ifstream file(path);
        if (!file) {
            throw EvalError(std::string(who) + ": cannot open " + path);
        }
        std::ostringstream text;
        text << file.rdbuf();
        Parser parser;
        Value result = Value(nullptr);
        for (const auto& form : parser.parse_program(text.str())) {
            result = eval.eval(form);
        }
        return result;
    }

    // ---------------------------------------------------------------------
    // Trace queries
    // ---------------------------------------------------------------------

    static void register_trace_queries(OperatorRegistry& registry) {
        // (loaded-traces) -- the ids of every loaded trace.
        registry.register_operator("loaded-traces", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (!args.empty()) {
                throw EvalError("loaded-traces: expects no arguments");
            }
            auto results = std::make_shared<WList>();
            if (eval.get_traces()) {
                for (const auto& tid : eval.get_traces()->get_trace_ids()) {
                    results->push_back(Value(tid));
                }
            }
            return Value(results);
        });

        // (find/g condition) -- like `find`, but steps every trace together, so the
        // condition may span traces. `find` advances each trace on its own.
        registry.register_operator("find/g", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("find/g: expects exactly one argument (find/g condition)");
            }
            if (!eval.get_traces() || eval.get_traces()->empty()) {
                throw EvalError("find/g: no trace loaded");
            }
            auto saved = eval.get_traces()->indices();
            auto found = std::make_shared<WList>();
            try {
                auto ids = eval.get_traces()->get_trace_ids();
                auto first = eval.get_traces()->get_trace(ids.front());
                bool ended = false;
                while (!ended) {
                    if (eval.eval(args[0]).is_truthy()) {
                        found->push_back(Value(static_cast<int64_t>(first->get_index())));
                    }
                    // step() returns the tids that could not advance; with every
                    // trace moving together, any of them ending ends the sweep.
                    ended = !eval.get_traces()->step(1).empty();
                }
            } catch (...) {
                restore(eval, saved);
                throw;
            }
            restore(eval, saved);
            return Value(found);
        });
    }

    // ---------------------------------------------------------------------
    // Trace authoring
    // ---------------------------------------------------------------------

    static void register_trace_authoring(OperatorRegistry& registry) {
        // (trim-trace tid new-max-index) -- shorten a trace.
        registry.register_operator("trim-trace", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("trim-trace: expects two arguments (trim-trace tid max-index)");
            }
            std::string tid = name_of(eval.eval(args[0]), "trim-trace");
            Value limit = eval.eval(args[1]);
            if (!limit.is_int()) {
                throw EvalError("trim-trace: second argument must evaluate to an int");
            }
            auto trace = eval.get_traces() ? eval.get_traces()->get_trace(tid) : nullptr;
            if (!trace) {
                throw EvalError("trim-trace: no trace with id " + tid);
            }
            int new_max = static_cast<int>(limit.to_int());
            if (new_max < 0 || new_max > trace->get_max_index()) {
                throw EvalError("trim-trace: " + std::to_string(new_max) +
                                " is outside 0.." + std::to_string(trace->get_max_index()));
            }
            trace->set_max_index(new_max);
            if (trace->get_index() > new_max) {
                trace->set_index(new_max);
            }
            return Value(static_cast<int64_t>(new_max));
        });

        // (sample-at timestamps [tid]) -- restrict a trace to the listed indices.
        registry.register_operator("sample-at", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty() || args.size() > 2) {
                throw EvalError("sample-at: expects one or two arguments "
                                "(sample-at timestamps:list [trace])");
            }
            Value points = eval.eval(args[0]);
            if (!points.is_list()) {
                throw EvalError("sample-at: first argument must be a list of integers");
            }
            std::vector<int> indices;
            auto list = points.to_list();
            for (const auto& item : *list) {
                if (!item.is_int()) {
                    throw EvalError("sample-at: first argument must be a list of integers");
                }
                indices.push_back(static_cast<int>(item.to_int()));
            }
            if (!eval.get_traces() || eval.get_traces()->empty()) {
                throw EvalError("sample-at: no trace loaded");
            }
            std::vector<std::string> targets;
            if (args.size() == 2) {
                targets.push_back(name_of(eval.eval(args[1]), "sample-at"));
            } else {
                targets = eval.get_traces()->get_trace_ids();
            }
            for (const auto& tid : targets) {
                auto trace = eval.get_traces()->get_trace(tid);
                if (!trace) {
                    throw EvalError("sample-at: no trace with id " + tid);
                }
                trace->set_sampling_points(indices);
            }
            return Value(static_cast<int64_t>(indices.size()));
        });

        // (defsig name body...) -- a virtual signal, whose value is the body
        // evaluated at the current index. Named relative to the current scope or
        // group, as upstream does.
        registry.register_operator("defsig", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) {
                throw EvalError("defsig: expects at least two arguments (defsig name body+)");
            }
            std::string leaf = name_of(args[0], "defsig");
            // A group already carries its scope, so it is used alone when set.
            std::string prefix = eval.get_group();
            if (prefix.empty() && !eval.get_scope().empty()) {
                prefix = eval.get_scope() + Trace::SCOPE_SEPARATOR;
            }
            std::string full = prefix + leaf;

            auto body = std::make_shared<WList>();
            body->push_back(Value(std::make_shared<Symbol>("begin")));
            for (size_t i = 1; i < args.size(); ++i) body->push_back(args[i]);

            if (!eval.get_traces() || eval.get_traces()->empty()) {
                throw EvalError("defsig: no trace loaded");
            }
            for (const auto& tid : eval.get_traces()->get_trace_ids()) {
                auto trace = eval.get_traces()->get_trace(tid);
                if (trace) trace->set_virtual_signal(full, Value(body));
            }
            return Value(std::make_shared<Symbol>(full));
        });

        // (new-trace id max-index) -- an empty trace to write virtual signals into.
        registry.register_operator("new-trace", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("new-trace: expects two arguments (new-trace id max-index)");
            }
            std::string tid = name_of(args[0], "new-trace");
            Value limit = eval.eval(args[1]);
            if (!limit.is_int() || limit.to_int() < 0) {
                throw EvalError("new-trace: max-index must be a non-negative int");
            }
            if (!eval.get_traces()) {
                throw EvalError("new-trace: no trace container");
            }
            eval.get_traces()->new_virtual_trace(tid, static_cast<int>(limit.to_int()));
            return Value(std::make_shared<Symbol>(tid));
        });

        // (dump-trace tid [path]) -- write a trace out as VCD.
        registry.register_operator("dump-trace", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.empty() || args.size() > 2) {
                throw EvalError("dump-trace: expects one or two arguments (dump-trace tid [path])");
            }
            std::string tid = name_of(eval.eval(args[0]), "dump-trace");
            auto trace = eval.get_traces() ? eval.get_traces()->get_trace(tid) : nullptr;
            if (!trace) {
                throw EvalError("dump-trace: no trace with id " + tid);
            }
            std::string path;
            if (args.size() == 2) {
                Value target = eval.eval(args[1]);
                if (!target.is_string()) {
                    throw EvalError("dump-trace: path must evaluate to a string");
                }
                path = target.to_string();
            }
            std::string written = trace->dump_vcd(path);
            return Value(written);
        });
    }

    // ---------------------------------------------------------------------
    // Scope as a statement
    // ---------------------------------------------------------------------

    static void register_scope_statements(OperatorRegistry& registry) {
        // (set-scope scope) -- the imperative form of in-scope, for a REPL session
        // where wrapping everything in a body would be awkward.
        registry.register_operator("set-scope", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("set-scope: expects exactly one argument (set-scope scope)");
            }
            std::string scope = name_of(eval.eval(args[0]), "set-scope");
            eval.set_scope(scope);
            eval.get_global_environment()->define("CS", Value(scope));
            return Value(scope);
        });

        registry.register_operator("unset-scope", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (!args.empty()) {
                throw EvalError("unset-scope: expects no arguments");
            }
            eval.set_scope("");
            eval.get_global_environment()->define("CS", Value(std::string()));
            return Value(nullptr);
        });
    }

    // ---------------------------------------------------------------------
    // Conversion
    // ---------------------------------------------------------------------

    static void register_conversions(OperatorRegistry& registry) {
        // (bits->sint "1011") -- read a bit string as two's complement.
        //
        // This is the operator that makes a wide signal useful: the VCD reader hands
        // back anything over 64 bits as a bit string, and this is how that string
        // becomes a signed number.
        registry.register_operator("bits->sint", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("bits->sint: expects exactly one argument (bits->sint expr:str)");
            }
            Value v = eval.eval(args[0]);
            if (!v.is_string()) {
                throw EvalError("bits->sint: argument must evaluate to a string");
            }
            const std::string& bits = v.to_string();
            if (bits.empty()) {
                throw EvalError("bits->sint: argument must not be empty");
            }
            for (char c : bits) {
                if (c != '0' && c != '1') {
                    throw EvalError("bits->sint: argument must contain only 0 and 1");
                }
            }
            if (bits.size() > 64) {
                throw EvalError("bits->sint: " + std::to_string(bits.size()) +
                                " bits does not fit in a 64-bit integer");
            }
            // A leading 1 means negative: invert, add one, negate.
            if (bits[0] == '1') {
                uint64_t magnitude = 0;
                for (char c : bits) {
                    magnitude = (magnitude << 1) | (c == '0' ? 1ULL : 0ULL);
                }
                magnitude += 1;
                return Value(-static_cast<int64_t>(magnitude));
            }
            uint64_t value = 0;
            for (char c : bits) {
                value = (value << 1) | (c == '1' ? 1ULL : 0ULL);
            }
            return Value(static_cast<int64_t>(value));
        });
    }

    // ---------------------------------------------------------------------
    // The CPython bridge, which has no C++ equivalent
    // ---------------------------------------------------------------------

    static void register_python_bridge(OperatorRegistry& registry) {
        // Upstream runs inside CPython, so `(import numpy)` and
        // `(call numpy.mean xs)` reach straight into the host interpreter. WAL++ is
        // a C++ program with no interpreter to reach into.
        //
        // They are registered anyway so the failure says what is actually wrong. Left
        // unregistered they would report "Undefined variable: import", which reads
        // like a missing operator rather than a capability the engine cannot have.
        auto unsupported = [](const char* name, const char* advice) {
            std::string message = std::string(name) +
                ": not available in WAL++. Upstream runs inside CPython, so this "
                "operator calls into the host Python interpreter; WAL++ is a C++ "
                "program and has none. " + advice;
            return [message](SEval&, const std::vector<Value>&) -> Value {
                throw EvalError(message);
            };
        };

        registry.register_operator("import", unsupported(
            "import",
            "Use the Python bindings (import wal_cpp) to drive WAL++ from Python "
            "instead of the other way round."));

        registry.register_operator("call", unsupported(
            "call",
            "Compute the value in Python around the WAL++ call, or add the operation "
            "as a WAL++ operator."));
    }
};

} // namespace operators
} // namespace wal
