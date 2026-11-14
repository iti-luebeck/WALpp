// A one-pass extraction tool, built the way --fast-hw-* is built.
#include "wal/fast/vcd_parser.h"
#include <cstdint>
#include <iostream>
#include <unordered_map>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: toggles <trace.vcd> <scope-prefix>\n";
        return 1;
    }

    fast_vcd::VCDParser parser;

    uint64_t timestamps = 0, toggles = 0;
    std::unordered_map<uint32_t, int64_t> state;
    bool ready = false;

    // The sink runs once per timestamp, during the parse. Nothing accumulates, so
    // memory is set by the number of signals followed, not by the trace length.
    parser.set_delta_sink([&](const fast_vcd::DeltaTimestamp& delta) {
        if (!ready) {
            for (uint32_t id : parser.get_signals_in_scope(argv[2])) {
                state[id] = 0;
            }
            for (const auto& [id, value] : parser.get_initial_values()) {
                if (state.count(id)) state[id] = value;
            }
            ready = true;
        }
        ++timestamps;
        for (const auto& [id, value] : delta.changes) {
            auto it = state.find(id);
            if (it != state.end()) {
                toggles += __builtin_popcountll(
                    static_cast<uint64_t>(it->second ^ value));
                it->second = value;
            }
        }
    });

    parser.parse(argv[1]);

    std::cout << "signals in scope: " << state.size() << "\n"
              << "timestamps:       " << timestamps << "\n"
              << "bit toggles:      " << toggles << "\n";
    return 0;
}
