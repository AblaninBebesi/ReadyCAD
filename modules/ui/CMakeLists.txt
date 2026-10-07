rc_add_module(ui
    SOURCES
        src/Application.cpp
        src/EditorPanels.cpp
        src/FileDialogs.cpp
        src/Fonts.cpp
    DEPENDS
        readycad::editor
        readycad::i18n
        readycad::render
        imgui::imgui
    PRIVATE_DEPENDS
        glfw
        nfd
        $<$<PLATFORM_ID:Windows>:shell32>)

# GLFW must not pull in its own GL header — glad provides it.
target_compile_definitions(readycad_ui PRIVATE GLFW_INCLUDE_NONE)
