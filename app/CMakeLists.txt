add_executable(ReadyCAD main.cpp)
target_link_libraries(ReadyCAD PRIVATE readycad::ui)
rc_set_warnings(ReadyCAD)

if(WIN32)
    # Release builds: GUI subsystem (no console window). Debug keeps the console for logs.
    set_target_properties(ReadyCAD PROPERTIES WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>)
    if(MINGW)
        target_link_options(ReadyCAD PRIVATE $<$<NOT:$<CONFIG:Debug>>:-Wl,--entry=mainCRTStartup>)
    elseif(MSVC)
        target_link_options(ReadyCAD PRIVATE $<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>)
    endif()
endif()

if(READYCAD_BUILD_TESTS)
    # GUI smoke test: opens the real window, renders a model with holes, saves a screenshot and
    # exits. Needs a display with OpenGL 3.3, so it only exists when the app is built.
    add_test(NAME gui_smoke_screenshot
        COMMAND ReadyCAD --catalog 1 --holes --select Shelf
                --screenshot ${CMAKE_BINARY_DIR}/gui_smoke.bmp)
    set_tests_properties(gui_smoke_screenshot PROPERTIES LABELS gui TIMEOUT 60)
endif()
