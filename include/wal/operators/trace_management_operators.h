#pragma once

#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include "wal/trace/trace_container.h"
#include <algorithm>

namespace wal::operators {

/**
 * @brief Trace file management operators (Phase 8)
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
 *
 * All implementations are header-only for compiler optimization.
 */
class TraceManagementOperators {
public:
    /**
     * @brief Register all trace management operators
     */
    static void register_all(OperatorRegistry& registry) {
        // ═══════════════════════════════════════════════════════════════
        // TRACE LOADING & MANAGEMENT
        // ═══════════════════════════════════════════════════════════════
        
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

        // ═══════════════════════════════════════════════════════════════
        // TRACE NAVIGATION
        // ═══════════════════════════════════════════════════════════════

        // step - Move all traces forward/backward in time
        registry.register_operator("step", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() > 2) {
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

        // set-index - Set the current time index
        registry.register_operator("set-index", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("set-index expects 1 argument (index), got " + std::to_string(args.size()));
            }

            auto evaluated = eval.eval_args(args);

            if (!evaluated[0].is_int()) {
                throw EvalError("set-index: index must be an integer");
            }

            // Note: set-index is complex - would need TraceContainer support
            // For now, returns the index
            return Value(evaluated[0].to_int());
        });

        // get-index - Get current time index
        registry.register_operator("get-index", [](SEval& /*eval*/, const std::vector<Value>& args) -> Value {
            if (args.size() != 0) {
                throw EvalError("get-index expects 0 arguments, got " + std::to_string(args.size()));
            }

            // Note: Would return actual index from TraceContainer
            // For now, returns 0
            return Value(static_cast<int64_t>(0));
        });

        // ═══════════════════════════════════════════════════════════════
        // TRACE QUERYING
        // ═══════════════════════════════════════════════════════════════

        // signal? - Check if a signal exists in current traces
        registry.register_operator("signal?", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("signal? expects 1 argument (signal-name), got " + std::to_string(args.size()));
            }

            auto evaluated = eval.eval_args(args);

            if (!evaluated[0].is_string()) {
                throw EvalError("signal?: signal name must be a string");
            }

            std::string signal_name = evaluated[0].to_string();

            try {
                return Value(eval.get_traces()->contains(signal_name));
            } catch (const std::exception&) {
                return Value(false);
            }
        });

        // signal-width - Get bit width of a signal
        registry.register_operator("signal-width", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 1) {
                throw EvalError("signal-width expects 1 argument (signal-name), got " + std::to_string(args.size()));
            }

            auto evaluated = eval.eval_args(args);

            if (!evaluated[0].is_string()) {
                throw EvalError("signal-width: signal name must be a string");
            }

            std::string signal_name = evaluated[0].to_string();

            try {
                int width = eval.get_traces()->signal_width(signal_name);
                return Value(static_cast<int64_t>(width));
            } catch (const std::exception& e) {
                throw EvalError("signal-width: " + std::string(e.what()));
            }
        });

        // get-signal - Get value of a signal at current or offset index
        registry.register_operator("get-signal", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() < 1 || args.size() > 2) {
                throw EvalError("get-signal expects 1-2 arguments (signal-name [offset]), got " + 
                              std::to_string(args.size()));
            }

            auto evaluated = eval.eval_args(args);

            if (!evaluated[0].is_string()) {
                throw EvalError("get-signal: signal name must be a string");
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

        // ═══════════════════════════════════════════════════════════════
        // TRACE METADATA
        // ═══════════════════════════════════════════════════════════════

        // get-traces - Get list of loaded trace IDs
        registry.register_operator("get-traces", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 0) {
                throw EvalError("get-traces expects 0 arguments, got " + std::to_string(args.size()));
            }

            try {
                auto trace_ids = eval.get_traces()->get_trace_ids();
                auto result_list = std::make_shared<WList>();
                for (const auto& tid : trace_ids) {
                    result_list->push_back(Value(std::make_shared<Symbol>(tid)));
                }
                return Value(result_list);
            } catch (const std::exception& e) {
                throw EvalError("get-traces: " + std::string(e.what()));
            }
        });

        // signal-names - Get all signal names in loaded traces
        registry.register_operator("signal-names", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() != 0) {
                throw EvalError("signal-names expects 0 arguments, got " + std::to_string(args.size()));
            }

            try {
                auto traces = eval.get_traces()->get_trace_ids();
                
                // Return empty list if no traces loaded
                if (traces.empty()) {
                    return Value(std::make_shared<WList>());
                }

                // Get signals from first trace
                auto first_trace = eval.get_traces()->get_trace(traces[0]);
                if (!first_trace) {
                    throw EvalError("signal-names: could not access trace");
                }

                auto signal_names = first_trace->get_all_signals();
                auto result_list = std::make_shared<WList>();
                for (const auto& name : signal_names) {
                    result_list->push_back(Value(std::make_shared<Symbol>(name)));
                }
                return Value(result_list);
            } catch (const std::exception& e) {
                throw EvalError("signal-names: " + std::string(e.what()));
            }
        });
    }
};

} // namespace wal::operators
