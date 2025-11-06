#include "wal/trace/trace.h"
#include "wal/ast.h"

namespace wal {

std::vector<std::string> Trace::get_scopes() const {
    return scope_names;
}

void Trace::set_index(int idx) {
    if (is_valid_index(idx)) {
        current_index = idx;
    } else {
        throw std::out_of_range("Index out of range: " + std::to_string(idx));
    }
}

bool Trace::step(int steps) {
    int new_index = current_index + steps;
    if (is_valid_index(new_index)) {
        current_index = new_index;
        return true;
    }
    return false;
}

Value Trace::get_special_signal(const std::string& name) {
    if (name == INDEX) {
        return Value(current_index);
    } else if (name == MAX_INDEX) {
        return Value(max_index);
    } else if (name == TRACE_NAME) {
        return Value(std::make_shared<Symbol>(tid));
    } else if (name == TRACE_FILE) {
        return Value(std::make_shared<Symbol>(filename));
    } else if (name == SIGNALS) {
        auto list = std::make_shared<WList>();
        for (const auto& sig : signal_names) {
            list->push_back(Value(std::make_shared<Symbol>(sig)));
        }
        return Value(list);
    } else if (name == SCOPES) {
        auto list = std::make_shared<WList>();
        for (const auto& scope : scope_names) {
            list->push_back(Value(std::make_shared<Symbol>(scope)));
        }
        return Value(list);
    } else if (name == VIRTUAL_SIGNALS) {
        auto list = std::make_shared<WList>();
        for (const auto& [vsig_name, vsig_value] : virtual_signals) {
            list->push_back(Value(std::make_shared<Symbol>(vsig_name)));
        }
        return Value(list);
    }
    
    throw std::runtime_error("Unknown special signal: " + name);
}

} // namespace wal
