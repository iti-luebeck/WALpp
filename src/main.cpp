#include <iostream>
#include "wal/cli.h"

int main(int argc, const char* argv[]) {
    try {
        wal::CLI cli(argc, argv);
        return cli.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

