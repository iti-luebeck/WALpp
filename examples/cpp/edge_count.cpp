#include "wal/core.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: edge_count <trace> <signal>\n";
        return 1;
    }
    wal::Wal w;
    w.load(argv[1], "t");
    std::string query = "(count (rising " + std::string(argv[2]) + "))";
    wal::Value result = w.eval_str(query);
    std::cout << argv[2] << ": " << result.to_int() << " rising edges\n";
    std::cout << "trace has " << w.eval_str("MAX-INDEX").to_int() + 1 << " indices\n";
    return 0;
}
