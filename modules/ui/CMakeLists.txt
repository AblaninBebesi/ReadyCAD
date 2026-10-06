rc_add_module(ui
    SOURCES
        src/Application.cpp
        src/Fonts.cpp
        src/Localization.cpp
    DEPENDS
        readycad::editor
        readycad::render
        imgui::imgui
    PRIVATE_DEPENDS
        glfw
    TESTS
        tests/LocalizationTests.cpp)

# GLFW must not pull in its own GL header — glad provides it.
target_compile_definitions(readycad_ui PRIVATE GLFW_INCLUDE_NONE)
