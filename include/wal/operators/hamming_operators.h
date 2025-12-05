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
#include <unordered_set>
#include <thread>
#include <mutex>

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
                
                // Check if trace has any data
                if (max_index < 0) {
                    throw EvalError("hamming-weight-all-signals-all-timestamps: trace has no timestamps");
                }

                std::vector<Value> results;
                results.reserve(max_index + 1);
                
                // Fast path for VCDTrace: direct access to time_steps
                auto vcd_trace = std::dynamic_pointer_cast<VCDTrace>(trace);
                if (vcd_trace) {
                    const auto& time_steps = vcd_trace->get_time_steps();
                    
                    // Verify we have data
                    if (time_steps.empty()) {
                        throw EvalError("hamming-weight-all-signals-all-timestamps: VCD trace has no time steps");
                    }
                    
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
                
                // Check if trace has any data
                if (max_index < 0) {
                    throw EvalError("hamming-distance-all-signals-all-timestamps: trace has no timestamps");
                }

                std::vector<Value> results;
                results.reserve(max_index);  // Pre-allocate (max_index timestamps, first skipped)
                
                // Fast path for VCDTrace: direct access to time_steps
                auto vcd_trace = std::dynamic_pointer_cast<VCDTrace>(trace);
                if (vcd_trace) {
                    const auto& time_steps = vcd_trace->get_time_steps();
                    
                    // Verify we have data
                    if (time_steps.empty()) {
                        throw EvalError("hamming-distance-all-signals-all-timestamps: VCD trace has no time steps");
                    }
                    
                    const size_t num_steps = std::min(static_cast<size_t>(max_index + 1), time_steps.size());
                    
                    if (num_steps > 0) {
                        // Build identifier index for consistent ordering
                        std::vector<std::string> all_identifiers;
                        std::unordered_map<std::string, size_t> identifier_to_idx;
                        
                        // Collect all unique identifiers across all timesteps
                        for (const auto& ts : time_steps) {
                            for (const auto& [identifier, value] : ts.signal_values) {
                                if (identifier_to_idx.find(identifier) == identifier_to_idx.end()) {
                                    identifier_to_idx[identifier] = all_identifiers.size();
                                    all_identifiers.push_back(identifier);
                                }
                            }
                        }
                        
                        const size_t num_signals = all_identifiers.size();
                        
                        // Pre-allocate value arrays (reused across timestamps)
                        std::vector<int64_t> prev_values(num_signals, 0);
                        std::vector<int64_t> curr_values(num_signals, 0);
                        std::vector<bool> prev_valid(num_signals, false);
                        std::vector<bool> curr_valid(num_signals, false);
                        
                        for (size_t idx = 0; idx < num_steps; ++idx) {
                            const auto& ts = time_steps[idx];
                            
                            // Reset current validity flags
                            std::fill(curr_valid.begin(), curr_valid.end(), false);
                            
                            // Populate current values by identifier index (no hash lookups!)
                            for (const auto& [identifier, value] : ts.signal_values) {
                                if (value.is_int()) {
                                    size_t sig_idx = identifier_to_idx[identifier];
                                    curr_values[sig_idx] = value.to_int();
                                    curr_valid[sig_idx] = true;
                                }
                            }
                            
                            // Calculate HD
                            if (idx > 0) {
                                int64_t total_distance = 0;
                                
                                for (size_t s = 0; s < num_signals; ++s) {
                                    if (curr_valid[s] && prev_valid[s]) {
                                        // Both timestamps have this signal
                                        uint64_t diff = static_cast<uint64_t>(curr_values[s]) ^ 
                                                       static_cast<uint64_t>(prev_values[s]);
                                        total_distance += __builtin_popcountll(diff);
                                    } else if (curr_valid[s] && !prev_valid[s]) {
                                        // New signal appeared
                                        int64_t val = curr_values[s];
                                        if (val > 0) {
                                            total_distance += __builtin_popcountll(static_cast<uint64_t>(val));
                                        } else if (val < 0) {
                                            total_distance += __builtin_popcountll(static_cast<uint64_t>(val));
                                        }
                                    }
                                }
                                
                                // Store result
                                auto result_tuple = std::make_shared<WList>();
                                result_tuple->push_back(Value(ts.timestamp));
                                result_tuple->push_back(Value(total_distance));
                                results.push_back(Value(result_tuple));
                            }
                            
                            // Swap current -> previous (no allocations!)
                            prev_values.swap(curr_values);
                            prev_valid.swap(curr_valid);
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

        // hamming-weight-scope: Calculate hamming weight for signals in a specific scope
        // Usage: (hamming-weight-scope "top.cpu")
        // Returns: ((timestamp1 weight1) (timestamp2 weight2) ...)
        registry.register_operator("hamming-weight-scope",
            [](SEval& eval, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw EvalError("hamming-weight-scope expects 1 argument (scope-prefix), got " 
                        + std::to_string(args.size()));
                }

                auto evaluated = eval.eval_args(args);
                if (!evaluated[0].is_string()) {
                    throw EvalError("hamming-weight-scope: scope prefix must be a string");
                }
                
                std::string scope_prefix = evaluated[0].to_string();

                auto trace_container = eval.get_traces();
                if (!trace_container) {
                    throw EvalError("hamming-weight-scope: no trace loaded");
                }

                auto trace_ids = trace_container->get_trace_ids();
                if (trace_ids.empty()) {
                    throw EvalError("hamming-weight-scope: no traces available");
                }

                auto trace = trace_container->get_trace(trace_ids[0]);
                if (!trace) {
                    throw EvalError("hamming-weight-scope: could not access trace");
                }

                // Get all signals and filter by scope
                auto all_signals = trace->get_all_signals();
                std::vector<std::string> scoped_signals;
                
                for (const auto& signal : all_signals) {
                    // Check if signal name starts with scope prefix
                    if (signal.length() >= scope_prefix.length() &&
                        signal.substr(0, scope_prefix.length()) == scope_prefix) {
                        scoped_signals.push_back(signal);
                    }
                }

                if (scoped_signals.empty()) {
                    throw EvalError("hamming-weight-scope: no signals found with scope prefix '" 
                        + scope_prefix + "'");
                }

                int original_index = trace->get_index();
                int max_index = trace->get_max_index();

                std::vector<Value> results;
                results.reserve(max_index + 1);

                // Calculate hamming weight for filtered signals
                auto vcd_trace = std::dynamic_pointer_cast<VCDTrace>(trace);
                if (vcd_trace) {
                    const auto& time_steps = vcd_trace->get_time_steps();
                    const auto& signal_info = vcd_trace->get_signal_info();
                    const size_t num_steps = std::min(static_cast<size_t>(max_index + 1), time_steps.size());

                    // Build set of identifiers for scoped signals
                    std::unordered_set<std::string> scoped_identifiers;
                    for (const auto& sig_name : scoped_signals) {
                        auto it = signal_info.find(sig_name);
                        if (it != signal_info.end()) {
                            scoped_identifiers.insert(it->second.identifier);
                        }
                    }

                    for (size_t idx = 0; idx < num_steps; ++idx) {
                        const auto& ts = time_steps[idx];
                        int64_t total_hamming_weight = 0;

                        for (const auto& [identifier, value] : ts.signal_values) {
                            // Only count if this identifier is in our scoped set
                            if (scoped_identifiers.find(identifier) == scoped_identifiers.end()) {
                                continue;
                            }

                            if (value.is_int()) {
                                int64_t val = value.to_int();
                                if (val > 0) {
                                    total_hamming_weight += __builtin_popcountll(static_cast<uint64_t>(val));
                                } else if (val < 0) {
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
                    // Fallback for non-VCD traces
                    for (int idx = 0; idx <= max_index; ++idx) {
                        trace->set_index(idx);
                        int64_t total_hamming_weight = 0;

                        for (const auto& signal_name : scoped_signals) {
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

                trace->set_index(original_index);
                return Value(std::make_shared<WList>(results));
            });

        // hamming-distance-scope: Calculate hamming distance for signals in a specific scope
        // Usage: (hamming-distance-scope "top.cpu")
        // Returns: ((timestamp1 distance1) (timestamp2 distance2) ...)
        registry.register_operator("hamming-distance-scope",
            [](SEval& eval, const std::vector<Value>& args) -> Value {
                if (args.size() != 1) {
                    throw EvalError("hamming-distance-scope expects 1 argument (scope-prefix), got " 
                        + std::to_string(args.size()));
                }

                auto evaluated = eval.eval_args(args);
                if (!evaluated[0].is_string()) {
                    throw EvalError("hamming-distance-scope: scope prefix must be a string");
                }
                
                std::string scope_prefix = evaluated[0].to_string();

                auto trace_container = eval.get_traces();
                if (!trace_container) {
                    throw EvalError("hamming-distance-scope: no trace loaded");
                }

                auto trace_ids = trace_container->get_trace_ids();
                if (trace_ids.empty()) {
                    throw EvalError("hamming-distance-scope: no traces available");
                }

                auto trace = trace_container->get_trace(trace_ids[0]);
                if (!trace) {
                    throw EvalError("hamming-distance-scope: could not access trace");
                }

                // Create a new filtered VCD trace with parse-time filtering
                auto vcd_trace = std::dynamic_pointer_cast<VCDTrace>(trace);
                if (!vcd_trace) {
                    throw EvalError("hamming-distance-scope: only VCD traces are currently supported");
                }

                // Create a new VCD trace with scope filter
                auto filtered_trace = std::make_shared<VCDTrace>(
                    "filtered_" + trace_ids[0],
                    vcd_trace->get_filename(),
                    trace_container,
                    scope_prefix  // This triggers parse-time filtering!
                );
                filtered_trace->load();

                // Check if any signals were loaded
                auto filtered_signals = filtered_trace->get_all_signals();
                if (filtered_signals.empty()) {
                    throw EvalError("hamming-distance-scope: no signals found with scope prefix '" 
                        + scope_prefix + "'");
                }

                int max_index = filtered_trace->get_max_index();
                std::vector<Value> results;
                results.reserve(max_index);

                const auto& time_steps = filtered_trace->get_time_steps();
                const auto& signal_info = filtered_trace->get_signal_info();
                const size_t num_steps = std::min(static_cast<size_t>(max_index + 1), time_steps.size());

                // Build vector of identifiers for all loaded signals (already filtered!)
                std::vector<std::string> identifiers;
                identifiers.reserve(filtered_signals.size());
                for (const auto& sig_name : filtered_signals) {
                    auto it = signal_info.find(sig_name);
                    if (it != signal_info.end()) {
                        identifiers.push_back(it->second.identifier);
                    }
                }

                if (num_steps > 0 && !identifiers.empty()) {
                    // Sequential HD calculation
                    std::unordered_map<std::string, int64_t> prev_int_values;
                    std::unordered_map<std::string, std::string> prev_str_values;
                    prev_int_values.reserve(identifiers.size());
                    prev_str_values.reserve(identifiers.size());

                    for (size_t idx = 0; idx < num_steps; ++idx) {
                        const auto& ts = time_steps[idx];
                        int64_t total_distance = 0;

                        std::unordered_map<std::string, int64_t> curr_int_values;
                        std::unordered_map<std::string, std::string> curr_str_values;
                        curr_int_values.reserve(identifiers.size());
                        curr_str_values.reserve(identifiers.size());

                        // Iterate only the filtered identifiers
                        for (const auto& identifier : identifiers) {
                            auto value_it = ts.signal_values.find(identifier);
                            if (value_it == ts.signal_values.end()) {
                                continue;
                            }
                            
                            const auto& value = value_it->second;

                            if (value.is_int()) {
                                int64_t curr_val = value.to_int();
                                curr_int_values[identifier] = curr_val;

                                if (idx > 0) {
                                    auto prev_it = prev_int_values.find(identifier);
                                    if (prev_it != prev_int_values.end()) {
                                        uint64_t diff = static_cast<uint64_t>(curr_val) ^ static_cast<uint64_t>(prev_it->second);
                                        total_distance += __builtin_popcountll(diff);
                                    } else {
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

                                if (idx > 0) {
                                    auto prev_it = prev_str_values.find(identifier);
                                    if (prev_it != prev_str_values.end()) {
                                        total_distance += hamming_distance(prev_it->second, curr_str);
                                    } else {
                                        total_distance += count_ones(curr_str);
                                    }
                                }
                            }
                        }

                        if (idx > 0) {
                            auto result_tuple = std::make_shared<WList>();
                            result_tuple->push_back(Value(ts.timestamp));
                            result_tuple->push_back(Value(total_distance));
                            results.push_back(Value(result_tuple));
                        }

                        prev_int_values = std::move(curr_int_values);
                        prev_str_values = std::move(curr_str_values);
                    }
                }

                return Value(std::make_shared<WList>(results));
            });

    }
};

} // namespace wal::operators
