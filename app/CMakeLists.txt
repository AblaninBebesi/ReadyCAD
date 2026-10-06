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
