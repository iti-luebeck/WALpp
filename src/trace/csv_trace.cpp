#include "wal/trace/csv_trace.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace wal {

std::vector<std::string> CSVTrace::read_file_lines(const std::string& filename) {
    std::vector<std::string> lines;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Trim trailing whitespace (but not leading)
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
            line.pop_back();
        }
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    
    file.close();
    return lines;
}

std::vector<std::string> CSVTrace::split_csv_line(const std::string& line, char delim) {
    std::vector<std::string> result;
    std::string field;
    bool in_quotes = false;
    
    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        
        if (c == '"') {
            in_quotes = !in_quotes;
            // Skip the quote character itself
            continue;
        }
        
        if (c == delim && !in_quotes) {
            result.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    
    // Add final field
    result.push_back(field);
    
    return result;
}

Value CSVTrace::parse_value(const std::string& value) {
    // Trim whitespace
    std::string trimmed = value;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    
    if (trimmed.empty()) {
        return Value(0);
    }
    
    // Check if it's a float
    if (trimmed.find('.') != std::string::npos) {
        try {
            return Value(std::stod(trimmed));
        } catch (...) {
            return Value(0);
        }
    }
    
    // Try to parse as integer (possibly hex/binary)
    try {
        // Binary format: 0b1010
        if (trimmed.substr(0, 2) == "0b" || trimmed.substr(0, 2) == "0B") {
            int64_t value = 0;
            for (size_t i = 2; i < trimmed.length(); ++i) {
                char c = trimmed[i];
                if (c == '0' || c == '1') {
                    value = (value << 1) | (c == '1' ? 1 : 0);
                }
            }
            return Value(value);
        }
        
        // Hexadecimal format: 0x1A or 1A or 0xFFFFFFFF
        if (trimmed.substr(0, 2) == "0x" || trimmed.substr(0, 2) == "0X") {
            return Value(static_cast<int64_t>(std::stoll(trimmed, nullptr, 16)));
        }
        if (trimmed.find_first_not_of("0123456789ABCDEFabcdef") == std::string::npos && 
            trimmed.length() > 1) {
            // Could be hex without 0x prefix
            return Value(static_cast<int64_t>(std::stoll(trimmed, nullptr, 16)));
        }
        
        // Decimal
        return Value(static_cast<int64_t>(std::stoll(trimmed)));
    } catch (...) {
        return Value(0);
    }
}

int CSVTrace::infer_width(int64_t max_value) {
    if (max_value == 0) return 1;
    if (max_value < 0) return 64;  // Signed
    
    int width = 0;
    int64_t val = max_value;
    while (val > 0) {
        width++;
        val >>= 1;
    }
    return width + 1;  // +1 for safety
}

void CSVTrace::parse_header(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        throw std::runtime_error("CSV file is empty");
    }
    
    if (!has_header) {
        // Generate column names: col0, col1, col2, ...
        auto fields = split_csv_line(lines[0], delimiter);
        for (size_t i = 0; i < fields.size(); ++i) {
            std::string name = "col" + std::to_string(i);
            column_to_signal[i] = name;
            signal_info[name] = {name, 1, i};
            signal_names.push_back(name);
        }
        return;
    }
    
    // Parse header row
    auto fields = split_csv_line(lines[0], delimiter);
    
    for (size_t i = 0; i < fields.size(); ++i) {
        std::string col_name = fields[i];
        // Trim whitespace
        size_t start = col_name.find_first_not_of(" \t");
        size_t end = col_name.find_last_not_of(" \t");
        if (start == std::string::npos) {
            // All whitespace - skip this column
            col_name = "";
        } else {
            col_name = col_name.substr(start, end - start + 1);
        }
        
        if (col_name.empty()) {
            // Skip empty column names (might be result of whitespace)
            column_to_signal[i] = "";
        } else if (i == 0 || col_name == time_column) {
            // First column or explicitly named time column - this is the timestamp column
            column_to_signal[i] = ""; // Empty name = timestamp
        } else {
            column_to_signal[i] = col_name;
            signal_info[col_name] = {col_name, 1, i};  // Width will be updated
            signal_names.push_back(col_name);
            current_values[col_name] = Value(0);
        }
    }
}

void CSVTrace::parse_data(const std::vector<std::string>& lines) {
    int start_row = has_header ? 1 : 0;
    int timestamp_count = 0;
    int64_t current_timestamp = 0;
    
    // Track max values to infer widths
    std::unordered_map<std::string, int64_t> max_values;
    for (const auto& sig_name : signal_names) {
        max_values[sig_name] = 0;
    }
    
    for (int row = start_row; row < static_cast<int>(lines.size()); ++row) {
        auto fields = split_csv_line(lines[row], delimiter);
        
        if (fields.empty()) continue;
        
        // First column is typically timestamp
        Value ts_val = parse_value(fields[0]);
        if (ts_val.is_int()) {
            current_timestamp = ts_val.to_int();
        } else if (ts_val.is_float()) {
            current_timestamp = static_cast<int64_t>(ts_val.to_float());
        }
        
        // Parse signal values
        TimeStep ts;
        ts.timestamp = current_timestamp;
        
        for (size_t col = 1; col < fields.size(); ++col) {
            auto it = column_to_signal.find(col);
            if (it == column_to_signal.end() || it->second.empty()) {
                continue;  // Skip unknown or timestamp columns
            }
            
            std::string sig_name = it->second;
            Value val = parse_value(fields[col]);
            
            ts.signal_values[sig_name] = val;
            current_values[sig_name] = val;
            
            // Track max value for width inference
            if (val.is_int()) {
                int64_t iv = val.to_int();
                if (iv < 0) {
                    max_values[sig_name] = 63;  // Signed
                } else if (iv > max_values[sig_name]) {
                    max_values[sig_name] = iv;
                }
            }
        }
        
        time_steps.push_back(ts);
        timestamp_index_map[current_timestamp] = timestamp_count;
        timestamp_count++;
    }
    
    // Update signal widths based on observed values
    for (const auto& [sig_name, max_val] : max_values) {
        if (signal_info.find(sig_name) != signal_info.end()) {
            signal_info[sig_name].width = infer_width(max_val);
        }
    }
    
    max_index = static_cast<int>(time_steps.size()) - 1;
}

void CSVTrace::load() {
    try {
        auto lines = read_file_lines(filename);
        
        if (lines.empty()) {
            throw std::runtime_error("CSV file is empty");
        }
        
        // Parse header
        parse_header(lines);
        
        // Parse data
        parse_data(lines);
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load CSV file: " + std::string(e.what()));
    }
}

Value CSVTrace::signal_value(const std::string& name, int offset,
                            const std::string& scope) {
    std::string full_name = name;
    if (!scope.empty()) {
        full_name = scope + Trace::SCOPE_SEPARATOR + name;
    }
    
    // Check if signal exists
    if (signal_info.find(full_name) == signal_info.end() &&
        signal_info.find(name) == signal_info.end()) {
        throw std::runtime_error("Signal not found: " + full_name);
    }
    
    std::string lookup_name = (signal_info.find(full_name) != signal_info.end()) ? full_name : name;
    
    int target_index = current_index + offset;
    if (target_index < 0 || target_index > max_index) {
        throw std::runtime_error("Index out of range");
    }
    
    if (target_index < static_cast<int>(time_steps.size())) {
        auto& ts = time_steps[target_index];
        auto it = ts.signal_values.find(lookup_name);
        if (it != ts.signal_values.end()) {
            return it->second;
        }
    }
    
    // Return last known value if not found at this timestep
    auto it = current_values.find(lookup_name);
    if (it != current_values.end()) {
        return it->second;
    }
    
    return Value(0);
}

int CSVTrace::signal_width(const std::string& name) {
    auto it = signal_info.find(name);
    if (it != signal_info.end()) {
        return it->second.width;
    }
    throw std::runtime_error("Signal not found: " + name);
}

bool CSVTrace::contains_signal(const std::string& name) const {
    auto it = signal_info.find(name);
    if (it != signal_info.end()) {
        return true;
    }
    return false;
}

std::vector<std::string> CSVTrace::get_all_signals() const {
    return signal_names;
}

} // namespace wal
