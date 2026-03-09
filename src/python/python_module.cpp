#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include "wal/core.h"
#include "wal/operator_registry.h"
#include "wal/evaluator.h"

namespace py = pybind11;
using namespace wal;

// Forward declarations for custom holders
namespace pybind11 { namespace detail {
    template <>
    struct holder_helper<std::shared_ptr<Environment>> {
        static const auto* get(const std::shared_ptr<Environment>& p) { return p.get(); }
    };
}}

// ═══════════════════════════════════════════════════════════════════
// PYBIND11 MODULE DEFINITION
// ═══════════════════════════════════════════════════════════════════

PYBIND11_MODULE(wal_cpp, m) {
    m.doc() = "WAL++ (Waveform Analysis Language in C++20) Python bindings";

    // ───────────────────────────────────────────────────────────────
    // Value Class Bindings
    // ───────────────────────────────────────────────────────────────
    
    py::class_<Value, std::shared_ptr<Value>>(m, "Value")
        .def(py::init<>(), "Create nil value")
        .def(py::init<bool>(), "Create boolean value")
        .def(py::init<int64_t>(), "Create integer value")
        .def(py::init<double>(), "Create float value")
        .def(py::init<std::string>(), "Create string value")
        .def(py::init<std::shared_ptr<WList>>(), "Create list value")
        
        // Type checking methods
        .def("is_nil", &Value::is_nil, "Check if nil")
        .def("is_bool", &Value::is_bool, "Check if boolean")
        .def("is_int", &Value::is_int, "Check if integer")
        .def("is_float", &Value::is_float, "Check if float")
        .def("is_string", &Value::is_string, "Check if string")
        .def("is_symbol", &Value::is_symbol, "Check if symbol")
        .def("is_list", &Value::is_list, "Check if list")
        .def("is_number", &Value::is_number, "Check if numeric (int or float)")
        
        // Conversion methods
        .def("to_bool", &Value::to_bool, "Convert to boolean")
        .def("to_int", &Value::to_int, "Convert to integer")
        .def("to_float", &Value::to_float, "Convert to float")
        .def("to_string", &Value::to_string, "Convert to string")
        .def("to_symbol", &Value::to_symbol, "Convert to symbol (returns Symbol object)")
        .def("to_list", &Value::to_list, "Convert to list (returns shared_ptr<WList>)")
        .def("to_string_repr", &Value::to_string_repr, "Get string representation")
        
        // Python special methods
        .def("__str__", &Value::to_string_repr)
        .def("__repr__", [](const Value& v) {
            return "#<Value: " + v.to_string_repr() + ">";
        })
        .def("__bool__", &Value::to_bool)
        .def("__eq__", [](const Value& a, const Value& b) {
            return a.to_string_repr() == b.to_string_repr();
        });

    // ───────────────────────────────────────────────────────────────
    // Symbol Class Bindings
    // ───────────────────────────────────────────────────────────────
    
    py::class_<Symbol, std::shared_ptr<Symbol>>(m, "Symbol")
        .def(py::init<const std::string&>(), "Create symbol")
        .def("get_name", &Symbol::get_name, "Get symbol name")
        .def("__str__", &Symbol::get_name)
        .def("__repr__", [](const Symbol& s) {
            return "#<Symbol: " + s.get_name() + ">";
        });

    // ───────────────────────────────────────────────────────────────
    // WList Class Bindings (Vector wrapper)
    // ───────────────────────────────────────────────────────────────
    
    py::class_<WList, std::shared_ptr<WList>>(m, "List")
        .def(py::init<>(), "Create empty list")
        .def("__len__", [](const WList& self) { return self.size(); })
        .def("__getitem__", [](WList& self, size_t idx) -> std::shared_ptr<Value> {
            if (idx >= self.size()) throw py::index_error("list index out of range");
            return std::make_shared<Value>(self[idx]);
        })
        .def("__setitem__", [](WList& self, size_t idx, const std::shared_ptr<Value>& v) {
            if (idx >= self.size()) throw py::index_error("list index out of range");
            self[idx] = *v;
        })
        .def("__iter__", [](WList& self) {
            return py::make_iterator(self.begin(), self.end());
        }, py::keep_alive<0, 1>())
        .def("append", [](WList& self, const std::shared_ptr<Value>& v) {
            self.push_back(*v);
        }, "Append value to list")
        .def("__repr__", [](const WList& self) {
            std::string result = "(";
            for (size_t i = 0; i < self.size(); ++i) {
                if (i > 0) result += " ";
                result += self[i].to_string_repr();
            }
            result += ")";
            return result;
        });    // ───────────────────────────────────────────────────────────────
    // Environment Class Bindings
    // ───────────────────────────────────────────────────────────────
    
    py::class_<Environment, std::shared_ptr<Environment>>(m, "Environment")
        .def(py::init<std::shared_ptr<Environment>>(), py::arg("parent") = nullptr,
             "Create environment with optional parent")
        .def("get", &Environment::get, "Get binding value", py::arg("name"))
        .def("define", &Environment::define, "Define new binding", py::arg("name"), py::arg("value"))
        .def("write", &Environment::write, "Update binding value", py::arg("name"), py::arg("value"))
        .def("undefine", &Environment::undefine, "Remove binding", py::arg("name"))
        .def("is_defined", &Environment::is_defined, "Check if binding exists", py::arg("name"));

    // ───────────────────────────────────────────────────────────────
    // OperatorRegistry Class Bindings
    // ───────────────────────────────────────────────────────────────
    
    py::class_<OperatorRegistry, std::shared_ptr<OperatorRegistry>>(m, "OperatorRegistry")
        .def(py::init<>(), "Create operator registry")
        .def("register_operator", 
            [](OperatorRegistry& self, const std::string& name, 
               py::object py_func) {
                // Wrap Python function as C++ lambda
                auto lambda = [py_func](SEval& eval, const std::vector<Value>& args) -> Value {
                    try {
                        // Convert C++ vector<Value> to Python list
                        py::list py_args;
                        for (const auto& arg : args) {
                            py_args.append(std::make_shared<Value>(arg));
                        }
                        
                        // Call Python function
                        py::object result = py_func(nullptr, py_args);
                        
                        // Convert Python result back to Value
                        if (py::isinstance<Value>(result)) {
                            return *py::cast<std::shared_ptr<Value>>(result);
                        } else if (py::isinstance<py::int_>(result)) {
                            return Value(py::cast<int64_t>(result));
                        } else if (py::isinstance<py::float_>(result)) {
                            return Value(py::cast<double>(result));
                        } else if (py::isinstance<py::str>(result)) {
                            return Value(py::cast<std::string>(result));
                        } else if (py::isinstance<py::bool_>(result)) {
                            return Value(py::cast<bool>(result));
                        } else if (py::isinstance<WList>(result)) {
                            return Value(py::cast<std::shared_ptr<WList>>(result));
                        } else {
                            return Value();  // nil
                        }
                    } catch (const std::exception& e) {
                        throw EvalError(std::string("Python operator error: ") + e.what());
                    }
                };
                
                self.register_operator(name, lambda);
            },
            "Register operator from Python callable", py::arg("name"), py::arg("function"))
        
        .def("has_operator", &OperatorRegistry::has_operator,
             "Check if operator exists", py::arg("name"))
        .def("get_all_operators", &OperatorRegistry::get_all_operators,
             "Get all registered operator names")
        .def("size", &OperatorRegistry::size,
             "Get number of registered operators");

    // ───────────────────────────────────────────────────────────────
    // SEval (Evaluator) Class Bindings
    // ───────────────────────────────────────────────────────────────
    
    py::class_<SEval, std::shared_ptr<SEval>>(m, "Evaluator")
        .def("eval", [](SEval& self, const std::shared_ptr<Value>& expr) {
            return self.eval(*expr);
        }, "Evaluate an S-expression", py::arg("expr"))
        .def("get_operator_registry", [](SEval& self) {
            return std::shared_ptr<OperatorRegistry>(&self.get_operator_registry(), [](void*){});
        }, "Get the operator registry (shared_ptr)")
        .def("get_environment", &SEval::get_environment,
             "Get the current environment");

    // ───────────────────────────────────────────────────────────────
    // Wal Core Class Bindings
    // ───────────────────────────────────────────────────────────────
    
    py::class_<Wal, std::shared_ptr<Wal>>(m, "Wal")
        .def(py::init<>(), "Create WAL instance")
        
        // Core evaluation methods
        .def("eval_str", &Wal::eval_str,
             "Evaluate WAL code from string", py::arg("code"))
        .def("eval", &Wal::eval,
             "Evaluate S-expression Value", py::arg("expr"))
        .def("run_str", &Wal::run_str,
             "Run WAL code with fresh context", py::arg("code"))
        .def("run", &Wal::run,
             "Run S-expression with fresh context", py::arg("expr"))
        .def("run_file", &Wal::run_file,
             "Load and execute WAL program from file", py::arg("filename"))
        .def("start_repl", &Wal::start_repl,
             "Start interactive REPL")
        
        // Trace methods
        .def("load", &Wal::load,
             "Load waveform file", py::arg("file"), py::arg("tid") = "DEFAULT")
        .def("unload", &Wal::unload,
             "Unload trace", py::arg("tid") = "DEFAULT")
        .def("step", &Wal::step,
             "Step traces forward/backward", py::arg("steps") = 1, py::arg("tid") = "")
        
        // Operator registry access
        .def("get_operator_registry", [](Wal& self) {
            return std::shared_ptr<OperatorRegistry>(
                &self.get_eval_context()->get_operator_registry(), 
                [](void*){}  // No-op deleter since it's held by SEval
            );
        }, "Get operator registry for dynamic registration")
        
        .def("register_operator",
            [](Wal& self, const std::string& name, py::object py_func) {
                auto registry_ptr = &self.get_eval_context()->get_operator_registry();
                auto lambda_func = [py_func](SEval& eval, const std::vector<Value>& args) -> Value {
                    try {
                        py::list py_args;
                        for (const auto& arg : args) {
                            py_args.append(std::make_shared<Value>(arg));
                        }
                        py::object result = py_func(nullptr, py_args);
                        
                        if (py::isinstance<Value>(result)) {
                            return *py::cast<std::shared_ptr<Value>>(result);
                        } else if (py::isinstance<py::int_>(result)) {
                            return Value(py::cast<int64_t>(result));
                        } else if (py::isinstance<py::float_>(result)) {
                            return Value(py::cast<double>(result));
                        } else if (py::isinstance<py::str>(result)) {
                            return Value(py::cast<std::string>(result));
                        } else if (py::isinstance<py::bool_>(result)) {
                            return Value(py::cast<bool>(result));
                        } else if (py::isinstance<WList>(result)) {
                            return Value(py::cast<std::shared_ptr<WList>>(result));
                        } else {
                            return Value();
                        }
                    } catch (const std::exception& e) {
                        throw EvalError(std::string("Python operator error: ") + e.what());
                    }
                };
                registry_ptr->register_operator(name, lambda_func);
            },
            "Register custom operator from Python", py::arg("name"), py::arg("function"))
        
        // Environment access
        .def("get_eval_context", &Wal::get_eval_context,
             "Get evaluator instance")
        .def("get_traces", &Wal::get_traces,
             "Get trace container")
        
        // Variable binding convenience methods
        .def("define", [](Wal& self, const std::string& name, const std::shared_ptr<Value>& value) {
            auto env = self.get_eval_context()->get_environment();
            env->define(name, *value);
        }, "Define variable in global environment", py::arg("name"), py::arg("value"))
        
        .def("get_var", [](Wal& self, const std::string& name) -> std::shared_ptr<Value> {
            auto env = self.get_eval_context()->get_environment();
            return std::make_shared<Value>(env->get(name));
        }, "Get variable from global environment", py::arg("name"))
        
        .def("set_var", [](Wal& self, const std::string& name, const std::shared_ptr<Value>& value) {
            auto env = self.get_eval_context()->get_environment();
            env->write(name, *value);
        }, "Set variable in global environment", py::arg("name"), py::arg("value"))
        
        // Python convenience methods
        .def("__call__", &Wal::eval_str,
             "Evaluate code (enables w(\"code\") syntax)", py::arg("code"));

    // ───────────────────────────────────────────────────────────────
    // Exception Bindings
    // ───────────────────────────────────────────────────────────────
    
    py::register_exception<EvalError>(m, "EvalError");

    // ───────────────────────────────────────────────────────────────
    // Helper Functions
    // ───────────────────────────────────────────────────────────────
    
    m.def("parse_string", [](const std::string& code) -> std::shared_ptr<Value> {
        // Parse a WAL string expression into a Value
        // This will use the parser when it's implemented
        // For now, return a simple list representation
        auto result = std::make_shared<WList>();
        result->push_back(Value(std::make_shared<Symbol>(code)));
        return std::make_shared<Value>(result);
    }, "Parse WAL string into Value", py::arg("code"));

    m.def("make_list", []() -> std::shared_ptr<WList> {
        return std::make_shared<WList>();
    }, "Create empty WAL list");

    m.def("make_value_int", [](int64_t i) -> std::shared_ptr<Value> {
        return std::make_shared<Value>(i);
    }, "Create integer Value", py::arg("value"));

    m.def("make_value_float", [](double d) -> std::shared_ptr<Value> {
        return std::make_shared<Value>(d);
    }, "Create float Value", py::arg("value"));

    m.def("make_value_string", [](const std::string& s) -> std::shared_ptr<Value> {
        return std::make_shared<Value>(s);
    }, "Create string Value", py::arg("value"));

    m.def("make_value_symbol", [](const std::string& s) -> std::shared_ptr<Value> {
        return std::make_shared<Value>(std::make_shared<Symbol>(s));
    }, "Create symbol Value", py::arg("name"));

    m.def("make_value_list", [](const std::shared_ptr<WList>& list) -> std::shared_ptr<Value> {
        return std::make_shared<Value>(list);
    }, "Create list Value", py::arg("list"));

    // ───────────────────────────────────────────────────────────────
    // Module Information
    // ───────────────────────────────────────────────────────────────
    
    m.attr("__version__") = "1.0.0";
    m.attr("__author__") = "WAL++ Development Team";
}
