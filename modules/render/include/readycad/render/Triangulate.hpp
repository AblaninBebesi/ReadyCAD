#pragma once

#include "readycad/core/Math.hpp"

#include <array>
#include <vector>

namespace rc::render {

using Triangle2 = std::array<core::Vec2, 3>;

/// Splits a polygon with holes into triangles (ear clipping): the face of a shaped panel, a
/// worktop with its sink. `outer` and the holes are simple polygons in either orientation, the
/// holes inside `outer` and apart from each other. Each hole is joined to the outline by a
/// bridge to a vertex it can see (Eberly's method), then ears are cut off one by one.
/// The triangles cover the face exactly; they come out counter-clockwise.
[[nodiscard]] std::vector<Triangle2> triangulate(std::vector<core::Vec2> outer,
                                                 const std::vector<std::vector<core::Vec2>>& holes);

} // namespace rc::render
