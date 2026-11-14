#include "wal/ast.h"
#include <iomanip>
#include <sstream>

namespace wal {

// ============================================================================
// Value Implementation
// ============================================================================

Value::Value() : data(nullptr) {}
Value::Value(std::nullptr_t) : data(nullptr) {}
Value::Value(bool b) : data(b) {}
Value::Value(int64_t i) : data(i) {}
Value::Value(double d) : data(d) {}
Value::Value(const std::string& s) : data(s) {}
Value::Value(std::string_view s) : data(std::string(s)) {}
Value::Value(const char* s) : data(std::string(s)) {}
Value::Value(std::shared_ptr<Symbol> sym) : data(sym) {}
Value::Value(std::shared_ptr<WList> list) : data(list) {}
Value::Value(std::shared_ptr<Closure> closure) : data(closure) {}
Value::Value(std::shared_ptr<Macro> macro) : data(macro) {}
Value::Value(std::shared_ptr<Environment> env) : data(env) {}
Value::Value(std::shared_ptr<UserOperator> op) : data(op) {}

// Type checking
bool Value::is_nil() const {
    return std::holds_alternative<std::nullptr_t>(data);
}

bool Value::is_bool() const {
    return std::holds_alternative<bool>(data);
}

bool Value::is_int() const {
    return std::holds_alternative<int64_t>(data);
}

bool Value::is_float() const {
    return std::holds_alternative<double>(data);
}

bool Value::is_number() const {
    return is_int() || is_float();
}

bool Value::is_string() const {
    return std::holds_alternative<std::string>(data);
}

bool Value::is_symbol() const {
    return std::holds_alternative<std::shared_ptr<Symbol>>(data);
}

bool Value::is_list() const {
    return std::holds_alternative<std::shared_ptr<WList>>(data);
}

bool Value::is_closure() const {
    return std::holds_alternative<std::shared_ptr<Closure>>(data);
}

bool Value::is_macro() const {
    return std::holds_alternative<std::shared_ptr<Macro>>(data);
}

bool Value::is_environment() const {
    return std::holds_alternative<std::shared_ptr<Environment>>(data);
}

bool Value::is_user_operator() const {
    return std::holds_alternative<std::shared_ptr<UserOperator>>(data);
}

// Type extraction with exceptions
bool Value::to_bool() const {
    if (!is_bool()) {
        throw EvalError("Expected boolean, got " + to_string_repr());
    }
    return std::get<bool>(data);
}

int64_t Value::to_int() const {
    if (is_int()) {
        return std::get<int64_t>(data);
    } else if (is_float()) {
        return static_cast<int64_t>(std::get<double>(data));
    }
    throw EvalError("Expected number, got " + to_string_repr());
}

double Value::to_float() const {
    if (is_float()) {
        return std::get<double>(data);
    } else if (is_int()) {
        return static_cast<double>(std::get<int64_t>(data));
    }
    throw EvalError("Expected number, got " + to_string_repr());
}

const std::string& Value::to_string() const {
    if (!is_string()) {
        throw EvalError("Expected string, got " + to_string_repr());
    }
    return std::get<std::string>(data);
}

std::shared_ptr<Symbol> Value::to_symbol() const {
    if (!is_symbol()) {
        throw EvalError("Expected symbol, got " + to_string_repr());
    }
    return std::get<std::shared_ptr<Symbol>>(data);
}

std::shared_ptr<WList> Value::to_list() const {
    if (!is_list()) {
        throw EvalError("Expected list, got " + to_string_repr());
    }
    return std::get<std::shared_ptr<WList>>(data);
}

std::shared_ptr<Closure> Value::to_closure() const {
    if (!is_closure()) {
        throw EvalError("Expected closure, got " + to_string_repr());
    }
    return std::get<std::shared_ptr<Closure>>(data);
}

std::shared_ptr<Macro> Value::to_macro() const {
    if (!is_macro()) {
        throw EvalError("Expected macro, got " + to_string_repr());
    }
    return std::get<std::shared_ptr<Macro>>(data);
}

std::shared_ptr<Environment> Value::to_environment() const {
    if (!is_environment()) {
        throw EvalError("Expected environment, got " + to_string_repr());
    }
    return std::get<std::shared_ptr<Environment>>(data);
}

// Conversions
std::string Value::to_string_repr() const {
    if (is_nil()) {
        return "nil";
    } else if (is_bool()) {
        return std::get<bool>(data) ? "#t" : "#f";
    } else if (is_int()) {
        return std::to_string(std::get<int64_t>(data));
    } else if (is_float()) {
        std::ostringstream oss;
        oss << std::setprecision(10) << std::get<double>(data);
        return oss.str();
    } else if (is_string()) {
        std::string escaped = std::get<std::string>(data);
        // Escape special characters
        size_t pos = 0;
        while ((pos = escaped.find('\\', pos)) != std::string::npos) {
            escaped.replace(pos, 1, "\\\\");
            pos += 2;
        }
        pos = 0;
        while ((pos = escaped.find('"', pos)) != std::string::npos) {
            escaped.replace(pos, 1, "\\\"");
            pos += 2;
        }
        return "\"" + escaped + "\"";
    } else if (is_symbol()) {
        return std::get<std::shared_ptr<Symbol>>(data)->get_name();
    } else if (is_list()) {
        return std::get<std::shared_ptr<WList>>(data)->to_string();
    } else if (is_closure()) {
        return std::get<std::shared_ptr<Closure>>(data)->to_string();
    } else if (is_macro()) {
        return std::get<std::shared_ptr<Macro>>(data)->to_string();
    } else if (is_environment()) {
        return std::get<std::shared_ptr<Environment>>(data)->to_string();
    } else if (is_user_operator()) {
        return std::get<std::shared_ptr<UserOperator>>(data)->to_string();
    }
    return "<unknown>";
}

bool Value::is_truthy() const {
    // WAL's notion of truth, which is the one the signal values in a waveform need:
    // zero is false. Treating every non-bool as true made `(if 0 ...)` take the then
    // branch, `(! 0)` yield false, and `(&& 1 0)` yield true, so any condition written
    // directly on a signal silently did the opposite of what it said. The upstream
    // reference (ics-jku/wal 0.9.1) was used to pin the rules:
    //   0, 0.0, "", () and #f are false; everything else, including a symbol, is true.
    if (is_nil()) return false;
    if (is_bool()) return std::get<bool>(data);
    if (is_int()) return std::get<int64_t>(data) != 0;
    if (is_float()) return std::get<double>(data) != 0.0;
    if (is_string()) return !std::get<std::string>(data).empty();
    if (is_list()) {
        auto list = std::get<std::shared_ptr<WList>>(data);
        return list && !list->empty();
    }
    return true; // symbols, operators and anything else
}

bool Value::operator==(const Value& other) const {
    if (data.index() != other.data.index()) return false;
    
    if (is_nil()) return true;
    if (is_bool()) return std::get<bool>(data) == std::get<bool>(other.data);
    if (is_int()) return std::get<int64_t>(data) == std::get<int64_t>(other.data);
    if (is_float()) {
        double a = std::get<double>(data);
        double b = std::get<double>(other.data);
        return std::abs(a - b) < 1e-10;
    }
    if (is_string()) return std::get<std::string>(data) == std::get<std::string>(other.data);
    if (is_symbol()) {
        auto a = std::get<std::shared_ptr<Symbol>>(data);
        auto b = std::get<std::shared_ptr<Symbol>>(other.data);
        return a->get_name() == b->get_name();
    }
    // For reference types (list, closure, etc.), use pointer equality
    return data.index() == other.data.index();
}

bool Value::operator!=(const Value& other) const {
    return !(*this == other);
}

// ============================================================================
// Symbol Implementation
// ============================================================================

Symbol::Symbol(const std::string& name) : name(name), scope(""), group("") {}

Symbol::Symbol(const std::string& name, const std::string& scope)
    : name(name), scope(scope), group("") {}

std::string Symbol::full_name() const {
    std::string result;
    if (!scope.empty()) {
        result += "~" + scope + "/";
    }
    if (!group.empty()) {
        result += "#" + group + "/";
    }
    result += name;
    return result;
}

bool Symbol::operator==(const Symbol& other) const {
    return name == other.name && scope == other.scope && group == other.group;
}

// ============================================================================
// WList Implementation
// ============================================================================

std::string WList::to_string() const {
    std::ostringstream oss;
    oss << "(";
    for (size_t i = 0; i < size(); ++i) {
        if (i > 0) oss << " ";
        oss << (*this)[i].to_string_repr();
    }
    oss << ")";
    return oss.str();
}

// ============================================================================
// Closure Implementation
// ============================================================================

Closure::Closure(std::shared_ptr<Environment> env, const Value& args,
                 const Value& expr, const std::string& name)
    : environment(env), args(args), expression(expr), name(name) {}

std::string Closure::to_string() const {
    return "#<closure:" + name + ">";
}

// ============================================================================
// Macro Implementation
// ============================================================================

Macro::Macro(std::shared_ptr<Environment> env, const Value& args,
             const Value& expr, const std::string& name)
    : environment(env), args(args), expression(expr), name(name) {}

std::string Macro::to_string() const {
    return "#<macro:" + name + ">";
}

// ============================================================================
// UserOperator Implementation
// ============================================================================

UserOperator::UserOperator(const std::string& name, const std::string& module)
    : name(name), module(module) {}

std::string UserOperator::to_string() const {
    if (module.empty()) {
        return "#<operator:" + name + ">";
    }
    return "#<operator:" + module + "/" + name + ">";
}

// ============================================================================
// Environment Implementation
// ============================================================================

Environment::Environment() : parent(nullptr) {}

Environment::Environment(std::shared_ptr<Environment> parent)
    : parent(parent) {}

Value Environment::get(const std::string& name) const {
    auto it = bindings.find(name);
    if (it != bindings.end()) {
        return it->second;
    }
    if (parent) {
        return parent->get(name);
    }
    throw EvalError("Undefined variable: " + name);
}

void Environment::define(const std::string& name, const Value& value) {
    bindings[name] = value;
}

void Environment::write(const std::string& name, const Value& value) {
    auto it = bindings.find(name);
    if (it != bindings.end()) {
        it->second = value;
        return;
    }
    if (parent) {
        parent->write(name, value);
        return;
    }
    throw EvalError("Cannot write to undefined variable: " + name);
}

void Environment::undefine(const std::string& name) {
    auto it = bindings.find(name);
    if (it != bindings.end()) {
        bindings.erase(it);
    }
}

bool Environment::is_defined(const std::string& name) const {
    if (bindings.count(name) > 0) {
        return true;
    }
    if (parent) {
        return parent->is_defined(name);
    }
    return false;
}

std::string Environment::to_string() const {
    return "#<environment>";
}

} // namespace wal

