rc_add_module(ui
    SOURCES
        src/Application.cpp
        src/Assets.cpp
        src/CncPanels.cpp
        src/EditorPanels.cpp
        src/FileDialogs.cpp
        src/Fonts.cpp
        src/NestingPanels.cpp
    DEPENDS
        readycad::editor
        readycad::production
        readycad::i18n
        readycad::render
        imgui::imgui
    PRIVATE_DEPENDS
        glfw
        nfd
        $<$<PLATFORM_ID:Windows>:shell32>)

# GLFW must not pull in its own GL header — glad provides it.
target_compile_definitions(readycad_ui PRIVATE GLFW_INCLUDE_NONE)
# Where the program's own files are when it runs from its build folder without a copy of them.
target_compile_definitions(readycad_ui PRIVATE READYCAD_SOURCE_ASSETS="${PROJECT_SOURCE_DIR}/assets")
