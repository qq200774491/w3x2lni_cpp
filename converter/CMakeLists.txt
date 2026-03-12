# ==============================================================================
# w3x_converter - format converters (W3X-to-LNI, resource extraction)
# ==============================================================================

file(GLOB_RECURSE W3X_CONVERTER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

list(FILTER W3X_CONVERTER_SOURCES EXCLUDE REGEX ".*/jass2lua/.*\\.cc$")

add_library(w3x_converter STATIC ${W3X_CONVERTER_SOURCES})

target_include_directories(w3x_converter
    PUBLIC
        "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(w3x_converter
    PUBLIC
        w3x_core
        w3x_parser
    PRIVATE
        w3x_warnings
)
