# ==============================================================================
# w3x_parser - file-format parsers (SLK, W3I, W3U, W3X, JASS)
# ==============================================================================

file(GLOB_RECURSE W3X_PARSER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

add_library(w3x_parser STATIC ${W3X_PARSER_SOURCES})

target_include_directories(w3x_parser
    PUBLIC
        "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(w3x_parser
    PUBLIC
        w3x_core
    PRIVATE
        w3x_warnings
)
