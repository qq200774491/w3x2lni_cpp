add_executable(w3x_smoke_tests
    smoke_tests.cc
)

target_include_directories(w3x_smoke_tests
    PRIVATE
        "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(w3x_smoke_tests
    PRIVATE
        w3x_core
        w3x_converter
        w3x_cli
        w3x_warnings
)

add_test(NAME w3x_smoke_tests COMMAND w3x_smoke_tests)
