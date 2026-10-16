rc_add_module(render
    SOURCES
        src/Camera.cpp
        src/GlObjects.cpp
        src/Image.cpp
        src/Renderer.cpp
        src/Scene.cpp
        src/Triangulate.cpp
    DEPENDS
        readycad::core
        readycad::detailing
        glad::glad
        OpenGL::GL
    PRIVATE_DEPENDS
        stb::image
    TESTS
        tests/CameraTests.cpp
        tests/SceneTests.cpp
        tests/TriangulateTests.cpp
        tests/ImageTests.cpp)

if(TARGET readycad_render_tests)
    target_compile_definitions(readycad_render_tests PRIVATE
        READYCAD_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets")
endif()
