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
     * Calculate Hamming distance between two binary strings.
     * Compares by set-bit-ness (only '1' counts as set), not raw character
     * equality: a literal '!=' would count every 'x'-vs-'0' position as a
     * difference, even though 'x'/'z' are treated as an unset bit everywhere
     * else in this file (count_ones() above) and in WAL++'s Fast Mode -- that
     * mismatch previously produced a spurious full-width distance spike on
     * every wide signal's first resolve from its initial all-x value to a
     * concrete one (the common reset-then-load case).
     */
    static int hamming_distance(const std::string& s1, const std::string& s2) {
        size_t max_len = std::max(s1.length(), s2.length());
        std::string padded1 = pad_to_length(s1, max_len);
        std::string padded2 = pad_to_length(s2, max_len);

        int distance = 0;
        for (size_t i = 0; i < max_len; ++i) {
            if ((padded1[i] == '1') != (padded2[i] == '1')) {
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
                
                // Fast path for VCDTrace: delta computation over sparse time_steps
                auto vcd_trace = std::dynamic_pointer_cast<VCDTrace>(trace);
                if (vcd_trace) {
                    const auto& time_steps = vcd_trace->get_time_steps();

                    if (time_steps.empty()) {
                        throw EvalError("hamming-weight-all-signals-all-timestamps: VCD trace has no time steps");
                    }

                    const size_t num_steps = std::min(static_cast<size_t>(max_index + 1), time_steps.size());

                    // Rolling state: current value per identifier, and running HW total
                    std::unordered_map<std::string, Value> current_state =
                        vcd_trace->get_initial_values();

                    int64_t running_hw = 0;
                    for (const auto& [id, val] : current_state) {
                        if (val.is_int()) {
                            running_hw += __builtin_popcountll(static_cast<uint64_t>(val.to_int()));
                        } else if (val.is_string()) {
                            running_hw += count_ones(val.to_string());
                        }
                    }

                    for (size_t idx = 0; idx < num_steps; ++idx) {
                        const auto& ts = time_steps[idx];

                        // Apply each change: subtract old HW, add new HW
                        for (const auto& [id, new_val] : ts.signal_values) {
                            auto old_it = current_state.find(id);
                            if (old_it != current_state.end()) {
                                const Value& old_val = old_it->second;
                                if (old_val.is_int()) {
                                    running_hw -= __builtin_popcountll(
                                        static_cast<uint64_t>(old_val.to_int()));
                                } else if (old_val.is_string()) {
                                    running_hw -= count_ones(old_val.to_string());
                                }
                            }
                            if (new_val.is_int()) {
                                running_hw += __builtin_popcountll(
                                    static_cast<uint64_t>(new_val.to_int()));
                            } else if (new_val.is_string()) {
                                running_hw += count_ones(new_val.to_string());
                            }
                            current_state[id] = new_val;
                        }

                        auto result_tuple = std::make_shared<WList>();
                        result_tuple->push_back(Value(ts.timestamp));
                        result_tuple->push_back(Value(running_hw));
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
                        
                        int64_t timestamp = trace->get_timestamp();
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
                
                // Fast path for VCDTrace: delta computation over sparse time_steps.
                // Hamming distance between t[i-1] and t[i] = XOR of only the changed
                // signals — unchanged signals contribute 0 difference.
                auto vcd_trace = std::dynamic_pointer_cast<VCDTrace>(trace);
                if (vcd_trace) {
                    const auto& time_steps = vcd_trace->get_time_steps();

                    if (time_steps.empty()) {
                        throw EvalError("hamming-distance-all-signals-all-timestamps: VCD trace has no time steps");
                    }

                    const size_t num_steps = std::min(static_cast<size_t>(max_index + 1), time_steps.size());

                    if (num_steps > 0) {
                        // Rolling state, split by representation for speed on the
                        // common (int-typed, <=64-bit) case: current_state for
                        // ints, current_str_state for signals too wide for
                        // int64_t (see VCDTrace::parse_bit_value).
                        //
                        // A single identifier can flip between the two across
                        // timestamps: parse_bit_value types a value by how many
                        // bits were written on *that* VCD line, not by the
                        // signal's declared width, and VCD is allowed to omit
                        // leading zeros -- so the same wide signal can show up
                        // int-typed on a line where its value happens to fit in
                        // 64 bits. Looking a changed value up in only its own
                        // type's map and treating a miss as "previously zero"
                        // (as a first, simpler fix here did) silently overcounts
                        // distance at exactly those crossover timestamps. The
                        // int64_to_bits fallback below handles that crossover
                        // correctly at the (rare) cost of one string conversion,
                        // without giving up the int/int fast path otherwise.
                        std::unordered_map<std::string, int64_t> current_state;
                        std::unordered_map<std::string, std::string> current_str_state;
                        for (const auto& [id, val] : vcd_trace->get_initial_values()) {
                            if (val.is_int()) current_state[id] = val.to_int();
                            else if (val.is_string()) current_str_state[id] = val.to_string();
                        }

                        auto int64_to_bits = [](int64_t v) {
                            std::string bits(64, '0');
                            uint64_t u = static_cast<uint64_t>(v);
                            for (int i = 0; i < 64; ++i) {
                                if (u & (uint64_t(1) << i)) bits[63 - i] = '1';
                            }
                            return bits;
                        };

                        for (size_t idx = 0; idx < num_steps; ++idx) {
                            const auto& ts = time_steps[idx];

                            if (idx > 0) {
                                // Distance = XOR over changed signals only
                                int64_t total_distance = 0;
                                for (const auto& [id, new_val] : ts.signal_values) {
                                    if (new_val.is_int()) {
                                        int64_t new_int = new_val.to_int();
                                        auto old_it = current_state.find(id);
                                        if (old_it != current_state.end()) {
                                            uint64_t diff = static_cast<uint64_t>(new_int) ^
                                                            static_cast<uint64_t>(old_it->second);
                                            total_distance += __builtin_popcountll(diff);
                                        } else {
                                            auto old_str_it = current_str_state.find(id);
                                            total_distance += (old_str_it != current_str_state.end())
                                                ? hamming_distance(old_str_it->second, int64_to_bits(new_int))
                                                : __builtin_popcountll(static_cast<uint64_t>(new_int));
                                        }
                                    } else if (new_val.is_string()) {
                                        const std::string& new_str = new_val.to_string();
                                        auto old_it = current_str_state.find(id);
                                        if (old_it != current_str_state.end()) {
                                            total_distance += hamming_distance(old_it->second, new_str);
                                        } else {
                                            auto old_int_it = current_state.find(id);
                                            total_distance += (old_int_it != current_state.end())
                                                ? hamming_distance(int64_to_bits(old_int_it->second), new_str)
                                                : count_ones(new_str);
                                        }
                                    }
                                }

                                auto result_tuple = std::make_shared<WList>();
                                result_tuple->push_back(Value(ts.timestamp));
                                result_tuple->push_back(Value(total_distance));
                                results.push_back(Value(result_tuple));
                            }

                            // Apply changes to rolling state AFTER computing distance.
                            // A signal that changed representation this step moves
                            // fully to the new map so a stale entry in the old one
                            // can't be mistaken for "still current" later.
                            for (const auto& [id, new_val] : ts.signal_values) {
                                if (new_val.is_int()) {
                                    current_state[id] = new_val.to_int();
                                    current_str_state.erase(id);
                                } else if (new_val.is_string()) {
                                    current_str_state[id] = new_val.to_string();
                                    current_state.erase(id);
                                }
                            }
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
                            
                            // Get actual simulation timestamp
                            int64_t timestamp = trace->get_timestamp();
                            
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

                    // Running weight of the *entire* current scoped state,
                    // seeded from $dumpvars and updated incrementally -- see
                    // hamming-weight-all-signals-all-timestamps. Weight is a
                    // property of the whole state, not just the signals that
                    // happen to change a given timestep, so it must be
                    // accumulated across timestamps rather than recomputed
                    // from only ts.signal_values each time (what this used to
                    // do, which is really a per-step delta, not a weight).
                    std::unordered_map<std::string, Value> current_state;
                    int64_t running_hw = 0;
                    for (const auto& [id, val] : vcd_trace->get_initial_values()) {
                        if (!scoped_identifiers.count(id)) continue;
                        current_state[id] = val;
                        if (val.is_int()) running_hw += __builtin_popcountll(static_cast<uint64_t>(val.to_int()));
                        else if (val.is_string()) running_hw += count_ones(val.to_string());
                    }

                    for (size_t idx = 0; idx < num_steps; ++idx) {
                        const auto& ts = time_steps[idx];

                        for (const auto& [identifier, new_val] : ts.signal_values) {
                            if (!scoped_identifiers.count(identifier)) continue;

                            auto old_it = current_state.find(identifier);
                            if (old_it != current_state.end()) {
                                const Value& old_val = old_it->second;
                                if (old_val.is_int()) running_hw -= __builtin_popcountll(static_cast<uint64_t>(old_val.to_int()));
                                else if (old_val.is_string()) running_hw -= count_ones(old_val.to_string());
                            }
                            if (new_val.is_int()) running_hw += __builtin_popcountll(static_cast<uint64_t>(new_val.to_int()));
                            else if (new_val.is_string()) running_hw += count_ones(new_val.to_string());
                            current_state[identifier] = new_val;
                        }

                        auto result_tuple = std::make_shared<WList>();
                        result_tuple->push_back(Value(ts.timestamp));
                        result_tuple->push_back(Value(running_hw));
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

                        int64_t timestamp = trace->get_timestamp();
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

                auto vcd_trace = std::dynamic_pointer_cast<VCDTrace>(trace);
                if (!vcd_trace) {
                    throw EvalError("hamming-distance-scope: only VCD traces are currently supported");
                }

                // Filter the already-loaded main trace's signals by scope
                // prefix -- same approach as hamming-weight-scope, and the
                // same data hamming-distance-all-signals-all-timestamps uses
                // unfiltered. A second, independently re-parsed VCDTrace
                // (scope_filter passed at construction) used to be built
                // here instead; that path produced wrong results for wide
                // (>64-bit) signals in scope for reasons not fully isolated,
                // so this now reuses the one trace/parse already verified
                // correct rather than trusting a second one.
                auto all_signals = trace->get_all_signals();
                std::vector<std::string> scoped_signals;
                for (const auto& signal : all_signals) {
                    if (signal.length() >= scope_prefix.length() &&
                        signal.substr(0, scope_prefix.length()) == scope_prefix) {
                        scoped_signals.push_back(signal);
                    }
                }
                if (scoped_signals.empty()) {
                    throw EvalError("hamming-distance-scope: no signals found with scope prefix '"
                        + scope_prefix + "'");
                }

                int max_index = vcd_trace->get_max_index();
                std::vector<Value> results;
                results.reserve(max_index);

                const auto& time_steps = vcd_trace->get_time_steps();
                const auto& signal_info = vcd_trace->get_signal_info();
                const size_t num_steps = std::min(static_cast<size_t>(max_index + 1), time_steps.size());

                // Build vector of identifiers for the scoped signals
                std::vector<std::string> identifiers;
                identifiers.reserve(scoped_signals.size());
                for (const auto& sig_name : scoped_signals) {
                    auto it = signal_info.find(sig_name);
                    if (it != signal_info.end()) {
                        identifiers.push_back(it->second.identifier);
                    }
                }

                if (num_steps > 0 && !identifiers.empty()) {
                    // Rolling state persists across timestamps -- see
                    // hamming-distance-all-signals-all-timestamps for why
                    // rebuilding it fresh each iteration (as this used to,
                    // via std::move(curr_*) below) is wrong: any identifier
                    // that goes even one timestamp without changing was
                    // dropped from prev_*_values and then miscounted as
                    // brand new next time it changed, overcounting distance.
                    // Also seeded from $dumpvars up front, for the same
                    // reason: without it, a signal only ever seen at t=0
                    // that first appears in identifiers' delta stream much
                    // later would be missing a real previous value too.
                    //
                    // Split by representation (int vs. string) purely for
                    // speed on the common <=64-bit case; a signal can flip
                    // representation across timestamps (VCD omits leading
                    // zeros, and VCDTrace types a value by how many bits were
                    // written on that line, not the signal's declared width),
                    // so a miss in one map falls back to the other rather
                    // than assuming "previously zero".
                    std::unordered_map<std::string, int64_t> prev_int_values;
                    std::unordered_map<std::string, std::string> prev_str_values;
                    prev_int_values.reserve(identifiers.size());
                    prev_str_values.reserve(identifiers.size());

                    std::unordered_set<std::string> scoped_ids(identifiers.begin(), identifiers.end());
                    for (const auto& [id, val] : vcd_trace->get_initial_values()) {
                        if (!scoped_ids.count(id)) continue;
                        if (val.is_int()) prev_int_values[id] = val.to_int();
                        else if (val.is_string()) prev_str_values[id] = val.to_string();
                    }

                    auto int64_to_bits = [](int64_t v) {
                        std::string bits(64, '0');
                        uint64_t u = static_cast<uint64_t>(v);
                        for (int i = 0; i < 64; ++i) {
                            if (u & (uint64_t(1) << i)) bits[63 - i] = '1';
                        }
                        return bits;
                    };

                    for (size_t idx = 0; idx < num_steps; ++idx) {
                        const auto& ts = time_steps[idx];

                        if (idx > 0) {
                            int64_t total_distance = 0;

                            // Iterate ts.signal_values (unique per identifier)
                            // filtered by scope membership, not the
                            // `identifiers` vector: many signal *names* can
                            // alias the same VCD identifier (routine in
                            // gate-level netlists), so identifiers can contain
                            // the same id more than once, and looking each one
                            // up in ts.signal_values re-added that one real
                            // change's distance once per alias -- see
                            // hamming-weight-scope, which avoids this the same
                            // way.
                            for (const auto& [identifier, value] : ts.signal_values) {
                                if (!scoped_ids.count(identifier)) continue;

                                if (value.is_int()) {
                                    int64_t curr_val = value.to_int();
                                    auto prev_it = prev_int_values.find(identifier);
                                    if (prev_it != prev_int_values.end()) {
                                        uint64_t diff = static_cast<uint64_t>(curr_val) ^ static_cast<uint64_t>(prev_it->second);
                                        total_distance += __builtin_popcountll(diff);
                                    } else {
                                        auto prev_str_it = prev_str_values.find(identifier);
                                        total_distance += (prev_str_it != prev_str_values.end())
                                            ? hamming_distance(prev_str_it->second, int64_to_bits(curr_val))
                                            : __builtin_popcountll(static_cast<uint64_t>(curr_val));
                                    }
                                } else if (value.is_string()) {
                                    const std::string& curr_str = value.to_string();
                                    auto prev_it = prev_str_values.find(identifier);
                                    if (prev_it != prev_str_values.end()) {
                                        total_distance += hamming_distance(prev_it->second, curr_str);
                                    } else {
                                        auto prev_int_it = prev_int_values.find(identifier);
                                        total_distance += (prev_int_it != prev_int_values.end())
                                            ? hamming_distance(int64_to_bits(prev_int_it->second), curr_str)
                                            : count_ones(curr_str);
                                    }
                                }
                            }

                            auto result_tuple = std::make_shared<WList>();
                            result_tuple->push_back(Value(ts.timestamp));
                            result_tuple->push_back(Value(total_distance));
                            results.push_back(Value(result_tuple));
                        }

                        // Apply this timestep's changes to state AFTER computing
                        // distance (or unconditionally for idx==0, which only
                        // completes the initial state alongside the $dumpvars seed).
                        for (const auto& [identifier, value] : ts.signal_values) {
                            if (!scoped_ids.count(identifier)) continue;
                            if (value.is_int()) {
                                prev_int_values[identifier] = value.to_int();
                                prev_str_values.erase(identifier);
                            } else if (value.is_string()) {
                                prev_str_values[identifier] = value.to_string();
                                prev_int_values.erase(identifier);
                            }
                        }
                    }
                }

                return Value(std::make_shared<WList>(results));
            });

    }
};

} // namespace wal::operators
