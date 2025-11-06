# Implementation Tests
# These tests verify operator implementations across all 8 phases

set(IMPL_TEST_FILES
    test_base_operators.cpp
    test_bitwise_operators.cpp
    test_string_operators.cpp
    test_aggregation_operators.cpp
    test_signal_operators.cpp
    test_spectrum_analysis.cpp
    test_statistical_analysis.cpp
    test_signal_processing.cpp
    test_data_export.cpp
    test_visualization.cpp
    test_trace_operators.cpp
)

# Create test executables
foreach(test_file ${IMPL_TEST_FILES})
    # Extract test name from filename (test_xxx.cpp -> test_xxx)
    string(REPLACE ".cpp" "" test_name ${test_file})
    
    add_executable(${test_name}
        ${test_file}
    )
    
    target_link_libraries(${test_name} PRIVATE walpp)
    
    # Add test to ctest
    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endforeach()

# Additional file format tests
set(FORMAT_TEST_FILES
    test_vcd.cpp
    test_csv.cpp
    test_fst.cpp
)

foreach(test_file ${FORMAT_TEST_FILES})
    string(REPLACE ".cpp" "" test_name ${test_file})
    
    add_executable(${test_name}
        ${test_file}
    )
    
    target_link_libraries(${test_name} PRIVATE walpp)
    
    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endforeach()

# CLI test (more complex, needs extra sources)
add_executable(test_cli
    test_cli.cpp
    ../../src/cli.cpp
    ../../src/repl.cpp
)

target_link_libraries(test_cli PRIVATE walpp)

# Link readline if available
if(readline_FOUND)
    target_link_libraries(test_cli PRIVATE readline::readline)
elseif(HAVE_READLINE)
    target_include_directories(test_cli PRIVATE ${READLINE_INCLUDE_DIR})
    target_link_libraries(test_cli PRIVATE ${READLINE_LIBRARY})
endif()

add_test(
    NAME test_cli
    COMMAND test_cli
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
