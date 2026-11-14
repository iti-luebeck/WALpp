#include "wal/trace/fst_trace.h"
#include <stdexcept>
#include <cstring>
#include <algorithm>

// GTKWave fstapi — standard FST format reader
extern "C" {
#include "fstapi.h"
}

namespace wal {

// ---------------------------------------------------------------------------
// Internal callback state (file-scope, not visible outside this TU)
// ---------------------------------------------------------------------------
namespace {

struct CallbackState {
    // Signal lookup (populated during hierarchy walk)
    std::unordered_map<fstHandle, std::vector<std::string>>* handle_to_signal;
    std::unordered_map<fstHandle, int>*         handle_to_width;
    // Rolling current values (updated per change event)
    std::unordered_map<std::string, Value>*     current_values;
    // Raw collected steps: (timestamp, snapshot) — built by callback,
    // converted to FSTTrace::TimeStep by load() afterwards
    struct RawStep {
        int64_t timestamp;
        std::unordered_map<std::string, Value> signal_values;
    };
    std::vector<RawStep>* raw_steps;

    uint64_t prev_time = 0;
    bool     has_prev  = false;
};

static Value parse_fst_value(const unsigned char* val_str) {
    // fstapi delivers null-terminated binary strings: "0", "1", "x", "z",
    // or multi-bit "00101101" etc.
    int64_t result = 0;
    for (const unsigned char* p = val_str; *p; ++p) {
        result <<= 1;
        if (*p == '1') result |= 1;
        // 'x', 'z', '0' → bit stays 0
    }
    return Value(result);
}

static void value_change_cb(void* user_data,
                             uint64_t time,
                             fstHandle handle,
                             const unsigned char* value)
{
    auto* s = static_cast<CallbackState*>(user_data);

    // When time advances, snapshot the current values as a completed step
    if (s->has_prev && time != s->prev_time) {
        CallbackState::RawStep step;
        step.timestamp    = static_cast<int64_t>(s->prev_time);
        step.signal_values = *s->current_values;
        s->raw_steps->push_back(std::move(step));
    }
    s->has_prev  = true;
    s->prev_time = time;

    auto it = s->handle_to_signal->find(handle);
    if (it != s->handle_to_signal->end()) {
        // One FST handle can back several names: FST marks the second and later
        // occurrences of a signal as aliases, which is how a port wired through a
        // hierarchy appears. They are real, separately named signals.
        Value parsed = parse_fst_value(value);
        for (const auto& name : it->second) {
            (*s->current_values)[name] = parsed;
        }
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// FSTTrace::load() — replaces the old hand-rolled parser
// ---------------------------------------------------------------------------
void FSTTrace::load() {
    void* ctx = fstReaderOpen(filename.c_str());
    if (!ctx) {
        throw std::runtime_error("Failed to load FST file: cannot open " + filename);
    }

    try {
        // ----------------------------------------------------------------
        // 1. Walk the signal hierarchy
        // ----------------------------------------------------------------
        std::unordered_map<fstHandle, std::vector<std::string>> handle_to_signal;
        std::unordered_map<fstHandle, int>         handle_to_width;
        std::string current_scope;

        fstReaderIterateHierRewind(ctx);
        struct fstHier* hier;
        while ((hier = fstReaderIterateHier(ctx)) != nullptr) {
            switch (hier->htyp) {
                case FST_HT_SCOPE:
                    if (!current_scope.empty())
                        current_scope += Trace::SCOPE_SEPARATOR;
                    current_scope += hier->u.scope.name;
                    if (std::find(scope_names.begin(), scope_names.end(), current_scope)
                            == scope_names.end())
                        scope_names.push_back(current_scope);
                    break;

                case FST_HT_UPSCOPE: {
                    auto sep = current_scope.rfind(Trace::SCOPE_SEPARATOR);
                    current_scope = (sep != std::string::npos)
                                    ? current_scope.substr(0, sep)
                                    : "";
                    break;
                }

                case FST_HT_VAR: {
                    // Aliases used to be skipped here, which silently dropped every
                    // signal FST had already seen under another name: counter.fst
                    // listed 4 signals where counter.vcd listed 7, and tb.dut.clk
                    // could not be read at all from an FST.

                    // FST spells a vector variable "counter [3:0]". The range is not
                    // part of the name -- the VCD reader does not keep it, and leaving
                    // it on meant `tb.dut.counter` did not resolve against an FST while
                    // the identical VCD worked. Width is kept from u.var.length.
                    std::string var_name = hier->u.var.name;
                    auto bracket = var_name.find(" [");
                    if (bracket != std::string::npos && var_name.back() == ']') {
                        var_name = var_name.substr(0, bracket);
                    }
                    std::string full_name = current_scope.empty()
                        ? var_name
                        : current_scope + Trace::SCOPE_SEPARATOR + var_name;

                    handle_to_signal[hier->u.var.handle].push_back(full_name);
                    handle_to_width[hier->u.var.handle]  = static_cast<int>(hier->u.var.length);

                    SignalInfo info;
                    info.name    = var_name;
                    info.wire_id = static_cast<int>(hier->u.var.handle);
                    info.width   = static_cast<int>(hier->u.var.length);
                    info.scope   = current_scope;
                    info.kind    = static_cast<char>(hier->u.var.typ);

                    if (signal_info.find(full_name) == signal_info.end()) {
                        signal_info[full_name] = info;
                        signal_names.push_back(full_name);
                        current_values[full_name] = Value(0);
                    }
                    // The reverse map keeps the first (non-alias) name for a handle.
                    if (wire_id_to_signal.find(static_cast<int>(hier->u.var.handle))
                            == wire_id_to_signal.end()) {
                        wire_id_to_signal[hier->u.var.handle] = full_name;
                    }
                    break;
                }

                default:
                    break;
            }
        }

        // ----------------------------------------------------------------
        // 2. Iterate value-change blocks via callback
        // ----------------------------------------------------------------
        std::vector<CallbackState::RawStep> raw_steps;

        CallbackState state;
        state.handle_to_signal = &handle_to_signal;
        state.handle_to_width  = &handle_to_width;
        state.current_values   = &current_values;
        state.raw_steps        = &raw_steps;

        fstReaderSetFacProcessMaskAll(ctx);
        fstReaderIterBlocks(ctx, value_change_cb, &state, nullptr);

        // Flush the final in-flight timestep
        if (state.has_prev) {
            CallbackState::RawStep step;
            step.timestamp    = static_cast<int64_t>(state.prev_time);
            step.signal_values = current_values;
            raw_steps.push_back(std::move(step));
        }

        // ----------------------------------------------------------------
        // 3. Convert raw steps to FSTTrace::TimeStep
        // ----------------------------------------------------------------
        time_steps.reserve(raw_steps.size());
        for (int i = 0; i < static_cast<int>(raw_steps.size()); ++i) {
            TimeStep ts;
            ts.timestamp    = raw_steps[i].timestamp;
            ts.signal_values = std::move(raw_steps[i].signal_values);
            time_steps.push_back(std::move(ts));
            timestamp_index_map[ts.timestamp] = i;
        }

        max_index = static_cast<int>(time_steps.size()) - 1;

    } catch (...) {
        fstReaderClose(ctx);
        throw;
    }

    fstReaderClose(ctx);
}

// ---------------------------------------------------------------------------
// Remaining methods — unchanged from original
// ---------------------------------------------------------------------------

Value FSTTrace::signal_value(const std::string& name, int offset,
                             const std::string& scope)
{
    std::string full_name = name;
    if (!scope.empty())
        full_name = scope + Trace::SCOPE_SEPARATOR + name;

    if (signal_info.find(full_name) == signal_info.end() &&
        signal_info.find(name) == signal_info.end())
        throw std::runtime_error("Signal not found: " + full_name);

    std::string lookup = (signal_info.count(full_name)) ? full_name : name;

    int target = current_index + offset;
    if (target < 0 || target > max_index)
        throw std::runtime_error("Index out of range");

    if (target < static_cast<int>(time_steps.size())) {
        auto& ts = time_steps[target];
        auto  it = ts.signal_values.find(lookup);
        if (it != ts.signal_values.end()) return it->second;
    }

    auto it = current_values.find(lookup);
    if (it != current_values.end()) return it->second;
    return Value(0);
}

int FSTTrace::signal_width(const std::string& name) {
    auto it = signal_info.find(name);
    if (it != signal_info.end()) return it->second.width;
    for (const auto& [full, info] : signal_info)
        if (info.name == name) return info.width;
    throw std::runtime_error("Signal not found: " + name);
}

bool FSTTrace::contains_signal(const std::string& name) const {
    if (signal_info.count(name)) return true;
    for (const auto& [full, info] : signal_info)
        if (info.name == name) return true;
    return false;
}

std::vector<std::string> FSTTrace::get_all_signals() const {
    return signal_names;
}

int64_t FSTTrace::get_timestamp() const {
    if (current_index >= 0 && current_index < static_cast<int>(time_steps.size()))
        return time_steps[current_index].timestamp;
    return 0;
}

} // namespace wal
