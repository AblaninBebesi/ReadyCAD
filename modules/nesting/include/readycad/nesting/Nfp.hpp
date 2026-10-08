#pragma once

#include "readycad/nesting/Geometry.hpp"

#include <optional>
#include <vector>

namespace rc::nesting {

/// No-Fit Polygon (NFP) of `moving` around `fixed`, both in their own coordinates with `fixed`
/// at the origin: the positions of `moving`'s origin at which the two parts overlap. On its
/// boundary they touch, outside it they are apart. It is the Minkowski sum fixed ⊕ (−moving).
/// Cavities of `fixed` that `moving` could only reach through `fixed` are filled, as in deepnest:
/// the result may forbid a few free positions but never allows an overlapping one.
/// Usually one polygon, counter-clockwise.
[[nodiscard]] std::vector<Polygon> noFitPolygon(const Polygon& fixed, const Polygon& moving);

/// Inner-Fit Rectangle (IFR): the positions of `moving`'s origin at which it lies inside
/// `container`. Empty optional if it is too big; zero width or height if it fits exactly.
[[nodiscard]] std::optional<Rect> innerFitRectangle(const Rect& container, const Polygon& moving);

} // namespace rc::nesting
