# Cross-module tests. Unit tests live next to their module (modules/<name>/tests).
# Stage 9 adds the end-to-end "user journey" test here.
rc_add_test_executable(readycad_integration_tests
    SOURCES
        integration/CuttingPipelineTests.cpp
        integration/ExportPipelineTests.cpp
        integration/ModuleGraphTests.cpp
    DEPENDS
        readycad::editor)
