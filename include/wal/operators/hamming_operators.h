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
    static inline int count_ones(const std::string& bits) {
        int count = 0;
        for (char c : bits) {
            count += (c == '1');
        }
        return count;
    }

    /**
     * Convert a value to binary string representation
     */
    static std::string value_to_binary_string(const Value& value) {
        if (value.is_int()) {
            int64_t val = value.to_int();
            if (val <= 0) {
                return val == 0 ? "0" : "";
            }
            // Pre-calculate size to avoid reallocations
            int bits = 64 - __builtin_clzll(static_cast<uint64_t>(val));
            std::string result;
            result.reserve(bits);
            uint64_t uval = static_cast<uint64_t>(val);
            for (int i = bits - 1; i >= 0; --i) {
                result += ((uval >> i) & 1) ? '1' : '0';
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
                results.reserve(max_index + 1);
                
                // Fast path for VCDTrace: direct access to time_steps
                auto vcd_trace = std::dynamic_pointer_cast<VCDTrace>(trace);
                if (vcd_trace) {
                    const auto& time_steps = vcd_trace->get_time_steps();
                    const size_t num_steps = std::min(static_cast<size_t>(max_index + 1), time_steps.size());
                    
                    for (size_t idx = 0; idx < num_steps; ++idx) {
                        const auto& ts = time_steps[idx];
                        int64_t total_hamming_weight = 0;
                        
                        // Iterate through all signal values at this timestamp
                        for (const auto& [identifier, value] : ts.signal_values) {
                            if (value.is_int()) {
                                int64_t val = value.to_int();
                                if (val > 0) {
                                    total_hamming_weight += __builtin_popcountll(static_cast<uint64_t>(val));
                                } else if (val < 0) {
                                    // For negative values, count bits in two's complement
                                    total_hamming_weight += __builtin_popcountll(static_cast<uint64_t>(val));
                                }
                            } else if (value.is_string()) {
                                const std::string& str = value.to_string();
                                total_hamming_weight += count_ones(str);
                            }
                        }
                        
                        auto result_tuple = std::make_shared<WList>();
                        result_tuple->push_back(Value(ts.timestamp));
                        result_tuple->push_back(Value(total_hamming_weight));
                        results.push_back(Value(result_tuple));
                    }
                } else {
                    // Fallback path for other trace types
                    auto signal_names = trace->get_all_signals();
                    
                    for (int idx = 0; idx <= max_index; ++idx) {
                        trace->set_index(idx);
                        
                        int64_t total_hamming_weight = 0;
                        
                        for (const auto& signal_name : signal_names) {
                            try {
                                Value signal_val = trace->signal_value(signal_name, 0);
                                if (signal_val.is_int()) {
                                    int64_t val = signal_val.to_int();
                                    if (val > 0) {
                                        total_hamming_weight += __builtin_popcountll(static_cast<uint64_t>(val));
                                    }
                                } else {
                                    std::string binary_str = value_to_binary_string(signal_val);
                                    total_hamming_weight += count_ones(binary_str);
                                }
                            } catch (const std::exception&) {
                                continue;
                            }
                        }
                        
                        int64_t timestamp = static_cast<int64_t>(idx);
                        auto result_tuple = std::make_shared<WList>();
                        result_tuple->push_back(Value(timestamp));
                        result_tuple->push_back(Value(total_hamming_weight));
                        results.push_back(Value(result_tuple));
                    }
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

                std::vector<Value> results;
                results.reserve(max_index);  // Pre-allocate (max_index timestamps, first skipped)
                
                // Fast path for VCDTrace: direct access to time_steps
                auto vcd_trace = std::dynamic_pointer_cast<VCDTrace>(trace);
                if (vcd_trace) {
                    const auto& time_steps = vcd_trace->get_time_steps();
                    const size_t num_steps = std::min(static_cast<size_t>(max_index + 1), time_steps.size());
                    
                    if (num_steps > 0) {
                        // Store previous timestamp signal values for comparison
                        std::unordered_map<std::string, int64_t> prev_int_values;
                        std::unordered_map<std::string, std::string> prev_str_values;
                        
                        for (size_t idx = 0; idx < num_steps; ++idx) {
                            const auto& ts = time_steps[idx];
                            int64_t total_distance = 0;
                            
                            // Current timestamp values
                            std::unordered_map<std::string, int64_t> curr_int_values;
                            std::unordered_map<std::string, std::string> curr_str_values;
                            
                            for (const auto& [identifier, value] : ts.signal_values) {
                                if (value.is_int()) {
                                    int64_t curr_val = value.to_int();
                                    curr_int_values[identifier] = curr_val;
                                    
                                    // Calculate distance from previous value if exists
                                    if (idx > 0) {
                                        auto prev_it = prev_int_values.find(identifier);
                                        if (prev_it != prev_int_values.end()) {
                                            // XOR to find differing bits
                                            uint64_t diff = static_cast<uint64_t>(curr_val) ^ static_cast<uint64_t>(prev_it->second);
                                            total_distance += __builtin_popcountll(diff);
                                        } else {
                                            // New signal, count all bits as changed
                                            if (curr_val > 0) {
                                                total_distance += __builtin_popcountll(static_cast<uint64_t>(curr_val));
                                            } else if (curr_val < 0) {
                                                total_distance += __builtin_popcountll(static_cast<uint64_t>(curr_val));
                                            }
                                        }
                                    }
                                } else if (value.is_string()) {
                                    const std::string& curr_str = value.to_string();
                                    curr_str_values[identifier] = curr_str;
                                    
                                    // Calculate distance from previous value if exists
                                    if (idx > 0) {
                                        auto prev_it = prev_str_values.find(identifier);
                                        if (prev_it != prev_str_values.end()) {
                                            total_distance += hamming_distance(prev_it->second, curr_str);
                                        } else {
                                            // New signal, count all '1' bits
                                            total_distance += count_ones(curr_str);
                                        }
                                    }
                                }
                            }
                            
                            // Store results starting from second timestamp
                            if (idx > 0) {
                                auto result_tuple = std::make_shared<WList>();
                                result_tuple->push_back(Value(ts.timestamp));
                                result_tuple->push_back(Value(total_distance));
                                results.push_back(Value(result_tuple));
                            }
                            
                            // Move current to previous (using move to avoid copies)
                            prev_int_values = std::move(curr_int_values);
                            prev_str_values = std::move(curr_str_values);
                        }
                    }
                } else {
                    // Fallback path for other trace types
                    auto signal_names = trace->get_all_signals();
                    std::vector<std::string> prev_values;
                    prev_values.reserve(signal_names.size());
                    
                    // Iterate through all timestamps
                    for (int idx = 0; idx <= max_index; ++idx) {
                        trace->set_index(idx);
                        
                        std::vector<std::string> curr_values;
                        curr_values.reserve(signal_names.size());
                        
                        // Get binary representation of all signals at current timestamp
                        for (const auto& signal_name : signal_names) {
                            try {
                                Value signal_val = trace->signal_value(signal_name, 0);
                                if (signal_val.is_int()) {
                                    // For integers, convert to binary string
                                    curr_values.push_back(value_to_binary_string(signal_val));
                                } else if (signal_val.is_string()) {
                                    curr_values.push_back(signal_val.to_string());
                                } else {
                                    curr_values.push_back("");
                                }
                            } catch (const std::exception&) {
                                curr_values.push_back("");
                            }
                        }
                        
                        // Calculate Hamming distance if we have a previous timestamp
                        if (!prev_values.empty()) {
                            int64_t total_distance = 0;
                            size_t min_size = std::min(prev_values.size(), curr_values.size());
                            
                            for (size_t i = 0; i < min_size; ++i) {
                                total_distance += hamming_distance(prev_values[i], curr_values[i]);
                            }
                            
                            // Get actual VCD timestamp
                            int64_t timestamp = static_cast<int64_t>(idx);
                            
                            // Create result tuple (timestamp, hamming_distance)
                            auto result_tuple = std::make_shared<WList>();
                            result_tuple->push_back(Value(timestamp));
                            result_tuple->push_back(Value(total_distance));
                            results.push_back(Value(result_tuple));
                        }
                        
                        prev_values = std::move(curr_values);
                    }
                }
                
                // Restore original index
                trace->set_index(original_index);
                
                return Value(std::make_shared<WList>(results));
            });
    }
};

} // namespace wal::operators
