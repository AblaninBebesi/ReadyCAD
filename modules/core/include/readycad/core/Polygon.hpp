#pragma once

#include "readycad/core/Math.hpp"

#include <vector>

/// Small 2D helpers for the outlines of panels (closed polygons, the last point joins the first).
namespace rc::core {

/// Positive for a counter-clockwise polygon.
[[nodiscard]] Real signedArea(const std::vector<Vec2>& polygon) noexcept;

/// True if the point lies inside the polygon (on the boundary: either answer).
[[nodiscard]] bool contains(const std::vector<Vec2>& polygon, Vec2 point) noexcept;

[[nodiscard]] Real distanceToSegment(Vec2 point, Vec2 a, Vec2 b) noexcept;

/// Distance from the point to the nearest side of the polygon.
[[nodiscard]] Real distanceToOutline(Vec2 point, const std::vector<Vec2>& polygon) noexcept;

/// True if the segments a-b and c-d cross or come closer than `tolerance`.
[[nodiscard]] bool segmentsMeet(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Real tolerance) noexcept;

/// True if no two sides of the polygon that are not neighbours meet (within `tolerance`): the
/// outline does not cut through itself.
[[nodiscard]] bool isSimple(const std::vector<Vec2>& polygon, Real tolerance) noexcept;

/// Area of the part of the polygon inside the axis-aligned rectangle lo..hi (the polygon clipped
/// by the rectangle, Sutherland-Hodgman: right for any simple polygon, the rectangle is convex).
[[nodiscard]] Real areaInside(const std::vector<Vec2>& polygon, Vec2 lo, Vec2 hi);

/// Smallest distance between the outlines of two polygons (0 if they cross).
[[nodiscard]] Real outlineDistance(const std::vector<Vec2>& a, const std::vector<Vec2>& b) noexcept;

} // namespace rc::core
