#include "wal/fast/streaming_ops.h"
#include <iostream>

int main(int argc, char** argv) {
    fast_vcd::VCDParser parser;
    fast_vcd::StreamingHamming kernel(
        parser,
        fast_vcd::TrackedSignals::in_scope(argv[2]),
        /* distance = */ true,
        [](int64_t ts, int64_t value) {
            std::cout << ts << "," << value << "\n";
        });

    parser.set_delta_sink([&](const fast_vcd::DeltaTimestamp& d) { kernel.on_delta(d); });
    parser.parse(argv[1]);
    kernel.finish();
    std::cerr << "rows: " << kernel.rows() << "\n";
    return 0;
}
