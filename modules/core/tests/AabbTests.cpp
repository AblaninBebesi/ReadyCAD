#include "readycad/core/Aabb.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using rc::core::Aabb;
using rc::core::Vec3;

TEST_CASE("Default Aabb is empty", "[core][aabb]")
{
    const Aabb box;
    CHECK(box.isEmpty());
    CHECK(box.volume() == 0.0);
    CHECK_FALSE(box.contains(Vec3(0.0)));
}

TEST_CASE("Aabb normalises corner order", "[core][aabb]")
{
    const Aabb box(Vec3(800, 2000, 560), Vec3(0, 0, 0));
    CHECK(box.min() == Vec3(0, 0, 0));
    CHECK(box.max() == Vec3(800, 2000, 560));
    CHECK(box.size() == Vec3(800, 2000, 560));
    CHECK(box.center() == Vec3(400, 1000, 280));
    CHECK(box.volume() == Catch::Approx(800.0 * 2000.0 * 560.0));
}

TEST_CASE("Aabb from origin and size", "[core][aabb]")
{
    // A left side wall of a wardrobe: 18 mm thick, 2000 mm high, 560 mm deep.
    const Aabb side = Aabb::fromOriginAndSize(Vec3(0, 0, 0), Vec3(18, 2000, 560));
    CHECK(side.max() == Vec3(18, 2000, 560));
    CHECK(side.contains(Vec3(9, 1000, 280)));
    CHECK(side.contains(Vec3(18, 2000, 560)));  // boundary counts as inside
    CHECK_FALSE(side.contains(Vec3(19, 1000, 280)));
}

TEST_CASE("Touching panels do not intersect, overlapping ones do", "[core][aabb]")
{
    const Aabb leftSide = Aabb::fromOriginAndSize(Vec3(0, 0, 0), Vec3(18, 2000, 560));

    SECTION("shelf placed right next to the side wall only touches it")
    {
        const Aabb shelf = Aabb::fromOriginAndSize(Vec3(18, 500, 0), Vec3(764, 18, 560));
        CHECK_FALSE(leftSide.intersects(shelf));
        CHECK_FALSE(shelf.intersects(leftSide));
    }

    SECTION("shelf that is 1 mm too wide overlaps the side wall")
    {
        const Aabb shelf = Aabb::fromOriginAndSize(Vec3(17, 500, 0), Vec3(765, 18, 560));
        CHECK(leftSide.intersects(shelf));
        CHECK(leftSide.intersection(shelf).size().x == Catch::Approx(1.0));
    }

    SECTION("an empty box never intersects")
    {
        CHECK_FALSE(leftSide.intersects(Aabb{}));
        CHECK(leftSide.intersection(Aabb{}).isEmpty());
    }
}

TEST_CASE("Aabb expand and translate", "[core][aabb]")
{
    Aabb box;
    box.expand(Vec3(10, 20, 30));
    CHECK_FALSE(box.isEmpty());
    CHECK(box.size() == Vec3(0.0));

    box.expand(Aabb(Vec3(-5, 0, 0), Vec3(0, 5, 5)));
    CHECK(box.min() == Vec3(-5, 0, 0));
    CHECK(box.max() == Vec3(10, 20, 30));

    const Aabb moved = box.translated(Vec3(100, 0, 0));
    CHECK(moved.min() == Vec3(95, 0, 0));
    CHECK(moved.size() == box.size());

    CHECK(moved.contains(Aabb(Vec3(96, 1, 1), Vec3(100, 2, 2))));
    CHECK_FALSE(moved.contains(Aabb(Vec3(0, 0, 0), Vec3(1, 1, 1))));
}
