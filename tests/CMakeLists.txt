# Cross-module tests. Unit tests live next to their module (modules/<name>/tests).
# Stage 9: the end-to-end user journey and the robustness tests.
rc_add_test_executable(readycad_integration_tests
    SOURCES
        integration/CncPipelineTests.cpp
        integration/CuttingPipelineTests.cpp
        integration/ExportPipelineTests.cpp
        integration/ModuleGraphTests.cpp
        integration/RobustnessTests.cpp
        integration/UserJourneyTests.cpp
    DEPENDS
        readycad::production)
