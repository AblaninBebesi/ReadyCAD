#include "readycad/nesting/Geometry.hpp"

#include <glm/common.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace rc::nesting {

Real signedArea(const Polygon& polygon) noexcept
{
    Real twice = 0.0;
    const std::size_t n = polygon.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Vec2& a = polygon[i];
        const Vec2& b = polygon[(i + 1) % n];
        twice += a.x * b.y - b.x * a.y;
    }
    return twice / 2.0;
}

Real area(const Polygon& polygon) noexcept
{
    return std::abs(signedArea(polygon));
}

Rect bounds(const Polygon& polygon) noexcept
{
    if (polygon.empty()) {
        return {};
    }
    Rect r{polygon.front(), polygon.front()};
    for (const Vec2& p : polygon) {
        r.min = glm::min(r.min, p);
        r.max = glm::max(r.max, p);
    }
    return r;
}

Polygon rectangle(const Rect& rect)
{
    return {rect.min, {rect.max.x, rect.min.y}, rect.max, {rect.min.x, rect.max.y}};
}

bool isRectangle(const Polygon& polygon, Real eps) noexcept
{
    if (polygon.size() < 4) {
        return false;
    }
    // A simple polygon as large as its bounding box is that box. The tolerance grows with the
    // perimeter: every side may be off by eps.
    const Rect box = bounds(polygon);
    return std::abs(area(polygon) - box.area()) <= eps * 2.0 * (box.width() + box.height());
}

Polygon normalized(Polygon polygon)
{
    if (polygon.size() < 3) {
        throw std::invalid_argument("a polygon needs at least 3 points");
    }
    const Real a = signedArea(polygon);
    if (std::abs(a) <= core::kEpsilon) {
        throw std::invalid_argument("the polygon has no area");
    }
    if (a < 0.0) {
        std::reverse(polygon.begin(), polygon.end());
    }
    return polygon;
}

namespace {

bool inside(Vec2 p, const Polygon& polygon) noexcept
{
    bool in = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Vec2 a = polygon[i];
        const Vec2 b = polygon[j];
        if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) {
            in = !in;
        }
    }
    return in;
}

Real distanceToOutline(Vec2 p, const Polygon& polygon) noexcept
{
    Real d = std::numeric_limits<Real>::max();
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Vec2 a = polygon[j];
        const Vec2 ab = polygon[i] - a;
        const Real len2 = ab.x * ab.x + ab.y * ab.y;
        const Real t = len2 > 0.0 ? std::clamp(((p - a).x * ab.x + (p - a).y * ab.y) / len2, 0.0, 1.0)
                                  : 0.0;
        const Vec2 q = a + t * ab - p;
        d = std::min(d, std::sqrt(q.x * q.x + q.y * q.y));
    }
    return d;
}

} // namespace

Rect labelArea(const Polygon& polygon)
{
    const Rect box = bounds(polygon);
    if (polygon.size() < 3 || isRectangle(polygon)) {
        return box;
    }
    constexpr int kSteps = 32;
    Vec2 best = (box.min + box.max) * 0.5;
    Real depth = inside(best, polygon) ? distanceToOutline(best, polygon) : 0.0;
    for (int i = 1; i < kSteps; ++i) {
        for (int j = 1; j < kSteps; ++j) {
            const Vec2 p = box.min + box.size() * Vec2(i, j) / static_cast<Real>(kSteps);
            if (!inside(p, polygon)) {
                continue;
            }
            if (const Real d = distanceToOutline(p, polygon); d > depth) {
                depth = d;
                best = p;
            }
        }
    }
    // A box that stays inside the circle of radius `depth`, a little wider than high (text).
    const Vec2 half(depth * 0.9, depth * 0.55);
    return {best - half, best + half};
}

Vec2 rotateQuarter(Vec2 p, int quarterTurns) noexcept
{
    switch (((quarterTurns % 4) + 4) % 4) {
    case 1: return {-p.y, p.x};
    case 2: return {-p.x, -p.y};
    case 3: return {p.y, -p.x};
    default: return p;
    }
}

Polygon transformed(const Polygon& polygon, const Transform& transform)
{
    Polygon out;
    out.reserve(polygon.size());
    for (const Vec2& p : polygon) {
        out.push_back(transform.apply(p));
    }
    return out;
}

Rect transformed(const Rect& rect, const Transform& transform) noexcept
{
    const Vec2 a = transform.apply(rect.min);
    const Vec2 b = transform.apply(rect.max);
    return {glm::min(a, b), glm::max(a, b)};
}

} // namespace rc::nesting
