# Tests configuration

# Find GoogleTest for unit tests
find_package(GTest QUIET)

# Google Test based unit tests
if(GTest_FOUND)
    # Core unit tests
    set(UNIT_TEST_SOURCES
        test_parser.cpp
        test_ast.cpp
        test_evaluator.cpp
    )

    # Create test executable
    add_executable(walpp_tests ${UNIT_TEST_SOURCES})

    # Link against WAL library and GoogleTest
    target_link_libraries(walpp_tests 
        PRIVATE 
            walpp 
            GTest::GTest 
            GTest::Main
    )

    # Include directories
    target_include_directories(walpp_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    # Register tests with CTest
    gtest_discover_tests(walpp_tests)
else()
    message(STATUS "GoogleTest not found - unit tests disabled (optional)")
endif()

# ============================================================================
# Phase 3 Writer Tests (Plain C++ with assert)
# ============================================================================

# VCD Writer tests
add_executable(test_vcd_writer test_vcd_writer.cpp)
target_link_libraries(test_vcd_writer PRIVATE walpp)
target_include_directories(test_vcd_writer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
add_test(NAME test_vcd_writer COMMAND test_vcd_writer WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# ============================================================================
# Phase 3 WAL Executable Integration Tests (VCD/FST/CSV processing)
# ============================================================================

add_executable(test_wal_executable test_wal_executable.cpp)
target_link_libraries(test_wal_executable PRIVATE walpp)
target_include_directories(test_wal_executable
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
add_test(
    NAME test_wal_executable
    COMMAND $<TARGET_FILE:test_wal_executable> $<TARGET_FILE:wal>
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# ============================================================================
# Phase 3 Streaming Implementation Tests (VCD/FST/CSV streaming, chunk reader)
# ============================================================================

add_executable(test_streaming_implementations test_streaming_implementations.cpp)
target_link_libraries(test_streaming_implementations PRIVATE walpp)
target_include_directories(test_streaming_implementations
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
add_test(NAME test_streaming_implementations COMMAND test_streaming_implementations WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# Implementation tests (operator phases and file format tests)
add_subdirectory(implementation)


# Round-trip: write a trace with each writer and read it back with WAL's own parser.
# The substring assertions in test_vcd_writer.cpp cannot detect an unreadable file; this
# can, because it asserts on decoded values.
add_executable(test_writer_roundtrip test_writer_roundtrip.cpp)
target_link_libraries(test_writer_roundtrip PRIVATE walpp)
target_include_directories(test_writer_roundtrip
    PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_writer_roundtrip COMMAND test_writer_roundtrip WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# ============================================================================
# Upstream WAL conformance (ics-jku/wal test suite, run against WAL++)
# ============================================================================
# Runs the vendored upstream test files in tests/original/ against the real
# WAL++ engine through its Python bindings. Before this, those files ran against
# a Python re-implementation in tests/original/wal/ and were not registered here
# at all, so they said nothing about WAL++.
#
# Failures are expected while operator parity is in progress, so the test is a
# ratchet rather than a pass/fail: it fails only if FEWER than MIN_PASS upstream
# tests pass. Raise MIN_PASS as parity work lands.
if(BUILD_PYTHON_MODULE)
    set(WALPP_CONFORMANCE_MIN_PASS 68 CACHE STRING
        "Minimum number of upstream WAL tests that must pass")
    add_test(
        NAME test_upstream_conformance
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/conformance/run_conformance.py
                --min-pass ${WALPP_CONFORMANCE_MIN_PASS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(test_upstream_conformance PROPERTIES
        ENVIRONMENT "WALPP_BUILD_DIR=$<TARGET_FILE_DIR:wal_cpp>")
endif()
