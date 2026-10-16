#include "readycad/render/Triangulate.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace rc::render {

using core::Real;
using core::Vec2;

namespace {

constexpr Real kTiny = 1e-9;

/// Twice the signed area of the triangle o, a, b: positive when it turns left.
Real cross(Vec2 o, Vec2 a, Vec2 b) noexcept
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

Real signedArea(const std::vector<Vec2>& polygon) noexcept
{
    Real twice = 0.0;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const Vec2 a = polygon[i];
        const Vec2 b = polygon[(i + 1) % polygon.size()];
        twice += a.x * b.y - b.x * a.y;
    }
    return twice / 2.0;
}

/// Inside or on the triangle, whatever its orientation.
bool inTriangle(Vec2 p, Vec2 a, Vec2 b, Vec2 c) noexcept
{
    const Real d1 = cross(a, b, p);
    const Real d2 = cross(b, c, p);
    const Real d3 = cross(c, a, p);
    const bool negative = d1 < -kTiny || d2 < -kTiny || d3 < -kTiny;
    const bool positive = d1 > kTiny || d2 > kTiny || d3 > kTiny;
    return !(negative && positive);
}

bool same(Vec2 a, Vec2 b) noexcept
{
    return std::abs(a.x - b.x) <= kTiny && std::abs(a.y - b.y) <= kTiny;
}

/// Joins a clockwise hole into the counter-clockwise polygon by a bridge from the hole's
/// rightmost vertex M to a polygon vertex P it can see: the polygon then runs ..., P, M, around
/// the hole, M, P, ... — one simple polygon again, with a slit of no width.
void bridge(std::vector<Vec2>& polygon, const std::vector<Vec2>& hole)
{
    std::size_t m = 0;
    for (std::size_t i = 1; i < hole.size(); ++i) {
        if (hole[i].x > hole[m].x || (hole[i].x == hole[m].x && hole[i].y < hole[m].y)) {
            m = i;
        }
    }
    const Vec2 M = hole[m];

    // The nearest edge the ray from M to the right meets.
    const std::size_t n = polygon.size();
    Real nearest = std::numeric_limits<Real>::max();
    std::size_t edge = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const Vec2 a = polygon[i];
        const Vec2 b = polygon[(i + 1) % n];
        if ((a.y - M.y) * (b.y - M.y) > 0.0 || a.y == b.y) {
            continue; // the edge stays on one side of the ray, or runs along it
        }
        const Real x = a.x + (M.y - a.y) * (b.x - a.x) / (b.y - a.y);
        if (x >= M.x - kTiny && x < nearest) {
            nearest = x;
            edge = i;
        }
    }
    const Vec2 I(nearest, M.y);
    const std::size_t e0 = edge;
    const std::size_t e1 = (edge + 1) % n;
    std::size_t p = same(polygon[e0], I) ? e0
                  : same(polygon[e1], I) ? e1
                  : (polygon[e0].x > polygon[e1].x ? e0 : e1);

    // A reflex vertex inside the triangle M, I, P would hide P: take the one seen from M at the
    // smallest angle to the ray instead.
    if (!same(polygon[p], I)) {
        const Vec2 P = polygon[p];
        Real bestAngle = std::numeric_limits<Real>::max();
        Real bestDistance = std::numeric_limits<Real>::max();
        for (std::size_t j = 0; j < n; ++j) {
            const Vec2 v = polygon[j];
            const bool reflex = cross(polygon[(j + n - 1) % n], v, polygon[(j + 1) % n]) <= 0.0;
            if (j == p || !reflex || !inTriangle(v, M, I, P) || v.x < M.x) {
                continue;
            }
            const Real angle = std::atan2(std::abs(v.y - M.y), v.x - M.x);
            const Real distance = glm::length(v - M);
            if (angle < bestAngle || (angle == bestAngle && distance < bestDistance)) {
                bestAngle = angle;
                bestDistance = distance;
                p = j;
            }
        }
    }

    std::vector<Vec2> merged(polygon.begin(), polygon.begin() + static_cast<std::ptrdiff_t>(p) + 1);
    for (std::size_t k = 0; k <= hole.size(); ++k) {
        merged.push_back(hole[(m + k) % hole.size()]); // M, around the hole, M again
    }
    merged.push_back(polygon[p]);
    merged.insert(merged.end(), polygon.begin() + static_cast<std::ptrdiff_t>(p) + 1, polygon.end());
    polygon = std::move(merged);
}

std::vector<Triangle2> clipEars(const std::vector<Vec2>& polygon)
{
    std::vector<Triangle2> triangles;
    std::vector<std::size_t> left(polygon.size());
    std::iota(left.begin(), left.end(), 0);
    while (left.size() > 3) {
        bool clipped = false;
        const std::size_t count = left.size();
        for (std::size_t k = 0; k < count && !clipped; ++k) {
            const Vec2 a = polygon[left[(k + count - 1) % count]];
            const Vec2 b = polygon[left[k]];
            const Vec2 c = polygon[left[(k + 1) % count]];
            const Real turn = cross(a, b, c);
            if (std::abs(turn) <= kTiny) {
                left.erase(left.begin() + static_cast<std::ptrdiff_t>(k)); // no area: drop it
                clipped = true;
                continue;
            }
            if (turn < 0.0) {
                continue; // a reflex vertex is no ear
            }
            bool empty = true;
            for (const std::size_t j : left) {
                const Vec2 v = polygon[j];
                if (same(v, a) || same(v, b) || same(v, c)) {
                    continue; // the ear's own corners, or their twins on a bridge
                }
                if (inTriangle(v, a, b, c)) {
                    empty = false;
                    break;
                }
            }
            if (empty) {
                triangles.push_back({a, b, c});
                left.erase(left.begin() + static_cast<std::ptrdiff_t>(k));
                clipped = true;
            }
        }
        if (!clipped) {
            break; // only for broken input: better a hole in the face than an endless loop
        }
    }
    if (left.size() == 3) {
        const Vec2 a = polygon[left[0]];
        const Vec2 b = polygon[left[1]];
        const Vec2 c = polygon[left[2]];
        if (cross(a, b, c) > kTiny) {
            triangles.push_back({a, b, c});
        }
    }
    return triangles;
}

} // namespace

std::vector<Triangle2> triangulate(std::vector<Vec2> outer, const std::vector<std::vector<Vec2>>& holes)
{
    if (outer.size() < 3) {
        return {};
    }
    if (signedArea(outer) < 0.0) {
        std::reverse(outer.begin(), outer.end());
    }
    // Holes clockwise, the rightmost one first: each bridge then runs to the right, over
    // nothing that is still to be joined.
    std::vector<std::vector<Vec2>> sorted;
    for (const auto& hole : holes) {
        if (hole.size() >= 3) {
            sorted.push_back(hole);
            if (signedArea(sorted.back()) > 0.0) {
                std::reverse(sorted.back().begin(), sorted.back().end());
            }
        }
    }
    const auto maxX = [](const std::vector<Vec2>& h) {
        return std::max_element(h.begin(), h.end(), [](Vec2 a, Vec2 b) { return a.x < b.x; })->x;
    };
    std::sort(sorted.begin(), sorted.end(),
              [&](const auto& a, const auto& b) { return maxX(a) > maxX(b); });
    for (const auto& hole : sorted) {
        bridge(outer, hole);
    }
    return clipEars(outer);
}

} // namespace rc::render
