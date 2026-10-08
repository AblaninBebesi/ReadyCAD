// Stage 8: short routes between holes (nearest neighbour + 2-opt).

#include "readycad/cam/Route.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <numeric>
#include <random>

using namespace rc::cam;
using rc::core::Real;
using rc::core::Vec2;

namespace {

Real cross(Vec2 a, Vec2 b)
{
    return a.x * b.y - a.y * b.x;
}

bool properlyCross(Vec2 a, Vec2 b, Vec2 c, Vec2 d)
{
    const Real d1 = cross(b - a, c - a);
    const Real d2 = cross(b - a, d - a);
    const Real d3 = cross(d - c, a - c);
    const Real d4 = cross(d - c, b - c);
    return ((d1 > 1e-9 && d2 < -1e-9) || (d1 < -1e-9 && d2 > 1e-9))
        && ((d3 > 1e-9 && d4 < -1e-9) || (d3 < -1e-9 && d4 > 1e-9));
}

} // namespace

TEST_CASE("Routes visit every point once", "[cam][route]")
{
    CHECK(shortRoute({}, Vec2(0.0)).empty());
    CHECK(shortRoute({{5, 5}}, Vec2(0.0)) == std::vector<std::size_t>{0});

    std::mt19937 random(5);
    std::uniform_real_distribution<Real> coordinate(0.0, 2000.0);
    std::vector<Vec2> points(60);
    for (Vec2& p : points) {
        p = {coordinate(random), coordinate(random)};
    }
    std::vector<std::size_t> order = shortRoute(points, Vec2(0.0));
    std::sort(order.begin(), order.end());
    std::vector<std::size_t> all(points.size());
    std::iota(all.begin(), all.end(), 0);
    CHECK(order == all);
}

TEST_CASE("A row of shelf support holes is drilled in one go", "[cam][route]")
{
    // A System 32 row, given in a scrambled order: the route must just walk along it.
    std::vector<Vec2> points;
    for (const int k : {7, 2, 9, 0, 5, 3, 8, 1, 6, 4}) {
        points.push_back({37.0, 100.0 + 32.0 * k});
    }
    const std::vector<std::size_t> order = shortRoute(points, Vec2(37.0, 0.0));
    CHECK(routeLength(points, order, Vec2(37.0, 0.0)) == 100.0 + 9 * 32.0);
}

TEST_CASE("Shortened routes are shorter and do not cross themselves", "[cam][route]")
{
    std::mt19937 random(11);
    std::uniform_real_distribution<Real> coordinate(0.0, 2800.0);
    for (int run = 0; run < 5; ++run) {
        std::vector<Vec2> points(80);
        for (Vec2& p : points) {
            p = {coordinate(random), coordinate(random) * 0.75};
        }
        std::vector<std::size_t> naive(points.size());
        std::iota(naive.begin(), naive.end(), 0);
        const std::vector<std::size_t> order = shortRoute(points, Vec2(0.0));
        const Real shortened = routeLength(points, order, Vec2(0.0));
        CHECK(shortened < 0.5 * routeLength(points, naive, Vec2(0.0)));

        // 2-opt leaves no two legs of the route crossing each other.
        std::vector<Vec2> route{Vec2(0.0)};
        for (const std::size_t i : order) {
            route.push_back(points[i]);
        }
        for (std::size_t i = 0; i + 1 < route.size(); ++i) {
            for (std::size_t j = i + 2; j + 1 < route.size(); ++j) {
                CHECK_FALSE(properlyCross(route[i], route[i + 1], route[j], route[j + 1]));
            }
        }
    }
}
