// Stage 7: polygons, rectangles and quarter-turn transforms of the nesting module.

#include "LayoutCheck.hpp"
#include "readycad/nesting/Geometry.hpp"
#include "readycad/nesting/Nfp.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace rc::nesting;
using Catch::Approx;

TEST_CASE("Polygon area, orientation and bounds", "[nesting][geometry]")
{
    const Polygon ccw{{0, 0}, {10, 0}, {10, 5}, {0, 5}};
    const Polygon cw{{0, 0}, {0, 5}, {10, 5}, {10, 0}};
    CHECK(signedArea(ccw) == Approx(50.0));
    CHECK(signedArea(cw) == Approx(-50.0));
    CHECK(area(cw) == Approx(50.0));
    CHECK(signedArea(normalized(cw)) == Approx(50.0));

    const Rect b = bounds(Polygon{{-2, 3}, {4, -1}, {1, 7}});
    CHECK(b.min == Vec2(-2, -1));
    CHECK(b.max == Vec2(4, 7));
    CHECK(b.area() == Approx(48.0));

    CHECK_THROWS_AS(normalized(Polygon{{0, 0}, {1, 1}}), std::invalid_argument);
    CHECK_THROWS_AS(normalized(Polygon{{0, 0}, {1, 1}, {2, 2}}), std::invalid_argument);
}

TEST_CASE("Rectangles are recognised, other shapes are not", "[nesting][geometry]")
{
    CHECK(isRectangle(rectangle(Rect{{-1, -1}, {762, 518}})));
    CHECK(isRectangle(Polygon{{0, 0}, {5, 0}, {10, 0}, {10, 5}, {0, 5}})); // extra point on a side
    CHECK_FALSE(isRectangle(Polygon{{0, 0}, {10, 0}, {10, 5}, {5, 5}, {5, 10}, {0, 10}})); // L
    CHECK_FALSE(isRectangle(Polygon{{0, 0}, {10, 0}, {5, 8}}));
    CHECK_FALSE(isRectangle(Polygon{{0, 0}, {10, 1}, {10, 6}, {0, 5}})); // parallelogram
}

TEST_CASE("Quarter turns are exact rotations", "[nesting][geometry]")
{
    const Vec2 p{3, 1};
    CHECK(rotateQuarter(p, 0) == p);
    CHECK(rotateQuarter(p, 1) == Vec2(-1, 3));
    CHECK(rotateQuarter(p, 2) == Vec2(-3, -1));
    CHECK(rotateQuarter(p, 3) == Vec2(1, -3));
    CHECK(rotateQuarter(p, 4) == p);
    CHECK(rotateQuarter(p, -1) == rotateQuarter(p, 3));

    const Transform t{1, {100, 20}};
    CHECK(t.apply({10, 0}) == Vec2(100, 30));
    CHECK(t.degrees() == Approx(90.0));

    // A 762 x 518 blank turned by 90° on the sheet: 518 wide, 762 high.
    const Rect turned = transformed(Rect{{0, 0}, {762, 518}}, Transform{1, {600, 0}});
    CHECK(turned.min == Vec2(82, 0));
    CHECK(turned.max == Vec2(600, 762));
    CHECK(area(transformed(rectangle(Rect{{0, 0}, {762, 518}}), t)) == Approx(762.0 * 518.0));
}

TEST_CASE("Labels of shaped parts go inside the part", "[nesting][geometry]")
{
    const Polygon box = rectangle(Rect{{0, 0}, {762, 518}});
    CHECK(labelArea(box) == bounds(box));

    const Polygon l{{0, 0}, {600, 0}, {600, 250}, {250, 250}, {250, 600}, {0, 600}};
    const Polygon u{{0, 0}, {800, 0}, {800, 500}, {600, 500}, {600, 200}, {200, 200}, {200, 500},
                    {0, 500}};
    for (const Polygon& shape : {l, u}) {
        const Rect area = labelArea(shape);
        CHECK(area.width() > 100.0);
        // All four corners and the middle are inside the part.
        for (const Vec2 p : {area.min, area.max, Vec2(area.min.x, area.max.y),
                             Vec2(area.max.x, area.min.y), (area.min + area.max) * 0.5}) {
            CHECK(test::inside(p, shape));
        }
    }
    // The middle of the U's box is in its notch, the label is not.
    const Rect area = labelArea(u);
    const Vec2 middle = (area.min + area.max) * 0.5;
    CHECK_FALSE((middle.x > 200.0 && middle.x < 600.0 && middle.y > 200.0));
}

TEST_CASE("Inner-fit rectangle of a part in a sheet", "[nesting][nfp]")
{
    const Rect sheet{{10, 10}, {2790, 2090}};
    const Polygon part = rectangle(Rect{{-1, -1}, {761, 517}});
    const auto fit = innerFitRectangle(sheet, part);
    REQUIRE(fit);
    CHECK(fit->min == Vec2(11, 11));
    CHECK(fit->max == Vec2(2790 - 761, 2090 - 517));

    // Exactly as wide as the sheet: one x only.
    const auto exact = innerFitRectangle(sheet, rectangle(Rect{{0, 0}, {2780, 100}}));
    REQUIRE(exact);
    CHECK(exact->width() == Approx(0.0));
    CHECK(exact->min.x == Approx(10.0));

    CHECK_FALSE(innerFitRectangle(sheet, rectangle(Rect{{0, 0}, {2781, 100}})));
}
