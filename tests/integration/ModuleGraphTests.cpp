// Smoke test for the build itself: `editor` sits on top of all GL-free modules, so if this
// executable links, the whole headless part of the dependency graph is wired correctly.

#include "readycad/core/Aabb.hpp"
#include "readycad/core/Version.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Headless modules link together", "[integration][build]")
{
    CHECK_FALSE(rc::core::versionString().empty());
    CHECK(rc::core::Aabb{}.isEmpty());
}
