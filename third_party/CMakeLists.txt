# ==============================================================================
# Third-party dependencies via FetchContent
# ==============================================================================
include(FetchContent)

# Avoid re-downloading if the user has already populated a dependency.
set(FETCHCONTENT_QUIET ON)

# ------------------------------------------------------------------------------
# spdlog - fast C++ logging library
# ------------------------------------------------------------------------------
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.1
    GIT_SHALLOW    TRUE
)

# ------------------------------------------------------------------------------
# nlohmann/json - JSON for Modern C++
# ------------------------------------------------------------------------------
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

# Populate non-GUI dependencies.
FetchContent_MakeAvailable(spdlog nlohmann_json)

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
