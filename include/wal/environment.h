#pragma once

#include "ast.h"
#include <unordered_map>
#include <memory>
#include <string>

namespace wal {

/**
 * @brief Variable binding environment with scope chain support
 * 
 * Environments form a chain where each environment has an optional parent.
 * Variable lookup follows the chain: local -> parent -> grandparent, etc.
 * Variable definition goes to the local environment unless explicitly targeting parent.
 */
inline Value Environment::get(const std::string& name) const {
    auto it = bindings.find(name);
    if (it != bindings.end()) {
        return it->second;
    }
    if (parent) {
        return parent->get(name);
    }
    throw EvalError("Undefined variable: " + name);
}

inline void Environment::define(const std::string& name, const Value& value) {
    bindings[name] = value;
}

inline void Environment::write(const std::string& name, const Value& value) {
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

inline void Environment::undefine(const std::string& name) {
    auto it = bindings.find(name);
    if (it != bindings.end()) {
        bindings.erase(it);
    }
}

inline bool Environment::is_defined(const std::string& name) const {
    if (bindings.count(name) > 0) {
        return true;
    }
    if (parent) {
        return parent->is_defined(name);
    }
    return false;
}

} // namespace wal
