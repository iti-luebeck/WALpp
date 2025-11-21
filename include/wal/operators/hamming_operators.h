#pragma once

#include "wal/ast.h"
#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include "wal/trace/vcd_trace.h"
#include <functional>
#include <algorithm>
#include <bitset>
#include <string>
#include <vector>

namespace wal::operators {

/**
 * @brief Hamming distance and weight operators
 * 
 * Implements operators for analyzing signal transitions and bit patterns:
 * - hamming-weight-all-signals-all-timestamps - Calculate Hamming weight across all signals
 * - hamming-distance-all-signals-all-timestamps - Calculate Hamming distance between consecutive timestamps
 */
class HammingOperators {
public:
    /**
     * Count the number of '1' bits in a string representation of a value
     */
    static int count_ones(const std::string& bits) {
        int count = 0;
        for (char c : bits) {
            if (c == '1') {
                count++;
            }
        }
        return count;
    }

    /**
     * Convert a value to binary string representation
     */
    static std::string value_to_binary_string(const Value& value) {
        if (value.is_int()) {
            int64_t val = value.to_int();
            if (val < 0) {
                return ""; // Handle negative values gracefully
            }
            if (val == 0) {
                return "0";
            }
            std::string result;
            while (val > 0) {
                result = (val % 2 == 0 ? '0' : '1') + result;
                val /= 2;
            }
            return result;
        } else if (value.is_string()) {
            return value.to_string();
        } else {
            return "";
        }
    }

    /**
     * Pad a string to a specific length with leading zeros
     */
    static std::string pad_to_length(const std::string& str, size_t length) {
        if (str.length() >= length) {
            return str;
        }
        return std::string(length - str.length(), '0') + str;
    }

    /**
     * Calculate Hamming distance between two binary strings
     */
    static int hamming_distance(const std::string& s1, const std::string& s2) {
        size_t max_len = std::max(s1.length(), s2.length());
        std::string padded1 = pad_to_length(s1, max_len);
        std::string padded2 = pad_to_length(s2, max_len);
        
        int distance = 0;
        for (size_t i = 0; i < max_len; ++i) {
            if (padded1[i] != padded2[i]) {
                distance++;
            }
        }
        return distance;
    }

    static void register_all(wal::OperatorRegistry& registry) {
        // hamming-weight-all-signals-all-timestamps
        // Returns a list of (timestamp, sum_of_hamming_weights) for all timestamps
        registry.register_operator("hamming-weight-all-signals-all-timestamps", 
            [](SEval& eval, const std::vector<Value>& args) -> Value {
                if (!args.empty()) {
                    throw EvalError("hamming-weight-all-signals-all-timestamps expects no arguments, got " 
                        + std::to_string(args.size()));
                }

                auto trace_container = eval.get_traces();
                if (!trace_container) {
                    throw EvalError("hamming-weight-all-signals-all-timestamps: no trace loaded");
                }

                auto trace_ids = trace_container->get_trace_ids();
                if (trace_ids.empty()) {
                    throw EvalError("hamming-weight-all-signals-all-timestamps: no traces available");
                }

                // Use the first trace
                auto trace = trace_container->get_trace(trace_ids[0]);
                if (!trace) {
                    throw EvalError("hamming-weight-all-signals-all-timestamps: could not access trace");
                }

                // Store original index
                int original_index = trace->get_index();
                int max_index = trace->get_max_index();

                std::vector<Value> results;
                
                // Iterate through all timestamps
                for (int idx = 0; idx <= max_index; ++idx) {
                    trace->set_index(idx);
                    
                    int64_t total_hamming_weight = 0;
                    auto signal_names = trace->get_all_signals();
                    
                    // Calculate Hamming weight for all signals at this timestamp
                    for (const auto& signal_name : signal_names) {
                        try {
                            Value signal_val = trace->signal_value(signal_name, 0);
                            std::string binary_str = value_to_binary_string(signal_val);
                            total_hamming_weight += count_ones(binary_str);
                        } catch (const std::exception&) {
                            // Skip signals that can't be converted
                            continue;
                        }
                    }
                    
                    // Get actual VCD timestamp
                    int64_t timestamp = 0;
                    auto vcd_trace = std::dynamic_pointer_cast<VCDTrace>(trace);
                    if (vcd_trace) {
                        timestamp = vcd_trace->get_timestamp();
                    } else {
                        timestamp = static_cast<int64_t>(idx);
                    }
                    
                    // Create result tuple (timestamp, hamming_weight)
                    auto result_tuple = std::make_shared<WList>();
                    result_tuple->push_back(Value(timestamp));
                    result_tuple->push_back(Value(total_hamming_weight));
                    results.push_back(Value(result_tuple));
                }
                
                // Restore original index
                trace->set_index(original_index);
                
                return Value(std::make_shared<WList>(results));
            });

        // hamming-distance-all-signals-all-timestamps
        // Returns a list of (timestamp, hamming_distance) for all timestamps
        // Distance is calculated from the previous timestamp
        registry.register_operator("hamming-distance-all-signals-all-timestamps", 
            [](SEval& eval, const std::vector<Value>& args) -> Value {
                if (!args.empty()) {
                    throw EvalError("hamming-distance-all-signals-all-timestamps expects no arguments, got " 
                        + std::to_string(args.size()));
                }

                auto trace_container = eval.get_traces();
                if (!trace_container) {
                    throw EvalError("hamming-distance-all-signals-all-timestamps: no trace loaded");
                }

                auto trace_ids = trace_container->get_trace_ids();
                if (trace_ids.empty()) {
                    throw EvalError("hamming-distance-all-signals-all-timestamps: no traces available");
                }

                // Use the first trace
                auto trace = trace_container->get_trace(trace_ids[0]);
                if (!trace) {
                    throw EvalError("hamming-distance-all-signals-all-timestamps: could not access trace");
                }

                // Store original index
                int original_index = trace->get_index();
                int max_index = trace->get_max_index();
                auto signal_names = trace->get_all_signals();

                std::vector<Value> results;
                std::vector<std::string> prev_values;
                
                // Iterate through all timestamps
                for (int idx = 0; idx <= max_index; ++idx) {
                    trace->set_index(idx);
                    
                    std::vector<std::string> curr_values;
                    
                    // Get binary representation of all signals at current timestamp
                    for (const auto& signal_name : signal_names) {
                        try {
                            Value signal_val = trace->signal_value(signal_name, 0);
                            std::string binary_str = value_to_binary_string(signal_val);
                            curr_values.push_back(binary_str);
                        } catch (const std::exception&) {
                            curr_values.push_back("");
                        }
                    }
                    
                    // Calculate Hamming distance if we have a previous timestamp
                    if (!prev_values.empty()) {
                        int64_t total_distance = 0;
                        
                        for (size_t i = 0; i < std::min(prev_values.size(), curr_values.size()); ++i) {
                            total_distance += hamming_distance(prev_values[i], curr_values[i]);
                        }
                        
                        // Get actual VCD timestamp
                        int64_t timestamp = 0;
                        auto vcd_trace = std::dynamic_pointer_cast<VCDTrace>(trace);
                        if (vcd_trace) {
                            timestamp = vcd_trace->get_timestamp();
                        } else {
                            timestamp = static_cast<int64_t>(idx);
                        }
                        
                        // Create result tuple (timestamp, hamming_distance)
                        auto result_tuple = std::make_shared<WList>();
                        result_tuple->push_back(Value(timestamp));
                        result_tuple->push_back(Value(total_distance));
                        results.push_back(Value(result_tuple));
                    }
                    
                    prev_values = curr_values;
                }
                
                // Restore original index
                trace->set_index(original_index);
                
                return Value(std::make_shared<WList>(results));
            });
    }
};

} // namespace wal::operators
