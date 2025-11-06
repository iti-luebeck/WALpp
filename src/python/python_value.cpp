#ifdef PYBIND11_MODULE

#include "wal/python/python_value.h"

namespace wal {

// ═══════════════════════════════════════════════════════════════════
// Python Value Implementation
// ═══════════════════════════════════════════════════════════════════

std::shared_ptr<Value> make_value_shared(const Value& v) {
    return std::make_shared<Value>(v);
}

void register_value_type(py::module_& m) {
    // Value type is registered in python_module.cpp
    // This function is a placeholder for future enhancements
}

}  // namespace wal

#endif  // PYBIND11_MODULE
