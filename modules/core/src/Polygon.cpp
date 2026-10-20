#include "readycad/core/Polygon.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <limits>

namespace rc::core {

namespace {

Real cross(Vec2 a, Vec2 b) noexcept
{
    return a.x * b.y - a.y * b.x;
}

/// -1, 0, 1: on which side of the line a-b the point c lies.
int orientation(Vec2 a, Vec2 b, Vec2 c) noexcept
{
    const Real v = cross(b - a, c - a);
    return v > 0.0 ? 1 : (v < 0.0 ? -1 : 0);
}

} // namespace

Real signedArea(const std::vector<Vec2>& polygon) noexcept
{
    Real twice = 0.0;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        twice += cross(polygon[i], polygon[(i + 1) % polygon.size()]);
    }
    return twice / 2.0;
}

Real areaInside(const std::vector<Vec2>& polygon, Vec2 lo, Vec2 hi)
{
    std::vector<Vec2> clipped = polygon;
    // One half-plane per side of the rectangle: keep what is on its inner side.
    const auto clip = [&clipped](int axis, Real limit, bool keepAbove) {
        std::vector<Vec2> out;
        const auto inside = [&](Vec2 p) { return keepAbove ? p[axis] >= limit : p[axis] <= limit; };
        for (std::size_t i = 0; i < clipped.size(); ++i) {
            const Vec2 a = clipped[i];
            const Vec2 b = clipped[(i + 1) % clipped.size()];
            if (inside(a)) {
                out.push_back(a);
            }
            if (inside(a) != inside(b)) {
                const Real t = (limit - a[axis]) / (b[axis] - a[axis]);
                out.push_back(a + (b - a) * t);
            }
        }
        clipped = std::move(out);
    };
    clip(0, lo.x, true);
    clip(0, hi.x, false);
    clip(1, lo.y, true);
    clip(1, hi.y, false);
    return clipped.size() < 3 ? 0.0 : std::abs(signedArea(clipped));
}

bool contains(const std::vector<Vec2>& polygon, Vec2 point) noexcept
{
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Vec2 a = polygon[i];
        const Vec2 b = polygon[j];
        if ((a.y > point.y) != (b.y > point.y)
            && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x) {
            inside = !inside;
        }
    }
    return inside;
}

Real distanceToSegment(Vec2 point, Vec2 a, Vec2 b) noexcept
{
    const Vec2 ab = b - a;
    const Real lengthSquared = glm::dot(ab, ab);
    const Real t =
        lengthSquared > 0.0 ? std::clamp(glm::dot(point - a, ab) / lengthSquared, 0.0, 1.0) : 0.0;
    return glm::length(point - (a + ab * t));
}

Real distanceToOutline(Vec2 point, const std::vector<Vec2>& polygon) noexcept
{
    Real best = std::numeric_limits<Real>::max();
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        best = std::min(best, distanceToSegment(point, polygon[i], polygon[(i + 1) % polygon.size()]));
    }
    return best;
}

bool segmentsMeet(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Real tolerance) noexcept
{
    const int o1 = orientation(a, b, c);
    const int o2 = orientation(a, b, d);
    const int o3 = orientation(c, d, a);
    const int o4 = orientation(c, d, b);
    if (o1 * o2 < 0 && o3 * o4 < 0) {
        return true; // a proper crossing
    }
    return distanceToSegment(a, c, d) <= tolerance || distanceToSegment(b, c, d) <= tolerance
        || distanceToSegment(c, a, b) <= tolerance || distanceToSegment(d, a, b) <= tolerance;
}

bool isSimple(const std::vector<Vec2>& polygon, Real tolerance) noexcept
{
    const std::size_t n = polygon.size();
    if (n < 3) {
        return false;
    }
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 2; j < n; ++j) {
            if (i == 0 && j == n - 1) {
                continue; // the last side is the first one's neighbour
            }
            if (segmentsMeet(polygon[i], polygon[(i + 1) % n], polygon[j], polygon[(j + 1) % n],
                             tolerance)) {
                return false;
            }
        }
    }
    return true;
}

Real outlineDistance(const std::vector<Vec2>& a, const std::vector<Vec2>& b) noexcept
{
    Real best = std::numeric_limits<Real>::max();
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t j = 0; j < b.size(); ++j) {
            const Vec2 a0 = a[i];
            const Vec2 a1 = a[(i + 1) % a.size()];
            const Vec2 b0 = b[j];
            const Vec2 b1 = b[(j + 1) % b.size()];
            if (segmentsMeet(a0, a1, b0, b1, 0.0)) {
                return 0.0;
            }
            best = std::min({best, distanceToSegment(a0, b0, b1), distanceToSegment(a1, b0, b1),
                             distanceToSegment(b0, a0, a1), distanceToSegment(b1, a0, a1)});
        }
    }
    return best;
}

} // namespace rc::core
