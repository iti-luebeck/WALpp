#ifndef WAL_PYTHON_TYPES_HPP
#define WAL_PYTHON_TYPES_HPP

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "wal/ast.h"

namespace py = pybind11;
namespace wal {

// ═══════════════════════════════════════════════════════════════════
// Python Type Conversion Utilities
// ═══════════════════════════════════════════════════════════════════

/**
 * @brief Convert Python object to WAL Value
 * 
 * Handles automatic conversion from Python types to WAL values:
 * - int, float, str, bool -> direct conversion
 * - list -> converts to WList
 * - None -> nil Value
 */
Value python_to_value(const py::object& obj);

/**
 * @brief Convert WAL Value to Python object
 * 
 * Handles automatic conversion from WAL values to Python types:
 * - int/float/string/bool -> direct conversion
 * - list -> list of Python objects
 * - symbol -> string representation
 * - nil -> None
 */
py::object value_to_python(const Value& val);

/**
 * @brief Convert Python list to WAL WList
 */
std::shared_ptr<WList> python_list_to_wlist(const py::list& py_list);

/**
 * @brief Convert WList to Python list
 */
py::list wlist_to_python_list(const std::shared_ptr<WList>& wlist);

}  // namespace wal

#endif  // WAL_PYTHON_TYPES_HPP
