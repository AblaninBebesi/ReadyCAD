# rc_set_warnings(<target>)
# Strict warnings for OUR code only. Third-party targets are compiled with their own flags.
function(rc_set_warnings target)
    if(MSVC)
        set(warnings /W4 /permissive- /utf-8)
        if(READYCAD_WARNINGS_AS_ERRORS)
            list(APPEND warnings /WX)
        endif()
    else()
        set(warnings
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Woverloaded-virtual
            -Wold-style-cast
            -Wcast-align
            -Wnull-dereference
            -Wdouble-promotion
            -Wimplicit-fallthrough)
        if(READYCAD_WARNINGS_AS_ERRORS)
            list(APPEND warnings -Werror)
        endif()
    endif()

    get_target_property(type ${target} TYPE)
    if(type STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()
    target_compile_options(${target} PRIVATE ${warnings})
endfunction()
