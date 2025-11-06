#ifndef WAL_PYTHON_VALUE_HPP
#define WAL_PYTHON_VALUE_HPP

#include <pybind11/pybind11.h>
#include "wal/ast.h"

namespace py = pybind11;
namespace wal {

// ═══════════════════════════════════════════════════════════════════
// Python Value Binding Utilities
// ═══════════════════════════════════════════════════════════════════

/**
 * @brief Custom Value holder for Python-C++ interop
 * 
 * Provides shared_ptr management for Value objects in Python,
 * ensuring proper reference counting and lifetime management.
 */
class ValueHolder {
public:
    explicit ValueHolder(const Value& v) : value(v) {}
    explicit ValueHolder(Value&& v) : value(std::move(v)) {}
    
    Value get() const { return value; }
    void set(const Value& v) { value = v; }
    
private:
    Value value;
};

/**
 * @brief Register Value with pybind11
 * 
 * Must be called during module initialization to expose Value
 * with proper holder type and conversion operators.
 */
void register_value_type(py::module_& m);

/**
 * @brief Create shared_ptr wrapper for Value
 * 
 * Useful for returning Value from C++ to Python
 * as it maintains reference counting across language boundary.
 */
std::shared_ptr<Value> make_value_shared(const Value& v);

}  // namespace wal

#endif  // WAL_PYTHON_VALUE_HPP
