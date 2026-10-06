configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInfo.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/BuildInfo.hpp
    @ONLY)

rc_add_module(core
    SOURCES
        src/Aabb.cpp
        src/Version.cpp
    DEPENDS
        glm::glm
    TESTS
        tests/AabbTests.cpp
        tests/MathTests.cpp
        tests/VersionTests.cpp)

target_include_directories(readycad_core PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
