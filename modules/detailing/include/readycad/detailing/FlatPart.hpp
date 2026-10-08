#pragma once

#include "readycad/core/ConstructionRules.hpp"
#include "readycad/core/Panel.hpp"
#include "readycad/core/model/Product.hpp"
#include "readycad/detailing/CutList.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace rc::detailing {

/// Where a hole or a groove is, seen from the view face of a FlatPart.
enum class FlatSide {
    Near, ///< in the view face: visible, machined while the part lies with this face up
    Far,  ///< in the opposite face: hidden in the drawing, the part must be turned over
    Edge, ///< in one of the four edges (horizontal drilling)
};

/// An axis-aligned rectangle in the drawing plane, mm.
struct FlatRect {
    core::Vec2 min{0.0};
    core::Vec2 max{0.0};

    [[nodiscard]] core::Vec2 size() const noexcept { return max - min; }
};

struct FlatHole {
    core::FastenerType fastener = core::FastenerType::Dowel;
    FlatSide side = FlatSide::Near;
    core::Vec2 center{0.0};    ///< face hole: its centre; edge hole: entry point on the outline
    core::Vec2 direction{0.0}; ///< edge hole: unit vector into the part; face hole: zero
    core::Real diameter = 0.0;
    core::Real depth = 0.0;
    core::Real height = 0.0; ///< edge hole: distance of its axis from the view face
    core::EdgeSide edge = core::EdgeSide::L1; ///< edge hole: the edge it is drilled into
};

struct FlatGroove {
    FlatSide side = FlatSide::Near;
    core::Vec2 start{0.0}; ///< centre line
    core::Vec2 end{0.0};
    core::Real width = 0.0;
    core::Real depth = 0.0;
};

/// One side of the finished outline: which edge of the panel it is, its band, and where it lies.
struct FlatEdge {
    core::EdgeSide side = core::EdgeSide::L1;
    core::EdgeBand band;
    core::Vec2 start{0.0}; ///< the outline runs counter-clockwise: bottom, right, top, left
    core::Vec2 end{0.0};
};

/// A panel unfolded into the drawing plane, as seen from one of its big faces ("view face"):
/// u to the right along the length, v up along the width, origin in the lower-left corner of the
/// FINISHED part. Hole coordinates are measured from there.
///
/// PanelFrame (where drillings are stored) can be a mirror image of what a person sees when
/// looking at a face. FlatPart never is: a hole near the left edge in the drawing is near the
/// left edge of the real part lying view face up. Drawings, DXF and later the CNC programs use
/// this one view, so they always agree.
struct FlatPart {
    core::Real length = 0.0; ///< finish size along u
    core::Real width = 0.0;  ///< finish size along v
    core::Real thickness = 0.0;
    core::PanelFace viewFace = core::PanelFace::Top; ///< Top or Bottom
    bool flipped = false; ///< v runs against the panel's local y (L2 at the bottom)

    FlatRect cut; ///< the sawn blank before edge banding (pre-milling included)
    std::array<FlatEdge, 4> outline{}; ///< bottom, right, top, left
    std::vector<FlatHole> holes;       ///< sorted: near, far, edge; then by v and u
    std::vector<FlatGroove> grooves;

    [[nodiscard]] std::size_t count(FlatSide side) const noexcept;
    /// The edge at a side of the outline: 0 bottom, 1 right, 2 top, 3 left.
    [[nodiscard]] const FlatEdge& edgeAt(std::size_t outlineIndex) const
    {
        return outline.at(outlineIndex);
    }
    [[nodiscard]] const FlatEdge& edge(core::EdgeSide side) const;
};

enum class ViewFace {
    Auto,   ///< the face with more machining (holes + grooves); Top on a tie
    Top,
    Bottom,
};

/// Unfolds a panel. Its drillings and grooves must be up to date (detailing applied).
[[nodiscard]] FlatPart flatten(const core::Panel& panel, const core::ConstructionRules& rules,
                               ViewFace face = ViewFace::Auto);

/// The unfolded part of a cut list row (its first component, view face chosen automatically).
/// Throws std::invalid_argument if the row's component is not in the product.
[[nodiscard]] FlatPart flatPartOf(const core::Product& product, const CutListRow& row);

/// True if the panel's local frame (length, width, thickness axes) is right-handed. Looking at
/// the Top face of such a panel you see local x to the right and local y up.
[[nodiscard]] bool isRightHanded(const core::Panel& panel) noexcept;

} // namespace rc::detailing
