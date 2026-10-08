// Stage 7: No-Fit Polygons. The NFP must say "overlap" exactly where two parts overlap — checked
// against an independent polygon test at many positions, for convex and concave shapes.

#include "LayoutCheck.hpp"
#include "readycad/nesting/Nfp.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace rc::nesting;
using Catch::Approx;

namespace {

/// Distance from p to the outline of a set of polygons.
Real boundaryDistance(Vec2 p, const std::vector<Polygon>& polygons)
{
    Real d = 1e300;
    for (const Polygon& poly : polygons) {
        for (std::size_t i = 0; i < poly.size(); ++i) {
            d = std::min(d, test::pointSegmentDistance(p, poly[i], poly[(i + 1) % poly.size()]));
        }
    }
    return d;
}

bool insideAny(Vec2 p, const std::vector<Polygon>& polygons)
{
    for (const Polygon& poly : polygons) {
        if (test::inside(p, poly)) {
            return true;
        }
    }
    return false;
}

/// Moves `moving` over a grid around `fixed`; wherever the NFP says "inside", the parts must
/// overlap, wherever it says "outside", they must not. Points closer than 0.5 mm to the NFP's
/// outline (touching) are skipped.
void checkNfp(const Polygon& fixed, const Polygon& moving, const std::string& name)
{
    INFO(name);
    const std::vector<Polygon> nfp = noFitPolygon(fixed, moving);
    REQUIRE_FALSE(nfp.empty());
    const Rect f = bounds(fixed);
    const Rect m = bounds(moving);
    int inside = 0;
    int outside = 0;
    for (Real x = f.min.x - m.max.x - 20.37; x <= f.max.x - m.min.x + 20; x += 7.13) {
        for (Real y = f.min.y - m.max.y - 20.11; y <= f.max.y - m.min.y + 20; y += 6.91) {
            const Vec2 t{x, y};
            if (boundaryDistance(t, nfp) < 0.5) {
                continue;
            }
            const bool predicted = insideAny(t, nfp);
            const bool real = test::overlap(fixed, transformed(moving, Transform{0, t}));
            INFO("position " << x << ", " << y);
            REQUIRE(predicted == real);
            (predicted ? inside : outside) += 1;
        }
    }
    CHECK(inside > 20);
    CHECK(outside > 20);
}

} // namespace

TEST_CASE("NFP of two rectangles is a rectangle", "[nesting][nfp]")
{
    const Polygon a = rectangle(Rect{{0, 0}, {100, 50}});
    const Polygon b = rectangle(Rect{{0, 0}, {30, 20}});
    const std::vector<Polygon> nfp = noFitPolygon(a, b);
    REQUIRE(nfp.size() == 1);
    const Rect box = bounds(nfp.front());
    CHECK(box.min == Vec2(-30, -20));
    CHECK(box.max == Vec2(100, 50));
    CHECK(area(nfp.front()) == Approx(130.0 * 70.0));
}

TEST_CASE("NFP matches the real overlap for convex and concave parts", "[nesting][nfp]")
{
    const Polygon square = rectangle(Rect{{0, 0}, {60, 60}});
    const Polygon triangle{{0, 0}, {80, 0}, {30, 50}};
    const Polygon lShape{{0, 0}, {120, 0}, {120, 40}, {40, 40}, {40, 120}, {0, 120}};
    const Polygon uShape{{0, 0}, {150, 0}, {150, 100}, {110, 100}, {110, 40},
                         {40, 40},  {40, 100}, {0, 100}};
    const Polygon hexagon{{20, 0}, {60, 0}, {80, 35}, {60, 70}, {20, 70}, {0, 35}};

    checkNfp(square, triangle, "square / triangle");
    checkNfp(triangle, square, "triangle / square");
    checkNfp(lShape, square, "L / square");
    checkNfp(uShape, rectangle(Rect{{0, 0}, {50, 40}}), "U / small block (fits the notch)");
    checkNfp(uShape, lShape, "U / L");
    checkNfp(hexagon, triangle, "hexagon / triangle");
}

TEST_CASE("A part fits into the notch of a U-shaped part", "[nesting][nfp]")
{
    // The notch is 70 wide and 60 deep: a 50 x 40 block inside it overlaps nothing.
    const Polygon uShape{{0, 0}, {150, 0}, {150, 100}, {110, 100}, {110, 40},
                         {40, 40},  {40, 100}, {0, 100}};
    const Polygon block = rectangle(Rect{{0, 0}, {50, 40}});
    const std::vector<Polygon> nfp = noFitPolygon(uShape, block);
    const Vec2 inNotch{50, 50};
    CHECK_FALSE(test::overlap(uShape, transformed(block, Transform{0, inNotch})));
    CHECK_FALSE(insideAny(inNotch, nfp));
}
