#pragma once

#include "readycad/core/Math.hpp"

namespace rc::core {

/// Axis-aligned bounding box in millimetres.
///
/// Case furniture is (almost) entirely made of axis-aligned rectangular panels, so an AABB is the
/// natural tool for "does this shelf collide with that side wall?" checks, picking and camera
/// framing. A default-constructed box is empty; expanding it with points makes it non-empty.
class Aabb {
public:
    /// Empty box (contains nothing, isEmpty() == true).
    Aabb() noexcept;

    /// Box spanned by two opposite corners; corner order does not matter.
    Aabb(const Vec3& cornerA, const Vec3& cornerB) noexcept;

    /// Box with its minimum corner at `origin` and the given (non-negative) size.
    [[nodiscard]] static Aabb fromOriginAndSize(const Vec3& origin, const Vec3& size) noexcept;

    [[nodiscard]] bool isEmpty() const noexcept;

    [[nodiscard]] const Vec3& min() const noexcept { return min_; }
    [[nodiscard]] const Vec3& max() const noexcept { return max_; }
    [[nodiscard]] Vec3 size() const noexcept;
    [[nodiscard]] Vec3 center() const noexcept;
    [[nodiscard]] Real volume() const noexcept;

    /// True if the point is inside or on the boundary.
    [[nodiscard]] bool contains(const Vec3& point, Real eps = kEpsilon) const noexcept;
    /// True if `other` lies completely inside this box (boundaries may coincide).
    [[nodiscard]] bool contains(const Aabb& other, Real eps = kEpsilon) const noexcept;

    /// True if the INTERIORS overlap by more than `eps` on every axis.
    /// Boxes that only touch (a shelf resting on a side wall's face) do NOT intersect —
    /// that is exactly the "parts must not overlap" rule of furniture assembly.
    [[nodiscard]] bool intersects(const Aabb& other, Real eps = kEpsilon) const noexcept;

    /// Common part of two boxes; empty if they do not overlap.
    [[nodiscard]] Aabb intersection(const Aabb& other) const noexcept;

    [[nodiscard]] Aabb translated(const Vec3& offset) const noexcept;

    void expand(const Vec3& point) noexcept;
    void expand(const Aabb& other) noexcept;

    friend bool operator==(const Aabb&, const Aabb&) = default;

private:
    Vec3 min_;
    Vec3 max_;
};

} // namespace rc::core
