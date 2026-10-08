#pragma once

#include "readycad/core/Math.hpp"

#include <cstddef>
#include <vector>

namespace rc::cam {

/// Order in which to visit `points` (drill holes) when starting at `from`, short to travel:
/// nearest neighbour first, then 2-opt — two edges of the route are swapped while that makes it
/// shorter (it also removes every crossing). Returns indices into `points`.
[[nodiscard]] std::vector<std::size_t> shortRoute(const std::vector<core::Vec2>& points,
                                                  core::Vec2 from);

/// Length of the open route from `from` through the points in `order`.
[[nodiscard]] core::Real routeLength(const std::vector<core::Vec2>& points,
                                     const std::vector<std::size_t>& order, core::Vec2 from);

} // namespace rc::cam
