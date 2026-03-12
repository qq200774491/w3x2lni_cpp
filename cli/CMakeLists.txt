# ==============================================================================
# w3x_cli - command-line interface layer
# ==============================================================================

file(GLOB_RECURSE W3X_CLI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

add_library(w3x_cli STATIC ${W3X_CLI_SOURCES})

target_include_directories(w3x_cli
    PUBLIC
        "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(w3x_cli
    PUBLIC
        w3x_core
        w3x_parser
        w3x_converter
    PRIVATE
        w3x_warnings
)
