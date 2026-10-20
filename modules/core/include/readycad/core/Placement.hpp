#pragma once

#include "readycad/core/Aabb.hpp"

namespace rc::core {

/// Where a part of the furniture with coordinates of its own stands in the product: turned about
/// the vertical axis Y by quarter turns, then moved.
///
/// A kitchen run on a side wall is laid out in its own coordinates — its length along X, its
/// front towards +Z, like every cabinet — so the formulas of the cabinets, their drilling and
/// their parts never learn which wall they stand at; only what needs the room (3D, collisions
/// between walls, the plan view) places them. Quarter turns keep every axis on an axis: a box
/// stays an exact box, and the collision check of boxes stays exact.
struct Placement {
    /// About +Y, right-handed: one turn takes +X to -Z and +Z to +X — a run turned once faces
    /// +X, it stands at the left wall; three turns: it faces -X, at the right wall.
    int quarterTurns = 0;
    Vec3 offset{0.0};

    [[nodiscard]] Vec3 point(const Vec3& p) const noexcept;
    /// A direction or a normal: turned, not moved.
    [[nodiscard]] Vec3 direction(const Vec3& v) const noexcept;
    [[nodiscard]] Aabb box(const Aabb& b) const noexcept;
    /// The product axis a local axis lies on (X and Z swap with an odd number of turns).
    [[nodiscard]] Axis axis(Axis a) const noexcept;
    /// First this placement, then `outer`: coordinates of a frame inside another one.
    [[nodiscard]] Placement within(const Placement& outer) const noexcept;
    [[nodiscard]] bool isIdentity() const noexcept;

    friend bool operator==(const Placement&, const Placement&) = default;
};

} // namespace rc::core
