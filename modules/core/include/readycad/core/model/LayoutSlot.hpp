#pragma once

#include "readycad/core/Aabb.hpp"
#include "readycad/core/ConstructionRules.hpp"
#include "readycad/core/Panel.hpp"

#include <optional>
#include <vector>

namespace rc::core {

class BackMount;

enum class Side { Left, Right };

/// How a horizontal carcass panel (top or bottom) meets the side walls.
enum class JointType {
    Overlay, // накладная: lies on the sides, full carcass width
    Inset,   // вкладная: sits between the sides
};

/// What closes a carcass at the top.
enum class CarcassTop {
    Panel,        // крышка: a top panel (wardrobes, wall cabinets)
    FlatRails,    // two flat rails (kuşak) under a worktop: base cabinets
    UprightRails, // two rails on edge: room under the worktop for a sink bowl or a hob
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
    Aabb outer;                       // the carcass itself (above its plinth), product coordinates
    // Thickness of each structural panel: its own material, which the user may change per part
    // (a 25 mm top on 18 mm sides). Every neighbour's formula uses the one it meets.
    Real leftThickness = 18.0;
    Real rightThickness = 18.0;
    Real topThickness = 18.0;
    Real bottomThickness = 18.0;
    JointType topJoint = JointType::Overlay; // rails always stand between the sides: Inset
    JointType bottomJoint = JointType::Inset;
    CarcassTop top = CarcassTop::Panel;
    const BackMount* backMount = nullptr;
    Real backThickness = 3.0;

    /// Where sides/top/bottom start in Z, measured from the carcass rear (non-zero when the
    /// back panel is nailed onto the rear edges).
    [[nodiscard]] Real rearOffset() const noexcept;
};

/// Thicknesses of the panels of one drawer box (set by a Drawer for its box parts): the parts
/// meet each other, so each formula needs the material its neighbours really have.
struct DrawerBoxFrame {
    Real leftSide = 16.0;
    Real rightSide = 16.0;
    Real bottom = 3.0;
};

/// What a Kitchen tells its runs, their corners and their worktops (heights in product Y, the
/// rest in the coordinates of the run).
struct KitchenFrame {
    Real baseTop = 0.0;    // top of the base cabinets: the underside of the worktop
    Real wallBottom = 0.0; // underside of the wall cabinets
    Real wallDepth = 320.0;
    Real worktopDepth = 600.0;
    /// How far from the back wall the cabinets of a side run start: the part of the back run's
    /// corner cabinets they hide (its cabinets' depth and their fronts), base and wall.
    Real cornerBase = 0.0;
    Real cornerWall = 0.0;
    /// For a worktop: the openings the cabinets below need (a sink, a hob; centre in X
    /// (center.x) and Z (center.y)), and which of its ends meet a wall or another worktop.
    std::vector<PanelCutout> cutouts;
    bool startClosed = false; // at X min
    bool endClosed = false;   // at X max
};

/// What a kitchen corner tells its blind cabinets: the end that runs on behind the cabinets of
/// the other wall, and how much of it they hide, from that end of the carcass.
struct CornerFrame {
    Side blindEnd = Side::Right;
    Real hidden = 0.0;
};

/// Sizes of items along one axis: an item with a fixed size keeps it, the automatic ones
/// (nullopt) share what is left of `available` equally. If the fixed sizes do not fit, or no
/// item is automatic, the fixed sizes are scaled to fill `available` exactly.
[[nodiscard]] std::vector<Real> distributeSizes(Real available,
                                                const std::vector<std::optional<Real>>& fixed);

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
    const DrawerBoxFrame* drawerBox = nullptr; // set by a Drawer for its box parts
    const KitchenFrame* kitchen = nullptr;     // set by a Kitchen for everything in it
    const CornerFrame* corner = nullptr;       // set by a kitchen corner for its cabinets
    /// How far the plinth of a base cabinet reaches beyond the carcass at X min / X max
    /// (negative: it stops short) — set at a kitchen corner, so that the plinths of the two
    /// walls meet. A cabinet row passes them to its first and last cabinet.
    Real plinthBeyondStart = 0.0;
    Real plinthBeyondEnd = 0.0;
    /// Y of a hole of the enclosing carcass's System 32 grid (set by the Carcass for its
    /// interior); shelves on supports snap to it.
    std::optional<Real> holeGridOriginY;

    [[nodiscard]] const ConstructionRules& r() const noexcept { return *rules; }

    /// `box` expanded by the overlays in X and Y: the rectangle a front covers.
    [[nodiscard]] Aabb frontRect() const noexcept;

    /// Copy of this slot with another box (same rules, frame, front plane, no overlays).
    [[nodiscard]] LayoutSlot withBox(const Aabb& newBox) const noexcept;
};

} // namespace rc::core
