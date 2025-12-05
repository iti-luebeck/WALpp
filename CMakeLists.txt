cmake_minimum_required(VERSION 3.20)
project(walpp 
    VERSION 1.0.0 
    LANGUAGES CXX
    DESCRIPTION "WAL - Waveform Analysis Language in C++20"
)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "Setting build type to '${CMAKE_BUILD_TYPE}' as none was specified.")
endif()

# Options
option(BUILD_PYTHON_MODULE "Build Python module bindings" ON)
option(ENABLE_TESTS "Build test suite" ON)
option(ENABLE_COVERAGE "Enable code coverage" OFF)
option(ENABLE_FST_SUPPORT "Enable FST waveform format support" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# Add cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ============================================================================
# Dependencies
# ============================================================================

# Lark parser - might be header-only or need custom implementation
# For now, we'll assume it needs to be found or provided
find_package(Lark QUIET)

# Python and pybind11 (required for Python bindings)
if(BUILD_PYTHON_MODULE)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    find_package(pybind11 CONFIG REQUIRED)
    message(STATUS "Python found: ${Python3_EXECUTABLE}")
    message(STATUS "pybind11 found: ${pybind11_INCLUDE_DIR}")
endif()

# Optional: Find pylibfst for FST support
if(ENABLE_FST_SUPPORT)
    find_package(Pylibfst QUIET)
    if(Pylibfst_FOUND)
        message(STATUS "pylibfst found - FST support enabled")
    else()
        message(STATUS "pylibfst not found - FST support disabled (optional)")
    endif()
endif()

# Optional: Find zlib for compression
find_package(ZLIB QUIET)

# ============================================================================
# Main Library
# ============================================================================

# Core WAL library sources - Phase 1 + Phase 2 + Phase 3 + Phase 4
set(WALPP_CORE_SOURCES
    src/ast.cpp
    src/evaluator.cpp
    src/parser.cpp
    src/core.cpp
    src/operator_registry.cpp
    
    # Trace system
    src/trace/trace.cpp
    src/trace/trace_container.cpp
    src/trace/vcd_trace.cpp
    src/trace/csv_trace.cpp
    src/trace/fst_trace.cpp
    
    # Streaming Implementation - enabled with completed implementations
    src/trace/streaming_chunk_reader.cpp
    src/trace/streaming_vcd_trace.cpp
    src/trace/streaming_csv_trace.cpp
    src/trace/streaming_fst_trace.cpp
    
    # Phase 3 Writers
    src/writers/trace_writer.cpp
    src/writers/vcd_writer.cpp
    src/writers/fst_writer.cpp
    src/writers/csv_writer.cpp
    src/writers/writers.cpp
    
    # Fast VCD implementation for HD/HW operators
    src/fast/vcd_parser.cpp
    src/fast/hamming_ops.cpp
    
    # Phase 2-5: All operators are header-only (implementations in .h files)
)

# Create main library
add_library(walpp ${WALPP_CORE_SOURCES})

# Include directories
target_include_directories(walpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compiler flags
if(MSVC)
    target_compile_options(walpp PRIVATE /W4 /WX)
else()
    target_compile_options(walpp PRIVATE -Wall -Wextra -Wpedantic)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(walpp PRIVATE -Wduplicated-cond -Wduplicated-branches)
    endif()
endif()

# Link Lark if found
if(Lark_FOUND)
    target_link_libraries(walpp PUBLIC Lark::lark)
else()
    message(WARNING "Lark not found - you may need to implement parser manually")
endif()

# Link pylibfst if found
if(Pylibfst_FOUND)
    target_link_libraries(walpp PUBLIC Pylibfst::Pylibfst)
    target_compile_definitions(walpp PRIVATE HAVE_FST_SUPPORT)
endif()

# Link zlib if found
if(ZLIB_FOUND)
    target_link_libraries(walpp PRIVATE ZLIB::ZLIB)
    target_compile_definitions(walpp PRIVATE HAVE_ZLIB)
endif()

# ============================================================================
# Readline support for CLI/REPL
# ============================================================================

find_package(readline QUIET)
if(NOT readline_FOUND)
    # Try finding readline manually
    find_path(READLINE_INCLUDE_DIR readline/readline.h)
    find_library(READLINE_LIBRARY readline)
    if(READLINE_INCLUDE_DIR AND READLINE_LIBRARY)
        set(HAVE_READLINE TRUE)
    endif()
endif()

# ============================================================================
# CLI Executable with REPL support
# ============================================================================

set(CLI_SOURCES
    src/main.cpp
    src/cli.cpp
    src/repl.cpp
)

add_executable(wal ${CLI_SOURCES})
target_link_libraries(wal PRIVATE walpp)

# Link readline if available
if(readline_FOUND)
    target_link_libraries(wal PRIVATE readline::readline)
elseif(HAVE_READLINE)
    target_include_directories(wal PRIVATE ${READLINE_INCLUDE_DIR})
    target_link_libraries(wal PRIVATE ${READLINE_LIBRARY})
endif()



if(BUILD_PYTHON_MODULE)
    pybind11_add_module(wal_cpp
        src/python/python_module.cpp
        src/python/python_types.cpp
        src/python/python_value.cpp
    )
    
    target_link_libraries(wal_cpp PRIVATE walpp)
    
    target_include_directories(wal_cpp
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    # Set output directory for Python module
    if(WIN32)
        set_target_properties(wal_cpp PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
        )
    else()
        set_target_properties(wal_cpp PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
        )
    endif()
endif()

# ============================================================================
# Tests
# ============================================================================

if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Code Coverage
# ============================================================================

if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(walpp PRIVATE --coverage -O0 -g)
    target_link_options(walpp PRIVATE --coverage)
    
    find_program(GCOV_EXECUTABLE gcov)
    if(GCOV_EXECUTABLE)
        message(STATUS "gcov found: ${GCOV_EXECUTABLE}")
    endif()
endif()

# ============================================================================
# Installation
# ============================================================================

# Install targets
install(TARGETS walpp
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(TARGETS wal
    RUNTIME DESTINATION bin
)

if(BUILD_PYTHON_MODULE)
    install(TARGETS wal_cpp
        LIBRARY DESTINATION lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/site-packages
    )
endif()

# Install headers
install(DIRECTORY include/wal
    DESTINATION include
)

# Install library files
install(DIRECTORY lib/std/
    DESTINATION share/wal/lib/std
)

# Install examples
install(DIRECTORY examples/
    DESTINATION share/wal/examples
    OPTIONAL
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "WAL++ Configuration Summary:")
message(STATUS "========================================")
message(STATUS "  C++ Standard:         ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:           ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build Shared Libs:    ${BUILD_SHARED_LIBS}")
message(STATUS "  Build Python Module:  ${BUILD_PYTHON_MODULE}")
message(STATUS "  Enable Tests:         ${ENABLE_TESTS}")
message(STATUS "  Enable Coverage:      ${ENABLE_COVERAGE}")
message(STATUS "  FST Support:          ${Pylibfst_FOUND}")
message(STATUS "  Lark Parser Found:    ${Lark_FOUND}")
message(STATUS "  Zlib Support:         ${ZLIB_FOUND}")
message(STATUS "")
message(STATUS "  Install Prefix:       ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Build with: cmake --build . -j$(nproc)")
message(STATUS "Install with: cmake --install .")
message(STATUS "")
