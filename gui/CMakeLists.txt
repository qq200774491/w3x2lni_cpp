# ==============================================================================
# w3x_gui - Dear ImGui graphical front-end
#
# This file is only add_subdirectory'd when W3X_BUILD_GUI is ON.
# ==============================================================================

file(GLOB_RECURSE W3X_GUI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

add_library(w3x_gui STATIC ${W3X_GUI_SOURCES})

target_include_directories(w3x_gui
    PUBLIC
        "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(w3x_gui
    PUBLIC
        w3x_core
        w3x_parser
        w3x_converter
        imgui_lib
        glfw
    PRIVATE
        w3x_warnings
)

# Platform-specific OpenGL linkage.
find_package(OpenGL REQUIRED)
target_link_libraries(w3x_gui PUBLIC OpenGL::GL)
