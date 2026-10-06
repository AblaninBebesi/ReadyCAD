# Third-party dependencies, fetched at configure time.
# Versions are pinned so every machine (you, your teammate, the reviewer) builds the same thing.
# Downloads are cached in <build>/_deps; set FETCHCONTENT_BASE_DIR to share them between builds.

include(FetchContent)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW) # extracted file timestamps = extraction time
endif()

# --- glm: vector/matrix math (header-only) ---------------------------------
set(GLM_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glm
    URL https://github.com/g-truc/glm/archive/refs/tags/1.0.3.tar.gz
    SYSTEM)
FetchContent_MakeAvailable(glm)

# --- nlohmann/json: project and workshop-profile files ---------------------
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    SYSTEM)
FetchContent_MakeAvailable(nlohmann_json)

# --- Catch2: unit testing ---------------------------------------------------
if(READYCAD_BUILD_TESTS)
    FetchContent_Declare(Catch2
        URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.0.tar.gz
        SYSTEM)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch) # catch_discover_tests()
endif()

if(READYCAD_BUILD_APP)
    # --- GLFW: window, OpenGL context, input --------------------------------
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
        SYSTEM)
    FetchContent_MakeAvailable(glfw)

    # --- Dear ImGui (docking branch): immediate-mode GUI ---------------------
    # ImGui ships no CMakeLists, so we define the target ourselves.
    FetchContent_Declare(imgui
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b-docking.tar.gz)
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp)
    target_include_directories(imgui SYSTEM PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${imgui_SOURCE_DIR}/misc/cpp)
    target_link_libraries(imgui PUBLIC glfw)
    target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
    add_library(imgui::imgui ALIAS imgui)

    # --- Native file dialogs (Open / Save as) ------------------------------------
    set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(NFD_INSTALL     OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(nfd
        URL https://github.com/btzy/nativefiledialog-extended/archive/refs/tags/v1.4.1.tar.gz
        SYSTEM)
    FetchContent_MakeAvailable(nfd)

    find_package(OpenGL REQUIRED)
endif()
