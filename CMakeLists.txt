cmake_minimum_required(VERSION 3.25)

project(ReadyCAD
    VERSION 0.1.0
    DESCRIPTION "Parametric case-furniture CAD/CAM (modular monolith)"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# All executables (app + test runners) land in one folder: easy to run, no DLL hunting.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(READYCAD_BUILD_APP   "Build the desktop application (GLFW + OpenGL + ImGui)" ON)
option(READYCAD_BUILD_TESTS "Build unit and integration tests (Catch2)"            ON)
option(READYCAD_WARNINGS_AS_ERRORS "Treat compiler warnings as errors"             OFF)

if(MINGW)
    # Static runtime: the .exe files run without libstdc++/libgcc DLLs on PATH.
    add_link_options(-static)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(CompilerWarnings)
include(Dependencies)
include(ReadyCADModule)

if(READYCAD_BUILD_TESTS)
    enable_testing()
endif()

# ---------------------------------------------------------------------------
# Modules. Order follows the dependency graph (see docs/ARCHITECTURE.md):
# a module may only depend on modules listed ABOVE it.
# ---------------------------------------------------------------------------
add_subdirectory(modules/core)       # geometry, furniture component tree, panels
add_subdirectory(modules/storage)    # project / workshop profile files (JSON)
add_subdirectory(modules/detailing)  # saw/finish sizes, edges, System 32 drilling
add_subdirectory(modules/nesting)    # stock, sheets, cutting/nesting optimisation
add_subdirectory(modules/export)     # DXF export, PDF documentation
add_subdirectory(modules/cam)        # machining operations + CNC postprocessors
add_subdirectory(modules/editor)     # document, commands, view models (GL-free)

if(READYCAD_BUILD_APP)
    add_subdirectory(third_party/glad)
    add_subdirectory(modules/render) # OpenGL renderer
    add_subdirectory(modules/ui)     # ImGui panels, main window
    add_subdirectory(app)            # executable entry point
endif()

if(READYCAD_BUILD_TESTS)
    add_subdirectory(tests)          # cross-module integration / end-to-end tests
endif()
