# ==============================================================================
# Third-party dependencies via FetchContent
# ==============================================================================
include(FetchContent)

# Avoid re-downloading if the user has already populated a dependency.
set(FETCHCONTENT_QUIET ON)

find_package(spdlog CONFIG QUIET)
if(NOT spdlog_FOUND)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.15.1
        GIT_SHALLOW    TRUE
    )
endif()

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
    )
endif()

if(NOT spdlog_FOUND)
    FetchContent_MakeAvailable(spdlog)
endif()

if(NOT nlohmann_json_FOUND)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

set(STORM_SKIP_INSTALL ON CACHE BOOL "" FORCE)
set(STORM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(STORM_USE_BUNDLED_LIBRARIES ON CACHE BOOL "" FORCE)
set(STORM_UNICODE ON CACHE BOOL "" FORCE)
set(W3X_LOCAL_STORMLIB_SOURCE
    "${CMAKE_SOURCE_DIR}/build/stormlib_src/StormLib-master")
if(EXISTS "${W3X_LOCAL_STORMLIB_SOURCE}/CMakeLists.txt")
    add_subdirectory(
        "${W3X_LOCAL_STORMLIB_SOURCE}"
        "${CMAKE_BINARY_DIR}/_deps/stormlib-build"
    )
else()
    FetchContent_Declare(
        stormlib
        GIT_REPOSITORY https://github.com/ladislav-zezula/StormLib.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(stormlib)
endif()

# ------------------------------------------------------------------------------
# GUI-only dependencies
# ------------------------------------------------------------------------------
if(W3X_BUILD_GUI)
    # GLFW - cross-platform windowing
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(glfw)

    # Dear ImGui - immediate-mode GUI (no CMakeLists.txt upstream, so we build
    # a small static library ourselves).
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.8-docking
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(imgui)

    # Build Dear ImGui as a static library with the GLFW + OpenGL3 backends.
    add_library(imgui_lib STATIC
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_demo.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
    )

    target_include_directories(imgui_lib
        PUBLIC
            "${imgui_SOURCE_DIR}"
            "${imgui_SOURCE_DIR}/backends"
    )

    target_link_libraries(imgui_lib PUBLIC glfw)

    # Platform-specific OpenGL linkage.
    find_package(OpenGL REQUIRED)
    target_link_libraries(imgui_lib PUBLIC OpenGL::GL)
endif()
