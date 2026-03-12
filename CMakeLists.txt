# ==============================================================================
# W3X Toolkit - Top-level CMake configuration
# ==============================================================================
cmake_minimum_required(VERSION 3.20)

project(w3x_toolkit
    VERSION 1.0.0
    DESCRIPTION "Pure C++ Warcraft III CLI toolchain"
    LANGUAGES CXX
)

# ==============================================================================
# Global C++ standard
# ==============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32 AND CMAKE_SIZEOF_VOID_P AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR
        "The Windows v1 build supports x64 only. Reconfigure with '-A x64'.")
endif()

# Place all binaries in predictable directories.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ==============================================================================
# Build options
# ==============================================================================
option(W3X_BUILD_GUI   "Build the Dear ImGui graphical front-end"  OFF)
option(W3X_BUILD_TESTS "Build unit tests"                          ON)

# ==============================================================================
# Third-party dependencies (FetchContent)
# ==============================================================================
add_subdirectory(third_party)

# ==============================================================================
# Internal libraries
# ==============================================================================
add_subdirectory(core)
add_subdirectory(parser)
add_subdirectory(converter)
add_subdirectory(cli)

if(W3X_BUILD_GUI)
    add_subdirectory(gui)
endif()

# ==============================================================================
# Main executable
# ==============================================================================
add_executable(w3x_tool app/main.cpp)

target_link_libraries(w3x_tool
    PRIVATE
        w3x_core
        w3x_parser
        w3x_converter
        w3x_cli
)

if(W3X_BUILD_GUI)
    target_link_libraries(w3x_tool PRIVATE w3x_gui)
    target_compile_definitions(w3x_tool PRIVATE W3X_BUILD_GUI=1)
endif()

# The project headers are included via paths relative to the project root
# (e.g. "core/logger/logger.h"), so expose the root directory.
target_include_directories(w3x_tool PRIVATE "${CMAKE_SOURCE_DIR}")

# ==============================================================================
# Compiler warnings (per-target via an interface library)
# ==============================================================================
add_library(w3x_warnings INTERFACE)

if(MSVC)
    target_compile_options(w3x_warnings INTERFACE
        /W4            # High warning level
        /permissive-   # Strict standard conformance
        /utf-8         # Source and execution character sets
    )
else()
    # GCC / Clang shared flags
    target_compile_options(w3x_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wnon-virtual-dtor
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(w3x_warnings INTERFACE
            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic
        )
    endif()
endif()

# Apply the warning flags to the main executable.
target_link_libraries(w3x_tool PRIVATE w3x_warnings)

# ==============================================================================
# Install rules
# ==============================================================================
include(GNUInstallDirs)

install(TARGETS w3x_tool
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(DIRECTORY data/
    DESTINATION "${CMAKE_INSTALL_BINDIR}/data"
)

# Install public headers preserving the directory structure.
install(DIRECTORY core/     DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/w3x_toolkit/core"     FILES_MATCHING PATTERN "*.h")
install(DIRECTORY parser/   DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/w3x_toolkit/parser"   FILES_MATCHING PATTERN "*.h")
install(DIRECTORY converter/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/w3x_toolkit/converter" FILES_MATCHING PATTERN "*.h")
install(DIRECTORY cli/      DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/w3x_toolkit/cli"      FILES_MATCHING PATTERN "*.h")

if(W3X_BUILD_GUI)
    install(DIRECTORY gui/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/w3x_toolkit/gui" FILES_MATCHING PATTERN "*.h")
endif()

if(W3X_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()
