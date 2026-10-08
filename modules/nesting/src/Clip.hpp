#pragma once

// Conversions between the module's polygons (mm, double) and Clipper2 paths (integers).

#include "readycad/nesting/Geometry.hpp"

#include <clipper2/clipper.h>

#include <cmath>
#include <cstdint>

namespace rc::nesting::clip {

using Clipper2Lib::Path64;
using Clipper2Lib::Paths64;
using Clipper2Lib::Point64;

/// Clipper computes with integers: 1 unit = 1 µm. Unions and differences are exact, and a
/// micrometre is far below anything a saw or a router can hold.
inline constexpr Real kUnitsPerMm = 1000.0;

[[nodiscard]] inline std::int64_t units(Real mm) noexcept
{
    return std::llround(mm * kUnitsPerMm);
}

[[nodiscard]] inline Real mm(std::int64_t units) noexcept
{
    return static_cast<Real>(units) / kUnitsPerMm;
}

[[nodiscard]] inline Point64 point(Vec2 p) noexcept
{
    return Point64(units(p.x), units(p.y));
}

[[nodiscard]] inline Vec2 vec(const Point64& p) noexcept
{
    return {mm(p.x), mm(p.y)};
}

/// Axis-aligned box in Clipper units, y up (Clipper's own Rect64 counts y downwards).
struct Box {
    std::int64_t x0 = 0;
    std::int64_t y0 = 0;
    std::int64_t x1 = 0;
    std::int64_t y1 = 0;

    [[nodiscard]] bool isValid() const noexcept { return x0 <= x1 && y0 <= y1; }
    [[nodiscard]] bool overlaps(const Box& o) const noexcept
    {
        return x0 < o.x1 && o.x0 < x1 && y0 < o.y1 && o.y0 < y1;
    }
    [[nodiscard]] Box moved(const Point64& d) const noexcept
    {
        return {x0 + d.x, y0 + d.y, x1 + d.x, y1 + d.y};
    }
    [[nodiscard]] Box grown(std::int64_t d) const noexcept
    {
        return {x0 - d, y0 - d, x1 + d, y1 + d};
    }
};

[[nodiscard]] Path64 path(const Polygon& polygon);
[[nodiscard]] Polygon polygon(const Path64& path);
/// Counter-clockwise rectangle.
[[nodiscard]] Path64 boxPath(const Box& box);
[[nodiscard]] Box boundsOf(const Path64& path) noexcept;
[[nodiscard]] Box boundsOf(const Paths64& paths) noexcept;
[[nodiscard]] Box box(const Rect& rect) noexcept;

} // namespace rc::nesting::clip
