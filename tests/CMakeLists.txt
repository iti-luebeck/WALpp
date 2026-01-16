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
    COMMAND $<TARGET_FILE:test_wal_executable>
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

