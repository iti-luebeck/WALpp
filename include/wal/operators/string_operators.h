#pragma once

#include "wal/ast.h"
#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include <functional>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace wal::operators {

/**
 * @brief String manipulation operators
 * 
 * Implements string operations:
 * - string-length, substring, concat, split, join
 * - uppercase, lowercase, trim
 * - startswith, endswith, replace
 * - string-index, contains
 */
class StringOperators {
public:
    static void register_all(wal::OperatorRegistry& registry) {
        registry.register_operator("string-length", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("string-length requires exactly 1 argument");
            }
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_string()) {
                throw EvalError("string-length: argument must be a string");
            }
            
            return Value(static_cast<int64_t>(evaluated[0].to_string().length()));
        });
        
        registry.register_operator("substring", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() < 2 || args.size() > 3) {
                throw EvalError("substring requires 2 or 3 arguments");
            }
            
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_string()) {
                throw EvalError("substring: first argument must be a string");
            }
            if (!evaluated[1].is_int()) {
                throw EvalError("substring: second argument must be an integer");
            }
            if (args.size() == 3 && !evaluated[2].is_int()) {
                throw EvalError("substring: third argument must be an integer");
            }
            
            const std::string& str = evaluated[0].to_string();
            int64_t start = evaluated[1].to_int();
            int64_t length = (args.size() == 3) ? evaluated[2].to_int() : str.length() - start;
            
            if (start < 0 || start > static_cast<int64_t>(str.length())) {
                throw EvalError("substring: start index out of bounds");
            }
            if (length < 0 || start + length > static_cast<int64_t>(str.length())) {
                throw EvalError("substring: length out of bounds");
            }
            
            return Value(str.substr(start, length));
        });
        
        registry.register_operator("concat", [](SEval& eval, const std::vector<Value>& args) -> Value {
            auto evaluated = eval.eval_args(args);
            
            std::string result;
            for (const auto& val : evaluated) {
                if (!val.is_string()) {
                    throw EvalError("concat: all arguments must be strings");
                }
                result += val.to_string();
            }
            
            return Value(result);
        });
        
        registry.register_operator("split", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("split requires exactly 2 arguments");
            }
            
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_string()) {
                throw EvalError("split: first argument must be a string");
            }
            if (!evaluated[1].is_string()) {
                throw EvalError("split: second argument must be a delimiter string");
            }
            
            const std::string& str = evaluated[0].to_string();
            const std::string& delimiter = evaluated[1].to_string();
            
            if (delimiter.empty()) {
                throw EvalError("split: delimiter cannot be empty");
            }
            
            std::vector<Value> result;
            size_t start = 0;
            size_t end = str.find(delimiter);
            
            while (end != std::string::npos) {
                result.push_back(Value(str.substr(start, end - start)));
                start = end + delimiter.length();
                end = str.find(delimiter, start);
            }
            result.push_back(Value(str.substr(start)));
            
            return Value(std::make_shared<WList>(result));
        });
        
        registry.register_operator("join", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("join requires exactly 2 arguments");
            }
            
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_list()) {
                throw EvalError("join: first argument must be a list");
            }
            if (!evaluated[1].is_string()) {
                throw EvalError("join: second argument must be a separator string");
            }
            
            const auto& list = evaluated[0].to_list();
            const std::string& separator = evaluated[1].to_string();
            
            std::string result;
            for (size_t i = 0; i < list->size(); ++i) {
                if (!(*list)[i].is_string()) {
                    throw EvalError("join: all list elements must be strings");
                }
                if (i > 0) result += separator;
                result += (*list)[i].to_string();
            }
            
            return Value(result);
        });
        
        registry.register_operator("uppercase", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("uppercase requires exactly 1 argument");
            }
            
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_string()) {
                throw EvalError("uppercase: argument must be a string");
            }
            
            std::string str = evaluated[0].to_string();
            std::transform(str.begin(), str.end(), str.begin(),
                          [](unsigned char c) { return std::toupper(c); });
            
            return Value(str);
        });
        
        registry.register_operator("lowercase", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("lowercase requires exactly 1 argument");
            }
            
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_string()) {
                throw EvalError("lowercase: argument must be a string");
            }
            
            std::string str = evaluated[0].to_string();
            std::transform(str.begin(), str.end(), str.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            
            return Value(str);
        });
        
        registry.register_operator("trim", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("trim requires exactly 1 argument");
            }
            
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_string()) {
                throw EvalError("trim: argument must be a string");
            }
            
            std::string str = evaluated[0].to_string();
            
            // Trim left
            size_t start = 0;
            while (start < str.length() && std::isspace(str[start])) {
                start++;
            }
            
            // Trim right
            size_t end = str.length();
            while (end > start && std::isspace(str[end - 1])) {
                end--;
            }
            
            return Value(str.substr(start, end - start));
        });
        
        registry.register_operator("startswith", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("startswith requires exactly 2 arguments");
            }
            
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_string()) {
                throw EvalError("startswith: first argument must be a string");
            }
            if (!evaluated[1].is_string()) {
                throw EvalError("startswith: second argument must be a prefix string");
            }
            
            const std::string& str = evaluated[0].to_string();
            const std::string& prefix = evaluated[1].to_string();
            
            if (prefix.length() > str.length()) {
                return Value(false);
            }
            
            return Value(str.compare(0, prefix.length(), prefix) == 0);
        });
        
        registry.register_operator("endswith", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("endswith requires exactly 2 arguments");
            }
            
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_string()) {
                throw EvalError("endswith: first argument must be a string");
            }
            if (!evaluated[1].is_string()) {
                throw EvalError("endswith: second argument must be a suffix string");
            }
            
            const std::string& str = evaluated[0].to_string();
            const std::string& suffix = evaluated[1].to_string();
            
            if (suffix.length() > str.length()) {
                return Value(false);
            }
            
            return Value(str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0);
        });
        
        registry.register_operator("replace", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 3) {
                throw EvalError("replace requires exactly 3 arguments");
            }
            
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_string()) {
                throw EvalError("replace: first argument must be a string");
            }
            if (!evaluated[1].is_string()) {
                throw EvalError("replace: second argument must be a search string");
            }
            if (!evaluated[2].is_string()) {
                throw EvalError("replace: third argument must be a replacement string");
            }
            
            std::string str = evaluated[0].to_string();
            const std::string& search = evaluated[1].to_string();
            const std::string& replacement = evaluated[2].to_string();
            
            if (search.empty()) {
                throw EvalError("replace: search string cannot be empty");
            }
            
            size_t pos = 0;
            while ((pos = str.find(search, pos)) != std::string::npos) {
                str.replace(pos, search.length(), replacement);
                pos += replacement.length();
            }
            
            return Value(str);
        });
        
        registry.register_operator("string-index", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("string-index requires exactly 2 arguments");
            }
            
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_string()) {
                throw EvalError("string-index: first argument must be a string");
            }
            if (!evaluated[1].is_string()) {
                throw EvalError("string-index: second argument must be a substring");
            }
            
            const std::string& str = evaluated[0].to_string();
            const std::string& substr = evaluated[1].to_string();
            
            size_t pos = str.find(substr);
            if (pos == std::string::npos) {
                return Value(static_cast<int64_t>(-1));
            }
            
            return Value(static_cast<int64_t>(pos));
        });
        
        registry.register_operator("contains", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 2) {
                throw EvalError("contains requires exactly 2 arguments");
            }
            
            auto evaluated = eval.eval_args(args);
            
            if (!evaluated[0].is_string()) {
                throw EvalError("contains: first argument must be a string");
            }
            if (!evaluated[1].is_string()) {
                throw EvalError("contains: second argument must be a substring");
            }
            
            const std::string& str = evaluated[0].to_string();
            const std::string& substr = evaluated[1].to_string();
            
            return Value(str.find(substr) != std::string::npos);
        });
    }
};

} // namespace wal::operators
