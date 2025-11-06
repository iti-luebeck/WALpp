#ifdef PYBIND11_MODULE

#include "wal/python/python_types.h"

namespace wal {

// ═══════════════════════════════════════════════════════════════════
// Type Conversion Implementations
// ═══════════════════════════════════════════════════════════════════

Value python_to_value(const py::object& obj) {
    // Handle None
    if (obj.is_none()) {
        return Value();
    }
    
    // Handle bool (must be before int, since bool is subclass of int)
    if (py::isinstance<py::bool_>(obj)) {
        return Value(py::cast<bool>(obj));
    }
    
    // Handle int
    if (py::isinstance<py::int_>(obj)) {
        return Value(py::cast<int64_t>(obj));
    }
    
    // Handle float
    if (py::isinstance<py::float_>(obj)) {
        return Value(py::cast<double>(obj));
    }
    
    // Handle string
    if (py::isinstance<py::str>(obj)) {
        return Value(py::cast<std::string>(obj));
    }
    
    // Handle list
    if (py::isinstance<py::list>(obj)) {
        return Value(python_list_to_wlist(py::cast<py::list>(obj)));
    }
    
    // Handle WAL Value directly
    if (py::isinstance<Value>(obj)) {
        return py::cast<Value>(obj);
    }
    
    // Handle WAL List directly
    if (py::isinstance<WList>(obj)) {
        return Value(py::cast<std::shared_ptr<WList>>(obj));
    }
    
    // Default: return nil
    return Value();
}

py::object value_to_python(const Value& val) {
    if (val.is_nil()) {
        return py::none();
    }
    
    if (val.is_bool()) {
        return py::bool_(val.to_bool());
    }
    
    if (val.is_int()) {
        return py::int_(val.to_int());
    }
    
    if (val.is_float()) {
        return py::float_(val.to_float());
    }
    
    if (val.is_string()) {
        return py::str(val.to_string());
    }
    
    if (val.is_symbol()) {
        return py::str(val.to_symbol()->get_name());
    }
    
    if (val.is_list()) {
        return py::cast(wlist_to_python_list(val.to_list()));
    }
    
    // Default: return the value's string representation
    return py::str(val.to_string_repr());
}

std::shared_ptr<WList> python_list_to_wlist(const py::list& py_list) {
    auto wlist = std::make_shared<WList>();
    
    for (const auto& item : py_list) {
        wlist->push_back(python_to_value(item));
    }
    
    return wlist;
}

py::list wlist_to_python_list(const std::shared_ptr<WList>& wlist) {
    py::list py_list;
    
    if (!wlist) {
        return py_list;
    }
    
    for (const auto& item : *wlist) {
        py_list.append(value_to_python(item));
    }
    
    return py_list;
}

}  // namespace wal

#endif  // PYBIND11_MODULE
