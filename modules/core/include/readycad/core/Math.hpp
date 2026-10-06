#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cmath>

// Basic numeric conventions of the whole project.
//
//  * Unit of length: millimetre. Every number that is a length is in mm.
//  * Scalar type: double. A 2800 mm wardrobe side must keep 0.001 mm precision through many
//    recalculations; float has only ~7 significant digits, double has ~15. Convert to float only
//    at the very edge (GPU buffers in the render module).
//  * Axes (right-handed, as in OpenGL): X = width (left -> right), Y = height (bottom -> top),
//    Z = depth (back -> front).
namespace rc::core {

using Real = double;
using Vec2 = glm::dvec2;
using Vec3 = glm::dvec3;

/// Geometric tolerance, mm. Two lengths closer than this are considered equal.
inline constexpr Real kEpsilon = 1e-6;

[[nodiscard]] inline bool nearlyEqual(Real a, Real b, Real eps = kEpsilon) noexcept
{
    return std::abs(a - b) <= eps;
}

[[nodiscard]] inline bool nearlyEqual(const Vec3& a, const Vec3& b, Real eps = kEpsilon) noexcept
{
    return nearlyEqual(a.x, b.x, eps) && nearlyEqual(a.y, b.y, eps) && nearlyEqual(a.z, b.z, eps);
}

/// Rounds a length to the given step (e.g. 0.5 mm, 32 mm). step must be > 0.
[[nodiscard]] inline Real roundToStep(Real value, Real step) noexcept
{
    return std::round(value / step) * step;
}

} // namespace rc::core
