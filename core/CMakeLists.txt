# ==============================================================================
# w3x_core - foundational utilities (logging, config, filesystem, error, platform)
# ==============================================================================

# Collect all .cc source files under core/.
file(GLOB_RECURSE W3X_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

add_library(w3x_core STATIC ${W3X_CORE_SOURCES})

# Headers are included as "core/logger/logger.h" etc., so the include root is
# the project source directory (one level up from this CMakeLists.txt).
target_include_directories(w3x_core
    PUBLIC
        "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(w3x_core
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
    PRIVATE
        w3x_warnings
)
