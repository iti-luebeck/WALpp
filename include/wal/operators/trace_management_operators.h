#pragma once

#include "wal/operator_registry.h"
#include "wal/evaluator.h"
#include "wal/trace/trace_container.h"
#include <algorithm>
#include "wal/signal_resolve.h"

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
private:
    /// A signal name given as a string OR as a symbol.
    ///
    /// Upstream writes these quoted, as `(signal? 'tb.clk)` and
    /// `(signal-width 'tb.dut.counter)`, so demanding a string rejected the spelling
    /// every upstream script uses.
    static std::string signal_name_of(const Value& v, const char* who) {
        if (v.is_string()) return v.to_string();
        if (v.is_symbol()) return v.to_symbol()->get_name();
        throw EvalError(std::string(who) + ": signal name must be a string or symbol");
    }

    /// A trace id given as a string OR as a symbol.
    ///
    /// The WAL manual types every trace id as `symbol?`, so `(load "t.vcd" 'tr)`,
    /// `(unload 'tr)` and `(step 'tr 1)` are the documented spellings. Demanding a
    /// string rejected all of them.
    static std::string trace_id_of(const Value& v, const char* who) {
        if (v.is_string()) return v.to_string();
        if (v.is_symbol()) return v.to_symbol()->get_name();
        throw EvalError(std::string(who) + ": trace-id must be a string or symbol");
    }

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
                trace_id = trace_id_of(evaluated[1], "load");
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

            std::string trace_id = trace_id_of(evaluated[0], "unload");

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
            // Upstream's signature is `(step id... amount)`: any trace ids come
            // first and the step count is last. WAL++ used to read the count first,
            // so `(step 'tr 5)` was rejected. The two orders can never be confused,
            // because a count is an integer and an id never is, so the count is
            // taken wherever it appears and every other argument is an id. That
            // accepts the documented order and keeps the old one working.
            int64_t steps = 1;
            std::vector<std::string> trace_ids;

            auto evaluated = eval.eval_args(args);
            bool have_steps = false;
            for (const auto& arg : evaluated) {
                if (arg.is_int()) {
                    if (have_steps) {
                        throw EvalError("step: expects at most one step count");
                    }
                    steps = arg.to_int();
                    have_steps = true;
                } else {
                    trace_ids.push_back(trace_id_of(arg, "step"));
                }
            }
            if (trace_ids.empty()) {
                trace_ids.push_back("");  // every loaded trace
            }

            try {
                // Returns whether the step actually happened, as upstream does: true
                // while the cursor advances, false once a trace cannot. It used to
                // return the step count, which is truthy for the usual `(step 1)`, so
                // `(while (step 1) ...)` never terminated.
                bool advanced = true;
                for (const auto& id : trace_ids) {
                    if (!eval.get_traces()->step(steps, id).empty()) advanced = false;
                }
                return Value(advanced);
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

            // Seek every loaded trace. Until TraceContainer grew set_index() this
            // operator silently did nothing but echo its argument, so every subsequent
            // get-signal read index 0 -- a whole analysis could look plausible and be
            // answering about the wrong cycle throughout.
            const auto idx = static_cast<int>(evaluated[0].to_int());
            eval.get_traces()->set_index(idx);
            return Value(static_cast<int64_t>(idx));
        });

        // get-index - Get current time index
        registry.register_operator("get-index", [](SEval& eval, const std::vector<Value>& args) -> Value {
            if (args.size() > 1) {
                throw EvalError("get-index expects 0 or 1 arguments (trace id), got " +
                              std::to_string(args.size()));
            }

            const auto indices = eval.get_traces()->indices();
            if (indices.empty()) {
                return Value(static_cast<int64_t>(0));
            }
            if (args.size() == 1) {
                auto evaluated = eval.eval_args(args);
                if (!evaluated[0].is_string()) {
                    throw EvalError("get-index: trace id must be a string");
                }
                const auto it = indices.find(evaluated[0].to_string());
                if (it == indices.end()) {
                    throw EvalError("get-index: no such trace: " + evaluated[0].to_string());
                }
                return Value(static_cast<int64_t>(it->second));
            }
            // Traces are stepped and sought together, so any of them answers for all.
            return Value(static_cast<int64_t>(indices.begin()->second));
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


            std::string signal_name = signal_name_of(evaluated[0], "signal");

            try {
                // Any accepted spelling counts as existing (see signal_resolve.h).
                std::string resolved;
                return Value(resolve_signal_name(eval.get_traces(), signal_name, resolved));
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


            std::string signal_name = signal_name_of(evaluated[0], "signal");

            try {
                std::string resolved = signal_name;
                resolve_signal_name(eval.get_traces(), signal_name, resolved);
                int width = eval.get_traces()->signal_width(resolved);
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


            std::string signal_name = signal_name_of(evaluated[0], "signal");
            int offset = 0;

            if (args.size() >= 2) {
                if (!evaluated[1].is_int()) {
                    throw EvalError("get-signal: offset must be an integer");
                }
                offset = static_cast<int>(evaluated[1].to_int());
            }

            try {
                // Accept every spelling of a hierarchical name (see signal_resolve.h):
                // the interpreter's `^`, the fast paths' and upstream WAL's `.`, and an
                // upstream `tid^path`. Falls through to the name as written, so an
                // unresolvable name still yields the trace layer's own error.
                // An alias may stand in for the path, which is what makes the
                // parser's `clk@-1` desugaring work on an aliased name.
                std::string lookup = signal_name;
                // A special variable can be asked for at an offset too: upstream's
                // `TS@-1` desugars to (get-signal "TS" -1). Resolving it as a signal
                // could never work, since no trace contains a signal called TS.
                {
                    Value special;
                    if (offset == 0 &&
                        resolve_special_variable(eval.get_traces(), lookup, special,
                                                 eval.get_scope())) {
                        return special;
                    }
                    if (offset != 0 && is_special_variable(lookup)) {
                        auto saved = eval.get_traces()->indices();
                        Value result;
                        try {
                            eval.get_traces()->step(offset);
                            resolve_special_variable(eval.get_traces(), lookup, result,
                                                     eval.get_scope());
                        } catch (...) {
                            for (const auto& [tid, index] : saved) {
                                eval.get_traces()->set_index(index, tid);
                            }
                            throw;
                        }
                        for (const auto& [tid, index] : saved) {
                            eval.get_traces()->set_index(index, tid);
                        }
                        return result;
                    }
                }
                if (eval.has_alias(lookup)) {
                    lookup = eval.get_aliases().at(lookup);
                }

                // An offset that lands outside the trace reads as false, it is not an
                // error. The WAL manual defines `expr@off` as a call to `reval`, and
                // `reval` answers false here, so the two spellings have to agree.
                // They did not: `(reval clk -1)` was false while `clk@-1` aborted the
                // run, which killed the `(whenever (!= sig sig@-1) ...)` idiom at
                // index 0 -- the shape the paper's own latency_signals.wal is built
                // on. Only `rising`/`falling` were exercised before, and those offset
                // forward, so nothing caught it.
                if (offset != 0 && eval.get_traces()) {
                    for (const auto& tid : eval.get_traces()->get_trace_ids()) {
                        auto trace = eval.get_traces()->get_trace(tid);
                        if (!trace) continue;
                        int target = trace->get_index() + offset;
                        if (target < 0 || target > trace->get_max_index()) {
                            return Value(false);
                        }
                    }
                }

                std::string resolved = lookup;
                resolve_signal_name(eval.get_traces(), lookup, resolved);
                return eval.get_traces()->signal_value(resolved, offset);
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
