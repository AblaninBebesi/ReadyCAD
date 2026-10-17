add_executable(ReadyCAD main.cpp)
target_link_libraries(ReadyCAD PRIVATE readycad::ui readycad::production)
rc_set_warnings(ReadyCAD)

# The program's own files (decor pictures) next to the executable, where it looks first.
add_custom_command(TARGET ReadyCAD POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:ReadyCAD>/assets
    COMMENT "Copying assets next to ReadyCAD")

if(WIN32)
    # The icon (app/resources/ReadyCAD.ico, 16-256 px) built into the executable.
    set(READYCAD_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/ReadyCAD.ico)
    configure_file(resources/ReadyCAD.rc.in ${CMAKE_CURRENT_BINARY_DIR}/ReadyCAD.rc @ONLY)
    target_sources(ReadyCAD PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/ReadyCAD.rc)
    # windres reports no dependencies: a new picture must still rebuild the resource.
    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/ReadyCAD.rc PROPERTIES
        OBJECT_DEPENDS ${READYCAD_ICON})

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

    # Stage 9, end to end with the real program and no window: a catalogue model, two copies,
    # cut on the nesting centre, the whole production package written. Then the package is
    # checked: its summary lists the part drawings, the sheet drawings and the CNC programs.
    set(batch_dir ${CMAKE_BINARY_DIR}/app_batch)
    add_test(NAME app_batch_clean COMMAND ${CMAKE_COMMAND} -E rm -rf ${batch_dir})
    add_test(NAME app_batch_package
        COMMAND ReadyCAD --catalog 1 --count 2 --nesting router
                --data-dir ${batch_dir}/atolye --package ${batch_dir}/paket)
    add_test(NAME app_batch_package_summary
        COMMAND ${CMAKE_COMMAND} -E cat ${batch_dir}/paket/OZET.txt)
    set_tests_properties(app_batch_clean PROPERTIES FIXTURES_SETUP app_batch_clean LABELS batch)
    set_tests_properties(app_batch_package PROPERTIES
        FIXTURES_REQUIRED app_batch_clean FIXTURES_SETUP app_batch_done LABELS batch TIMEOUT 300)
    set_tests_properties(app_batch_package_summary PROPERTIES
        FIXTURES_REQUIRED app_batch_done LABELS batch
        PASS_REGULAR_EXPRESSION "DXF/.*KESIM/.*CNC/")
endif()
