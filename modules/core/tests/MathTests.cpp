#include "readycad/core/Math.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace rc::core;

TEST_CASE("nearlyEqual uses the geometric tolerance", "[core][math]")
{
    CHECK(nearlyEqual(18.0, 18.0 + kEpsilon / 2));
    CHECK_FALSE(nearlyEqual(18.0, 18.001));
    CHECK(nearlyEqual(18.0, 18.001, 0.01));
    CHECK(nearlyEqual(Vec3(1, 2, 3), Vec3(1, 2, 3 + kEpsilon / 2)));
    CHECK_FALSE(nearlyEqual(Vec3(1, 2, 3), Vec3(1, 2.1, 3)));
}

TEST_CASE("double keeps sub-micron precision on furniture-sized values", "[core][math]")
{
    // 1000 repeated +0.1 mm / -0.1 mm edits on a 2800 mm panel must not drift.
    Real height = 2800.0;
    for (int i = 0; i < 1000; ++i) {
        height += 0.1;
        height -= 0.1;
    }
    CHECK(nearlyEqual(height, 2800.0));
}

TEST_CASE("roundToStep snaps to a grid", "[core][math]")
{
    CHECK(roundToStep(37.4, 32.0) == 32.0);
    CHECK(roundToStep(48.1, 32.0) == 64.0);
    CHECK(roundToStep(17.74, 0.5) == 17.5);
}
