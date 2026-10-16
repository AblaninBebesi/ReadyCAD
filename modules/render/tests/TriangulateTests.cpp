// Ear clipping of the faces of shaped panels.

#include "readycad/render/Triangulate.hpp"

#include "readycad/core/PanelShape.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace rc;
using core::Vec2;
using Catch::Approx;

namespace {

double area(const std::vector<render::Triangle2>& triangles)
{
    double total = 0.0;
    for (const auto& t : triangles) {
        const double twice = (t[1].x - t[0].x) * (t[2].y - t[0].y) - (t[1].y - t[0].y) * (t[2].x - t[0].x);
        CHECK(twice > 0.0); // counter-clockwise, no slivers of negative area
        total += twice / 2.0;
    }
    return total;
}

} // namespace

TEST_CASE("A rectangle becomes two triangles", "[render][triangulate]")
{
    const auto t = render::triangulate({{0, 0}, {800, 0}, {800, 560}, {0, 560}}, {});
    CHECK(t.size() == 2);
    CHECK(area(t) == Approx(800.0 * 560.0));
    // Clockwise input works the same.
    CHECK(area(render::triangulate({{0, 0}, {0, 560}, {800, 560}, {800, 0}}, {})) == Approx(800.0 * 560.0));
}

TEST_CASE("Notched outlines: n - 2 triangles covering the area", "[render][triangulate]")
{
    const auto outline = core::outlinePoints(core::panelOutline(
        2000.0, 560.0,
        {core::CornerNotch{core::PanelCorner::L1W1, 100.0, 15.0},
         core::EdgeNotch{core::EdgeSide::L2, false, 600.0, 120.0, 80.0},
         core::CornerRound{core::PanelCorner::L2W2, 50.0}}));
    const auto t = render::triangulate(outline, {});
    CHECK(t.size() == outline.size() - 2);
    const double pi = 3.14159265358979;
    CHECK(area(t) == Approx(2000.0 * 560.0 - 100.0 * 15.0 - 120.0 * 80.0 - (1.0 - pi / 4.0) * 2500.0)
                         .epsilon(1e-3));
}

TEST_CASE("Faces with several openings: the openings stay open", "[render][triangulate]")
{
    const std::vector<Vec2> worktop{{0, 0}, {2400, 0}, {2400, 600}, {0, 600}};
    const auto sink = core::openingOutline({Vec2(700, 300), 560, 480, 15});
    const auto hob = core::openingOutline({Vec2(1700, 295), 560, 490, 5});
    const auto cable = core::openingOutline({Vec2(2200, 500), 60, 60, 30});
    const auto t = render::triangulate(worktop, {sink, cable, hob});
    const double pi = 3.14159265358979;
    const double expected = 2400.0 * 600.0 - (560.0 * 480.0 - (4.0 - pi) * 225.0)
                          - (560.0 * 490.0 - (4.0 - pi) * 25.0) - pi * 900.0;
    CHECK(area(t) == Approx(expected).epsilon(1e-3));
    // No triangle covers the middle of an opening.
    for (const auto& tri : t) {
        for (const Vec2 p : {Vec2(700, 300), Vec2(1700, 295), Vec2(2200, 500)}) {
            const auto side = [&](Vec2 a, Vec2 b) { return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x); };
            CHECK_FALSE((side(tri[0], tri[1]) > 0 && side(tri[1], tri[2]) > 0 && side(tri[2], tri[0]) > 0));
        }
    }
}
