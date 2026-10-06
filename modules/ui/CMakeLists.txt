rc_add_module(ui
    SOURCES
        src/Application.cpp
        src/EditorPanels.cpp
        src/FileDialogs.cpp
        src/Fonts.cpp
        src/Labels.cpp
        src/Localization.cpp
    DEPENDS
        readycad::editor
        readycad::render
        imgui::imgui
    PRIVATE_DEPENDS
        glfw
        nfd
    TESTS
        tests/LocalizationTests.cpp
        tests/LabelsTests.cpp)

# GLFW must not pull in its own GL header — glad provides it.
target_compile_definitions(readycad_ui PRIVATE GLFW_INCLUDE_NONE)
