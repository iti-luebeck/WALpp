# Standalone examples for the C++ API guides.
#
# These build against an INSTALLED WAL++, the way a consumer would, rather than as
# part of this tree. That is the point of them: if find_package(walpp) or the
# exported target ever breaks, these stop compiling.
#
#   cmake --install <walpp-build> --prefix /opt/walpp
#   cmake -S examples/cpp -B build -DCMAKE_PREFIX_PATH=/opt/walpp
#   cmake --build build
#
cmake_minimum_required(VERSION 3.20)
project(walpp_examples CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(walpp REQUIRED)

# Interpreter-level: load a trace, run a query (docs/guides/cpp-api.md)
add_executable(edge_count edge_count.cpp)
target_link_libraries(edge_count PRIVATE walpp::walpp)

# Interpreter-level: register an operator written in C++
add_executable(custom_operator custom_operator.cpp)
target_link_libraries(custom_operator PRIVATE walpp::walpp)

# Extraction-level: a one-pass tool (docs/guides/writing-fast-tools.md)
add_executable(toggles toggles.cpp)
target_link_libraries(toggles PRIVATE walpp::walpp)

# Extraction-level: reuse the kernels the CLI itself uses
add_executable(reuse_kernel reuse_kernel.cpp)
target_link_libraries(reuse_kernel PRIVATE walpp::walpp)
