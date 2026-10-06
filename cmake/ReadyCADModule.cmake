# rc_add_module(<name>
#     [SOURCES   <files...>]   # .cpp files; without them the module is header-only/placeholder
#     [DEPENDS   <targets...>] # public dependencies (visible to users of this module)
#     [PRIVATE_DEPENDS <targets...>]
#     [TESTS     <files...>])  # Catch2 test sources -> readycad_<name>_tests executable
#
# Creates target `readycad_<name>` with alias `readycad::<name>`.
# Layout convention for every module:
#   modules/<name>/include/readycad/<name>/*.hpp   public API   (#include "readycad/<name>/X.hpp")
#   modules/<name>/src/*.cpp                       implementation (private)
#   modules/<name>/tests/*Tests.cpp                unit tests
function(rc_add_module name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS;PRIVATE_DEPENDS;TESTS" ${ARGN})

    set(target readycad_${name})
    set(include_dir ${CMAKE_CURRENT_SOURCE_DIR}/include)

    if(ARG_SOURCES)
        add_library(${target} STATIC ${ARG_SOURCES})
        if(EXISTS ${include_dir})
            target_include_directories(${target} PUBLIC ${include_dir})
        endif()
        target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(${target}
            PUBLIC  ${ARG_DEPENDS}
            PRIVATE ${ARG_PRIVATE_DEPENDS})
        rc_set_warnings(${target})
    else()
        add_library(${target} INTERFACE)
        if(EXISTS ${include_dir})
            target_include_directories(${target} INTERFACE ${include_dir})
        endif()
        target_link_libraries(${target} INTERFACE ${ARG_DEPENDS})
    endif()
    add_library(readycad::${name} ALIAS ${target})

    if(READYCAD_BUILD_TESTS AND ARG_TESTS)
        rc_add_test_executable(${target}_tests
            SOURCES ${ARG_TESTS}
            DEPENDS ${target})
    endif()
endfunction()

# rc_add_test_executable(<name> SOURCES <files...> DEPENDS <targets...>)
# A Catch2 test runner whose test cases are registered individually in CTest.
function(rc_add_test_executable name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    # Shared test helpers (matchers etc.): #include "readycad/test/TestHelpers.hpp"
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/tests/support)
    target_link_libraries(${name} PRIVATE ${ARG_DEPENDS} Catch2::Catch2WithMain)
    rc_set_warnings(${name})
    catch_discover_tests(${name} DISCOVERY_MODE PRE_TEST)
endfunction()
