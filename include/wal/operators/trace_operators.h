#pragma once

#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include "wal/trace/trace_container.h"
#include <algorithm>
#include <vector>
#include <functional>

namespace wal {

/**
 * @brief Phase 8: Trace File Support Operators
 *
 * Operators for loading, managing, and querying waveform trace files:
 * - Load/unload trace files (VCD, FST, CSV formats)
 * - Navigate traces (step, set-index, get-index)
 * - Query signal values and metadata
 * - Multi-trace management
 *
 * Operators (10 total):
 * - load: Load a trace file (VCD, FST, or CSV)
 * - unload: Unload a loaded trace
 * - step: Move all traces forward/backward in time
 * - signal?: Check if a signal exists in current traces
 * - get-signal: Get value of a signal at current or offset index
 * - signal-width: Get bit width of a signal
 * - set-index: Set the current time index
 * - get-index: Get current time index
 * - get-traces: Get list of loaded trace IDs
 * - signal-names: Get all signal names in loaded traces
 */
inline void register_trace_operators(OperatorRegistry& registry) {
    // load - Load a trace file (VCD, FST, or CSV)
    registry.register_operator("load", [](SEval& eval, const std::vector<Value>& args) -> Value {
        if (args.size() < 1 || args.size() > 3) {
            throw EvalError("load expects 1-3 arguments (filename [trace-id [signal-filter]]), got " + 
                          std::to_string(args.size()));
        }

        auto evaluated = eval.eval_args(args);

        if (!evaluated[0].is_string()) {
            throw EvalError("load: filename must be a string");
        }

        std::string filename = evaluated[0].to_string();
        std::string trace_id = "";
        std::vector<std::string>* keep_signals = nullptr;

        if (args.size() >= 2) {
            if (!evaluated[1].is_string()) {
                throw EvalError("load: trace-id must be a string");
            }
            trace_id = evaluated[1].to_string();
        }

        if (args.size() >= 3) {
            if (!evaluated[2].is_list()) {
                throw EvalError("load: signal filter must be a list of strings");
            }
            // For now, ignore filter list - stub implementation
            // In full implementation, would pass keep_signals to TraceContainer::load()
        }

        try {
            eval.get_traces()->load(filename, trace_id, keep_signals);
            return Value(std::make_shared<Symbol>(trace_id.empty() ? "trace" : trace_id));
        } catch (const std::exception& e) {
            throw EvalError("load: failed to load trace: " + std::string(e.what()));
        }
    });

    // unload - Unload a loaded trace
    registry.register_operator("unload", [](SEval& eval, const std::vector<Value>& args) -> Value {
        if (args.size() != 1) {
            throw EvalError("unload expects 1 argument (trace-id), got " + std::to_string(args.size()));
        }

        auto evaluated = eval.eval_args(args);

        if (!evaluated[0].is_string()) {
            throw EvalError("unload: trace-id must be a string");
        }

        std::string trace_id = evaluated[0].to_string();

        try {
            eval.get_traces()->unload(trace_id);
            return Value(true);
        } catch (const std::exception& e) {
            throw EvalError("unload: failed to unload trace: " + std::string(e.what()));
        }
    });

    // step - Move all traces forward/backward in time
    registry.register_operator("step", [](SEval& eval, const std::vector<Value>& args) -> Value {
        if (args.size() < 0 || args.size() > 2) {
            throw EvalError("step expects 0-2 arguments ([steps [trace-id]]), got " + 
                          std::to_string(args.size()));
        }

        int64_t steps = 1;
        std::string trace_id = "";

        if (args.size() >= 1) {
            auto evaluated = eval.eval_args(args);
            if (!evaluated[0].is_int()) {
                throw EvalError("step: steps must be an integer");
            }
            steps = evaluated[0].to_int();
        }

        if (args.size() >= 2) {
            auto evaluated = eval.eval_args(args);
            if (!evaluated[1].is_string()) {
                throw EvalError("step: trace-id must be a string");
            }
            trace_id = evaluated[1].to_string();
        }

        try {
            eval.get_traces()->step(steps, trace_id);
            return Value(static_cast<int64_t>(steps));
        } catch (const std::exception& e) {
            throw EvalError("step: " + std::string(e.what()));
        }
    });

    // signal? - Check if a signal exists
    registry.register_operator("signal?", [](SEval& eval, const std::vector<Value>& args) -> Value {
        if (args.size() != 1) {
            throw EvalError("signal? expects 1 argument (signal-name), got " + std::to_string(args.size()));
        }

        auto evaluated = eval.eval_args(args);

        if (!evaluated[0].is_string()) {
            throw EvalError("signal?: signal-name must be a string");
        }

        std::string signal_name = evaluated[0].to_string();
        bool exists = eval.get_traces()->contains(signal_name);

        return Value(exists);
    });

    // get-signal - Get value of a signal at current or offset index
    registry.register_operator("get-signal", [](SEval& eval, const std::vector<Value>& args) -> Value {
        if (args.size() < 1 || args.size() > 2) {
            throw EvalError("get-signal expects 1-2 arguments (signal-name [offset]), got " + 
                          std::to_string(args.size()));
        }

        auto evaluated = eval.eval_args(args);

        if (!evaluated[0].is_string()) {
            throw EvalError("get-signal: signal-name must be a string");
        }

        std::string signal_name = evaluated[0].to_string();
        int offset = 0;

        if (args.size() >= 2) {
            if (!evaluated[1].is_int()) {
                throw EvalError("get-signal: offset must be an integer");
            }
            offset = static_cast<int>(evaluated[1].to_int());
        }

        try {
            return eval.get_traces()->signal_value(signal_name, offset);
        } catch (const std::exception& e) {
            throw EvalError("get-signal: " + std::string(e.what()));
        }
    });

    // signal-width - Get bit width of a signal
    registry.register_operator("signal-width", [](SEval& eval, const std::vector<Value>& args) -> Value {
        if (args.size() != 1) {
            throw EvalError("signal-width expects 1 argument (signal-name), got " + std::to_string(args.size()));
        }

        auto evaluated = eval.eval_args(args);

        if (!evaluated[0].is_string()) {
            throw EvalError("signal-width: signal-name must be a string");
        }

        std::string signal_name = evaluated[0].to_string();

        try {
            int width = eval.get_traces()->signal_width(signal_name);
            return Value(static_cast<int64_t>(width));
        } catch (const std::exception& e) {
            throw EvalError("signal-width: " + std::string(e.what()));
        }
    });

    // set-index - Set the current time index
    registry.register_operator("set-index", [](SEval& eval, const std::vector<Value>& args) -> Value {
        if (args.size() < 1 || args.size() > 2) {
            throw EvalError("set-index expects 1-2 arguments (index [trace-id]), got " + 
                          std::to_string(args.size()));
        }

        auto evaluated = eval.eval_args(args);

        if (!evaluated[0].is_int()) {
            throw EvalError("set-index: index must be an integer");
        }

        int64_t index = evaluated[0].to_int();
        std::string trace_id = "";

        if (args.size() >= 2) {
            if (!evaluated[1].is_string()) {
                throw EvalError("set-index: trace-id must be a string");
            }
            trace_id = evaluated[1].to_string();
        }

        try {
            if (!trace_id.empty()) {
                auto trace = eval.get_traces()->get_trace(trace_id);
                if (!trace) {
                    throw EvalError("set-index: trace not found: " + trace_id);
                }
                trace->set_index(static_cast<int>(index));
            } else {
                // Set index for all traces
                auto& traces = eval.get_traces()->get_traces();
                for (auto& [_, trace] : traces) {
                    if (trace) {
                        trace->set_index(static_cast<int>(index));
                    }
                }
            }
            return Value(index);
        } catch (const std::exception& e) {
            throw EvalError("set-index: " + std::string(e.what()));
        }
    });

    // get-index - Get current time index
    registry.register_operator("get-index", [](SEval& eval, const std::vector<Value>& args) -> Value {
        if (args.size() > 1) {
            throw EvalError("get-index expects 0-1 arguments ([trace-id]), got " + std::to_string(args.size()));
        }

        std::string trace_id = "";

        if (args.size() >= 1) {
            auto evaluated = eval.eval_args(args);
            if (!evaluated[0].is_string()) {
                throw EvalError("get-index: trace-id must be a string");
            }
            trace_id = evaluated[0].to_string();
        }

        try {
            if (!trace_id.empty()) {
                auto trace = eval.get_traces()->get_trace(trace_id);
                if (!trace) {
                    throw EvalError("get-index: trace not found: " + trace_id);
                }
                return Value(static_cast<int64_t>(trace->get_index()));
            } else {
                // Return map of all indices
                auto indices = eval.get_traces()->indices();
                std::vector<Value> result;
                for (const auto& [id, idx] : indices) {
                    auto pair = std::make_shared<WList>(
                        std::vector<Value>{
                            Value(std::make_shared<Symbol>(id)),
                            Value(static_cast<int64_t>(idx))
                        }
                    );
                    result.push_back(Value(pair));
                }
                return Value(std::make_shared<WList>(result));
            }
        } catch (const std::exception& e) {
            throw EvalError("get-index: " + std::string(e.what()));
        }
    });

    // get-traces - Get list of loaded trace IDs
    registry.register_operator("get-traces", [](SEval& eval, const std::vector<Value>& args) -> Value {
        if (args.size() != 0) {
            throw EvalError("get-traces expects 0 arguments, got " + std::to_string(args.size()));
        }

        try {
            auto trace_ids = eval.get_traces()->get_trace_ids();
            std::vector<Value> result;
            for (const auto& id : trace_ids) {
                result.push_back(Value(std::make_shared<Symbol>(id)));
            }
            return Value(std::make_shared<WList>(result));
        } catch (const std::exception& e) {
            throw EvalError("get-traces: " + std::string(e.what()));
        }
    });

    // signal-names - Get all signal names in loaded traces
    registry.register_operator("signal-names", [](SEval& eval, const std::vector<Value>& args) -> Value {
        if (args.size() > 1) {
            throw EvalError("signal-names expects 0-1 arguments ([trace-id]), got " + std::to_string(args.size()));
        }

        std::string trace_id = "";

        if (args.size() >= 1) {
            auto evaluated = eval.eval_args(args);
            if (!evaluated[0].is_string()) {
                throw EvalError("signal-names: trace-id must be a string");
            }
            trace_id = evaluated[0].to_string();
        }

        try {
            std::vector<Value> result;

            if (!trace_id.empty()) {
                auto trace = eval.get_traces()->get_trace(trace_id);
                if (!trace) {
                    throw EvalError("signal-names: trace not found: " + trace_id);
                }
                auto signals = trace->get_signal_names();
                for (const auto& sig : signals) {
                    result.push_back(Value(std::make_shared<Symbol>(sig)));
                }
            } else {
                // Collect signals from all traces (return empty list if no traces)
                auto& traces = eval.get_traces()->get_traces();
                if (!traces.empty()) {
                    for (const auto& [_, trace] : traces) {
                        if (trace) {
                            auto signals = trace->get_signal_names();
                            for (const auto& sig : signals) {
                                result.push_back(Value(std::make_shared<Symbol>(sig)));
                            }
                        }
                    }
                    // Remove duplicates
                    std::sort(result.begin(), result.end(),
                        [](const Value& a, const Value& b) {
                            return a.to_symbol()->get_name() < b.to_symbol()->get_name();
                        });
                    result.erase(std::unique(result.begin(), result.end(),
                        [](const Value& a, const Value& b) {
                            return a.to_symbol()->get_name() == b.to_symbol()->get_name();
                        }), result.end());
                }
            }

            return Value(std::make_shared<WList>(result));
        } catch (const std::exception& e) {
            throw EvalError("signal-names: " + std::string(e.what()));
        }
    });
}

} // namespace wal

