#include "wal/operator_registry.h"
#include "wal/evaluator.h"

namespace wal {

void OperatorRegistry::register_operator(const std::string& name, OperatorFunc func) {
    operators[name] = func;
}

std::optional<OperatorFunc> OperatorRegistry::get_operator(const std::string& name) const {
    auto it = operators.find(name);
    if (it != operators.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool OperatorRegistry::has_operator(const std::string& name) const {
    return operators.count(name) > 0;
}

std::vector<std::string> OperatorRegistry::get_all_operators() const {
    std::vector<std::string> result;
    for (const auto& [name, _] : operators) {
        result.push_back(name);
    }
    return result;
}

} // namespace wal

