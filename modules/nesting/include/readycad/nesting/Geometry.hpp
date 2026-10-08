#pragma once

#include "readycad/core/Math.hpp"

#include <vector>

/// Stage 7: stock of sheets, panel saw cutting and CNC nesting.
namespace rc::nesting {

using core::Real;
using core::Vec2;

/// Axis-aligned rectangle, mm.
struct Rect {
    Vec2 min{0.0};
    Vec2 max{0.0};

    [[nodiscard]] Real width() const noexcept { return max.x - min.x; }
    [[nodiscard]] Real height() const noexcept { return max.y - min.y; }
    [[nodiscard]] Vec2 size() const noexcept { return max - min; }
    [[nodiscard]] Real area() const noexcept { return width() * height(); }
    [[nodiscard]] bool isEmpty() const noexcept { return width() <= 0.0 || height() <= 0.0; }

    friend bool operator==(const Rect&, const Rect&) = default;
};

/// A simple polygon (no self-intersections, no holes) without the first point repeated at the
/// end. Parts are nested by their outer contour; normalized() makes it counter-clockwise.
using Polygon = std::vector<Vec2>;

/// Positive for a counter-clockwise polygon.
[[nodiscard]] Real signedArea(const Polygon& polygon) noexcept;
[[nodiscard]] Real area(const Polygon& polygon) noexcept;
[[nodiscard]] Rect bounds(const Polygon& polygon) noexcept;

/// The four corners, counter-clockwise from rect.min.
[[nodiscard]] Polygon rectangle(const Rect& rect);

/// True if the polygon fills its bounding box: an axis-aligned rectangle (possibly with extra
/// points along the sides).
[[nodiscard]] bool isRectangle(const Polygon& polygon, Real eps = 1e-6) noexcept;

/// Counter-clockwise copy. Throws std::invalid_argument for fewer than 3 points or no area.
[[nodiscard]] Polygon normalized(Polygon polygon);

/// Where a label fits inside a part: the part's box for a rectangle, otherwise a box around the
/// inside point farthest from the outline (searched on a grid) — for an L or a U shape the middle
/// of the bounding box can lie outside the part.
[[nodiscard]] Rect labelArea(const Polygon& polygon);

/// Rotation by quarter turns (90° each) counter-clockwise about the origin. Exact: parts on a
/// sheet are only ever turned by multiples of 90° (the grain of a decor allows 0° and 180°).
[[nodiscard]] Vec2 rotateQuarter(Vec2 p, int quarterTurns) noexcept;

/// Where a part lies on a sheet: sheet point = rotateQuarter(part point, quarterTurns) + offset.
struct Transform {
    int quarterTurns = 0; ///< 0..3
    Vec2 offset{0.0};

    [[nodiscard]] Vec2 apply(Vec2 p) const noexcept
    {
        return rotateQuarter(p, quarterTurns) + offset;
    }
    [[nodiscard]] Real degrees() const noexcept { return 90.0 * quarterTurns; }

    friend bool operator==(const Transform&, const Transform&) = default;
};

[[nodiscard]] Polygon transformed(const Polygon& polygon, const Transform& transform);
[[nodiscard]] Rect transformed(const Rect& rect, const Transform& transform) noexcept;

} // namespace rc::nesting
