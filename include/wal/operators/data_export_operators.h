#pragma once

#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <memory>
#include <vector>

namespace wal::operators {

/// DataExportOperators - Serialization and data format conversion
/// 
/// Operators: to-json, from-json, to-csv, from-csv, matrix-transpose, json-array
class DataExportOperators {
public:
    static void register_all(OperatorRegistry& registry) {
        // ═══════════════════════════════════════════════════════════════
        // DATA EXPORT & SERIALIZATION (6 operators)
        // ═══════════════════════════════════════════════════════════════

        // to-json: Convert WAL value to JSON string
        registry.register_operator("to-json", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("to-json expects 1 argument");
            
            auto signals = eval.eval_args(args);
            const auto& val = signals[0];
            
            std::ostringstream oss;
            
            std::function<void(const Value&)> value_to_json = [&](const Value& v) {
                if (v.is_nil()) {
                    oss << "null";
                } else if (v.is_bool()) {
                    oss << (v.to_bool() ? "true" : "false");
                } else if (v.is_int()) {
                    oss << v.to_int();
                } else if (v.is_float()) {
                    oss << std::fixed << std::setprecision(6) << v.to_float();
                } else if (v.is_string()) {
                    oss << "\"" << v.to_string() << "\"";
                } else if (v.is_symbol()) {
                    oss << "\"" << v.to_symbol()->get_name() << "\"";
                } else if (v.is_list()) {
                    auto list = v.to_list();
                    oss << "[";
                    for (size_t i = 0; i < list->size(); ++i) {
                        if (i > 0) oss << ",";
                        value_to_json((*list)[i]);
                    }
                    oss << "]";
                } else {
                    oss << "null";
                }
            };
            
            value_to_json(val);
            return Value(oss.str());
        });

        // from-json: Parse JSON string to WAL value (simplified JSON parser)
        registry.register_operator("from-json", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("from-json expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_string()) {
                throw EvalError("from-json requires a string argument");
            }
            
            const auto& json_str = signals[0].to_string();
            size_t pos = 0;
            
            std::function<Value()> parse_value = [&]() -> Value {
                while (pos < json_str.length() && std::isspace(json_str[pos])) pos++;
                
                if (pos >= json_str.length()) throw EvalError("from-json: unexpected end of input");
                
                char c = json_str[pos];
                
                if (c == 'n') {  // null
                    if (json_str.substr(pos, 4) == "null") {
                        pos += 4;
                        return Value();
                    }
                    throw EvalError("from-json: invalid null");
                } else if (c == 't' || c == 'f') {  // boolean
                    if (json_str.substr(pos, 4) == "true") {
                        pos += 4;
                        return Value(true);
                    } else if (json_str.substr(pos, 5) == "false") {
                        pos += 5;
                        return Value(false);
                    }
                    throw EvalError("from-json: invalid boolean");
                } else if (c == '"') {  // string
                    pos++;
                    size_t start = pos;
                    while (pos < json_str.length() && json_str[pos] != '"') pos++;
                    if (pos >= json_str.length()) throw EvalError("from-json: unterminated string");
                    Value result(json_str.substr(start, pos - start));
                    pos++;
                    return result;
                } else if (c == '[') {  // array
                    pos++;
                    auto list = std::make_shared<WList>();
                    while (pos < json_str.length() && std::isspace(json_str[pos])) pos++;
                    
                    if (pos < json_str.length() && json_str[pos] == ']') {
                        pos++;
                        return Value(list);
                    }
                    
                    while (true) {
                        list->push_back(parse_value());
                        while (pos < json_str.length() && std::isspace(json_str[pos])) pos++;
                        
                        if (pos >= json_str.length()) throw EvalError("from-json: unterminated array");
                        if (json_str[pos] == ']') {
                            pos++;
                            break;
                        }
                        if (json_str[pos] != ',') throw EvalError("from-json: expected , or ]");
                        pos++;
                    }
                    return Value(list);
                } else if (c == '-' || std::isdigit(c)) {  // number
                    size_t start = pos;
                    if (c == '-') pos++;
                    while (pos < json_str.length() && std::isdigit(json_str[pos])) pos++;
                    if (pos < json_str.length() && json_str[pos] == '.') {
                        pos++;
                        while (pos < json_str.length() && std::isdigit(json_str[pos])) pos++;
                        return Value(std::stod(json_str.substr(start, pos - start)));
                    } else {
                        return Value(static_cast<int64_t>(std::stoll(json_str.substr(start, pos - start))));
                    }
                } else {
                    throw EvalError("from-json: unexpected character");
                }
            };
            
            return parse_value();
        });

        // to-csv: Convert list of lists to CSV string
        registry.register_operator("to-csv", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() < 1 || args.size() > 2) throw EvalError("to-csv expects 1 or 2 arguments");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("to-csv requires a list argument");
            }
            
            auto list = signals[0].to_list();
            std::string delimiter = ",";
            
            if (args.size() == 2) {
                if (!signals[1].is_string()) {
                    throw EvalError("to-csv delimiter must be a string");
                }
                delimiter = signals[1].to_string();
            }
            
            std::ostringstream oss;
            for (const auto& row : *list) {
                if (row.is_list()) {
                    auto row_list = row.to_list();
                    for (size_t i = 0; i < row_list->size(); ++i) {
                        if (i > 0) oss << delimiter;
                        const auto& cell = (*row_list)[i];
                        if (cell.is_string()) {
                            oss << "\"" << cell.to_string() << "\"";
                        } else if (cell.is_int()) {
                            oss << cell.to_int();
                        } else if (cell.is_float()) {
                            oss << std::fixed << std::setprecision(6) << cell.to_float();
                        } else if (cell.is_symbol()) {
                            oss << cell.to_symbol()->get_name();
                        } else {
                            oss << cell.to_string_repr();
                        }
                    }
                } else {
                    if (row.is_string()) {
                        oss << "\"" << row.to_string() << "\"";
                    } else if (row.is_int()) {
                        oss << row.to_int();
                    } else if (row.is_float()) {
                        oss << std::fixed << std::setprecision(6) << row.to_float();
                    } else {
                        oss << row.to_string_repr();
                    }
                }
                oss << "\n";
            }
            
            return Value(oss.str());
        });

        // from-csv: Parse CSV string to list of lists
        registry.register_operator("from-csv", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() < 1 || args.size() > 2) throw EvalError("from-csv expects 1 or 2 arguments");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_string()) {
                throw EvalError("from-csv requires a string argument");
            }
            
            const auto& csv_str = signals[0].to_string();
            std::string delimiter = ",";
            
            if (args.size() == 2) {
                if (!signals[1].is_string()) {
                    throw EvalError("from-csv delimiter must be a string");
                }
                delimiter = signals[1].to_string();
            }
            
            auto result = std::make_shared<WList>();
            std::istringstream iss(csv_str);
            std::string line;
            
            while (std::getline(iss, line)) {
                if (line.empty()) continue;
                
                auto row = std::make_shared<WList>();
                size_t pos = 0;
                
                while (pos < line.length()) {
                    size_t next_delim = line.find(delimiter, pos);
                    if (next_delim == std::string::npos) next_delim = line.length();
                    
                    std::string cell = line.substr(pos, next_delim - pos);
                    
                    // Trim whitespace
                    cell.erase(0, cell.find_first_not_of(" \t"));
                    cell.erase(cell.find_last_not_of(" \t") + 1);
                    
                    // Remove quotes
                    if (cell.length() >= 2 && cell[0] == '"' && cell[cell.length() - 1] == '"') {
                        cell = cell.substr(1, cell.length() - 2);
                        row->push_back(Value(cell));
                    } else if (cell == "true" || cell == "false") {
                        row->push_back(Value(cell == "true"));
                    } else {
                        try {
                            if (cell.find('.') != std::string::npos) {
                                row->push_back(Value(std::stod(cell)));
                            } else {
                                row->push_back(Value(static_cast<int64_t>(std::stoll(cell))));
                            }
                        } catch (...) {
                            row->push_back(Value(cell));
                        }
                    }
                    
                    pos = next_delim + delimiter.length();
                }
                
                result->push_back(Value(row));
            }
            
            return Value(result);
        });

        // matrix-transpose: Transpose a 2D matrix (list of lists)
        registry.register_operator("matrix-transpose", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("matrix-transpose expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("matrix-transpose requires a list of lists");
            }
            
            auto matrix = signals[0].to_list();
            if (matrix->empty()) throw EvalError("matrix-transpose: empty matrix");
            
            size_t rows = matrix->size();
            size_t cols = 0;
            
            for (const auto& row : *matrix) {
                if (row.is_list()) {
                    cols = std::max(cols, row.to_list()->size());
                } else {
                    throw EvalError("matrix-transpose: all rows must be lists");
                }
            }
            
            auto result = std::make_shared<WList>();
            
            for (size_t j = 0; j < cols; ++j) {
                auto new_row = std::make_shared<WList>();
                for (size_t i = 0; i < rows; ++i) {
                    auto row_list = (*matrix)[i].to_list();
                    if (j < row_list->size()) {
                        new_row->push_back((*row_list)[j]);
                    } else {
                        new_row->push_back(Value());
                    }
                }
                result->push_back(Value(new_row));
            }
            
            return Value(result);
        });

        // json-array: Create JSON array representation
        registry.register_operator("json-array", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) throw EvalError("json-array expects 1 argument");
            
            auto signals = eval.eval_args(args);
            if (!signals[0].is_list()) {
                throw EvalError("json-array requires a list argument");
            }
            
            auto list = signals[0].to_list();
            std::ostringstream oss;
            oss << "[";
            
            for (size_t i = 0; i < list->size(); ++i) {
                if (i > 0) oss << ",";
                const auto& v = (*list)[i];
                
                if (v.is_nil()) {
                    oss << "null";
                } else if (v.is_bool()) {
                    oss << (v.to_bool() ? "true" : "false");
                } else if (v.is_int()) {
                    oss << v.to_int();
                } else if (v.is_float()) {
                    oss << std::fixed << std::setprecision(6) << v.to_float();
                } else if (v.is_string()) {
                    oss << "\"" << v.to_string() << "\"";
                } else {
                    oss << "null";
                }
            }
            
            oss << "]";
            return Value(oss.str());
        });
    }
};

}  // namespace wal::operators
