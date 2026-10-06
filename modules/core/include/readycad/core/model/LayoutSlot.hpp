#pragma once

#include "readycad/core/Aabb.hpp"
#include "readycad/core/ConstructionRules.hpp"

namespace rc::core {

class BackMount;

/// How a horizontal carcass panel (top or bottom) meets the side walls.
enum class JointType {
    Overlay, // накладная: lies on the sides, full carcass width
    Inset,   // вкладная: sits between the sides
};

/// Front overlay: how far a front (door, drawer front) may extend beyond the opening it closes,
/// on each side, mm. The parent computes it, because only the parent knows whether a boundary
/// is an outer carcass wall (front covers almost all of its thickness) or a partition shared
/// with a neighbouring front (each front covers half of it, minus half the gap).
struct FrontOverlay {
    Real left = 0.0;
    Real right = 0.0;
    Real bottom = 0.0;
    Real top = 0.0;

    /// Overlay at the min/max end of an axis: X -> left/right, Y -> bottom/top.
    [[nodiscard]] Real& atMin(Axis axis) noexcept { return axis == Axis::X ? left : bottom; }
    [[nodiscard]] Real& atMax(Axis axis) noexcept { return axis == Axis::X ? right : top; }
    [[nodiscard]] Real atMin(Axis axis) const noexcept { return axis == Axis::X ? left : bottom; }
    [[nodiscard]] Real atMax(Axis axis) const noexcept { return axis == Axis::X ? right : top; }
};

/// Geometry of the enclosing carcass that its structural parts (sides, top, bottom, back)
/// need for their own formulas.
struct CarcassFrame {
    Aabb outer;                       // overall carcass box, product coordinates
    Real thickness = 18.0;            // carcass material thickness
    JointType topJoint = JointType::Overlay;
    JointType bottomJoint = JointType::Inset;
    const BackMount* backMount = nullptr;
    Real backThickness = 3.0;

    /// Where sides/top/bottom start in Z, measured from the carcass rear (non-zero when the
    /// back panel is nailed onto the rear edges).
    [[nodiscard]] Real rearOffset() const noexcept;
};

/// What a parent offers to a child during layout — "constraints go down".
///
/// The parent decides WHERE (the region and context), the child decides HOW (its own formula
/// inside that region). The child returns its size and position — "sizes go up" — and the
/// parent may read them to place the next children.
struct LayoutSlot {
    Aabb box;                     // region offered to the child, product coordinates
    FrontOverlay overlays{};      // only meaningful for fronts and front holders
    Real frontZ = 0.0;            // Z of the carcass front plane: fronts start here
    const ConstructionRules* rules = &ConstructionRules::defaults();
    const CarcassFrame* frame = nullptr; // set by a Carcass for its own structural parts

    [[nodiscard]] const ConstructionRules& r() const noexcept { return *rules; }

    /// `box` expanded by the overlays in X and Y: the rectangle a front covers.
    [[nodiscard]] Aabb frontRect() const noexcept;

    /// Copy of this slot with another box (same rules, frame, front plane, no overlays).
    [[nodiscard]] LayoutSlot withBox(const Aabb& newBox) const noexcept;
};

} // namespace rc::core
