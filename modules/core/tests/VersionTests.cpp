#include "readycad/core/Version.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("Version string matches the numeric version", "[core][version]")
{
    const auto v = rc::core::version();
    const std::string expected =
        std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch);

    CHECK(rc::core::versionString() == expected);
    CHECK(v.major >= 0);
}
