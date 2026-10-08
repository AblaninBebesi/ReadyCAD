#pragma once

// Independent checks of nesting results for the tests: plain floating-point geometry, no
// Clipper, no code of the module except the data types. If the optimiser and these checks
// disagree, one of them is wrong — and these are short enough to read.

#include "readycad/nesting/Nesting.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace rc::nesting::test {

/// Polygon of a placed part on its sheet.
inline Polygon placedOutline(const NestingResult& result, const PlacedPart& placed)
{
    return transformed(result.parts[placed.part].outline, placed.transform);
}

inline Real cross(Vec2 a, Vec2 b)
{
    return a.x * b.y - a.y * b.x;
}

/// Distance from p to the segment ab.
inline Real pointSegmentDistance(Vec2 p, Vec2 a, Vec2 b)
{
    const Vec2 ab = b - a;
    const Real len2 = glm::dot(ab, ab);
    const Real t = len2 > 0.0 ? std::clamp(glm::dot(p - a, ab) / len2, 0.0, 1.0) : 0.0;
    return glm::length(p - (a + t * ab));
}

inline bool segmentsCross(Vec2 a, Vec2 b, Vec2 c, Vec2 d)
{
    const Real d1 = cross(b - a, c - a);
    const Real d2 = cross(b - a, d - a);
    const Real d3 = cross(d - c, a - c);
    const Real d4 = cross(d - c, b - c);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0)) && d1 != 0 && d2 != 0 && d3 != 0
        && d4 != 0;
}

inline Real segmentDistance(Vec2 a, Vec2 b, Vec2 c, Vec2 d)
{
    if (segmentsCross(a, b, c, d)) {
        return 0.0;
    }
    return std::min({pointSegmentDistance(a, c, d), pointSegmentDistance(b, c, d),
                     pointSegmentDistance(c, a, b), pointSegmentDistance(d, a, b)});
}

/// Strictly inside (ray casting; points on the boundary may go either way).
inline bool inside(Vec2 p, const Polygon& polygon)
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

/// Distance between the outlines of two polygons; 0 if they overlap or one contains the other.
inline Real distance(const Polygon& p, const Polygon& q)
{
    Real d = std::numeric_limits<Real>::max();
    for (std::size_t i = 0; i < p.size(); ++i) {
        for (std::size_t j = 0; j < q.size(); ++j) {
            d = std::min(d, segmentDistance(p[i], p[(i + 1) % p.size()], q[j],
                                            q[(j + 1) % q.size()]));
        }
    }
    // Containment: then no edges come close, but the parts still overlap.
    if (d > 1e-9 && (inside(p.front(), q) || inside(q.front(), p))) {
        return 0.0;
    }
    return d;
}

/// True if two polygons overlap by more than touching: some vertex lies inside the other by
/// more than `tolerance`, or edges cross.
inline bool overlap(const Polygon& p, const Polygon& q, Real tolerance = 1e-6)
{
    const auto deepInside = [&](const Polygon& a, const Polygon& b) {
        for (const Vec2& v : a) {
            if (!inside(v, b)) {
                continue;
            }
            Real edge = std::numeric_limits<Real>::max();
            for (std::size_t j = 0; j < b.size(); ++j) {
                edge = std::min(edge, pointSegmentDistance(v, b[j], b[(j + 1) % b.size()]));
            }
            if (edge > tolerance) {
                return true;
            }
        }
        return false;
    };
    for (std::size_t i = 0; i < p.size(); ++i) {
        for (std::size_t j = 0; j < q.size(); ++j) {
            const Vec2 a = p[i];
            const Vec2 b = p[(i + 1) % p.size()];
            const Vec2 c = q[j];
            const Vec2 d = q[(j + 1) % q.size()];
            if (segmentsCross(a, b, c, d)
                && std::min({pointSegmentDistance(a, c, d), pointSegmentDistance(b, c, d),
                             pointSegmentDistance(c, a, b), pointSegmentDistance(d, a, b)})
                       > tolerance) {
                return true;
            }
        }
    }
    return deepInside(p, q) || deepInside(q, p);
}

/// Checks one sheet layout: every part inside `area`, every two parts at least `gap` apart.
/// Returns an empty string if all is well, else what is wrong.
inline std::string checkSheet(const NestingResult& result, const SheetLayout& layout,
                              const Rect& area, Real gap, Real tolerance = 0.01)
{
    std::vector<Polygon> outlines;
    for (const PlacedPart& p : layout.parts) {
        outlines.push_back(placedOutline(result, p));
        const Rect b = bounds(outlines.back());
        if (b.min.x < area.min.x - tolerance || b.min.y < area.min.y - tolerance
            || b.max.x > area.max.x + tolerance || b.max.y > area.max.y + tolerance) {
            return "part " + result.parts[p.part].label + " leaves the usable area";
        }
    }
    for (std::size_t i = 0; i < outlines.size(); ++i) {
        for (std::size_t j = i + 1; j < outlines.size(); ++j) {
            const Real d = distance(outlines[i], outlines[j]);
            if (d < gap - tolerance) {
                return "parts " + result.parts[layout.parts[i].part].label + " and "
                     + result.parts[layout.parts[j].part].label + " are "
                     + std::to_string(d) + " mm apart";
            }
        }
    }
    return {};
}

/// How many guillotine stages the rectangles need to be cut apart with this kerf: cuts are
/// straight lines through the whole piece, the cuts of one stage all run in one direction,
/// stage 1 horizontally (y = const) if `horizontal`. A stage that separates nothing (the saw
/// only cuts waste off) still counts. -1 if guillotine cuts cannot separate them at all.
inline int guillotineStages(const std::vector<Rect>& rects, Real kerf, bool horizontal,
                            bool otherTried = false)
{
    if (rects.size() <= 1) {
        return 0;
    }
    constexpr Real eps = 1e-6;
    const auto lo = [&](const Rect& r) { return horizontal ? r.min.y : r.min.x; };
    const auto hi = [&](const Rect& r) { return horizontal ? r.max.y : r.max.x; };
    std::vector<Rect> sorted = rects;
    std::sort(sorted.begin(), sorted.end(),
              [&](const Rect& a, const Rect& b) { return lo(a) < lo(b); });
    // Slabs: wherever a cut line fits between the rectangles with a kerf of room.
    std::vector<std::vector<Rect>> slabs{{sorted.front()}};
    Real reach = hi(sorted.front());
    for (std::size_t i = 1; i < sorted.size(); ++i) {
        if (lo(sorted[i]) >= reach + kerf - eps) {
            slabs.emplace_back();
        }
        slabs.back().push_back(sorted[i]);
        reach = std::max(reach, hi(sorted[i]));
    }
    if (slabs.size() == 1) {
        if (otherTried) {
            return -1; // no cut in either direction separates anything
        }
        const int rest = guillotineStages(rects, kerf, !horizontal, true);
        return rest < 0 ? -1 : 1 + rest;
    }
    int deepest = 0;
    for (const auto& slab : slabs) {
        const int inner = guillotineStages(slab, kerf, !horizontal);
        if (inner < 0) {
            return -1;
        }
        deepest = std::max(deepest, inner);
    }
    return 1 + deepest;
}

} // namespace rc::nesting::test
