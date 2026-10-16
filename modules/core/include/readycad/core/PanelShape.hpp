#pragma once

#include "readycad/core/Math.hpp"

#include <array>
#include <variant>
#include <vector>

namespace rc::core {

/// Edges of a rectangular panel in its own 2D system (length along local X, width along local Y):
///
///            L2 (long edge at width = max)
///        +---------------------------+
///    W1  |                           |  W2    (short edges at length = 0 / length = max)
///        +---------------------------+
///            L1 (long edge at width = 0)
///
/// Edges on L1/L2 make the part WIDER, edges on W1/W2 make it LONGER (see cut sizes).
enum class EdgeSide { L1 = 0, L2 = 1, W1 = 2, W2 = 3 };

/// Corners of a panel by the two edges that meet there, counter-clockwise from the origin:
/// L1W1 (0, 0), L1W2 (length, 0), L2W2 (length, width), L2W1 (0, width).
enum class PanelCorner { L1W1, L1W2, L2W2, L2W1 };

/// An opening cut through the whole panel — the hole for a sink in a worktop, a cable hole: a
/// rectangle with rounded corners in panel x/y coordinates (x along the length). A round hole is
/// one whose radius is half its size.
struct PanelCutout {
    Vec2 center{0.0};
    Real length = 0.0; // along x
    Real width = 0.0;  // along y
    Real radius = 0.0; // of the corners

    friend bool operator==(const PanelCutout&, const PanelCutout&) = default;
};

// =============================================================================================
// Shape features: changes of a panel's shape the user asks for. Each one is bound to the edges
// of the panel, so it keeps its place when the panel changes its size.
// =============================================================================================

/// Köşe kesimi: a rectangle taken out of a corner (a notch for the skirting board).
struct CornerNotch {
    PanelCorner corner = PanelCorner::L1W1;
    Real alongLength = 20.0; // size along the panel's length
    Real alongWidth = 100.0; // size along its width

    friend bool operator==(const CornerNotch&, const CornerNotch&) = default;
};

/// Köşe yuvarlatma: a rounded corner.
struct CornerRound {
    PanelCorner corner = PanelCorner::L1W1;
    Real radius = 50.0;

    friend bool operator==(const CornerRound&, const CornerRound&) = default;
};

/// Köşe pahı: the corner cut off in a straight line.
struct CornerChamfer {
    PanelCorner corner = PanelCorner::L1W1;
    Real alongLength = 50.0;
    Real alongWidth = 50.0;

    friend bool operator==(const CornerChamfer&, const CornerChamfer&) = default;
};

/// Kenar kesimi: a rectangular notch in one edge (around a pipe).
struct EdgeNotch {
    EdgeSide edge = EdgeSide::L1;
    /// The offset is measured from the edge's corner with W1 (an L edge) or with L1 (a W edge);
    /// fromEnd: from its other corner.
    bool fromEnd = false;
    Real offset = 100.0; // from the corner to the notch
    Real length = 100.0; // along the edge
    Real depth = 30.0;   // into the panel

    friend bool operator==(const EdgeNotch&, const EdgeNotch&) = default;
};

/// İç kesim: an opening inside the panel, a rectangle with rounded corners. Its centre is
/// measured from the edges of the corner `from`.
struct InnerCutout {
    PanelCorner from = PanelCorner::L1W1;
    Real alongLength = 200.0; // the centre from the corner's W edge
    Real alongWidth = 100.0;  // the centre from the corner's L edge
    Real length = 100.0;
    Real width = 60.0;
    Real radius = 5.0;

    friend bool operator==(const InnerCutout&, const InnerCutout&) = default;
};

/// Yuvarlak delik: a round opening (a cable hole), its centre measured like an InnerCutout's.
struct RoundHole {
    PanelCorner from = PanelCorner::L1W1;
    Real alongLength = 100.0;
    Real alongWidth = 100.0;
    Real diameter = 60.0;

    friend bool operator==(const RoundHole&, const RoundHole&) = default;
};

using ShapeFeature =
    std::variant<CornerNotch, CornerRound, CornerChamfer, EdgeNotch, InnerCutout, RoundHole>;

/// True for features that change the outline (corners, edge notches), false for openings.
[[nodiscard]] bool changesOutline(const ShapeFeature& feature) noexcept;

/// What a feature must have whatever the panel: sizes > 0 (offsets >= 0), corners rounder than
/// the opening is not. Throws std::invalid_argument.
void checkFeature(const ShapeFeature& feature);

// =============================================================================================
// The outline that results
// =============================================================================================

enum class OutlineSideKind {
    Edge,   ///< a piece of one of the four edges
    Corner, ///< a rounded or chamfered corner: banded like its edges
    Notch,  ///< a side of a notch: not banded (an edgebander cannot follow inner corners)
};

/// One side of an outline: from `start` to the next side's start.
struct OutlineSide {
    Vec2 start{0.0};
    OutlineSideKind kind = OutlineSideKind::Edge;
    EdgeSide edge = EdgeSide::L1;           ///< for an Edge side
    PanelCorner corner = PanelCorner::L1W1; ///< for a Corner side

    friend bool operator==(const OutlineSide&, const OutlineSide&) = default;
};

/// The outline of a panel `length` x `width` with its features: counter-clockwise in panel
/// coordinates, starting at the corner L1W1. Without corner and edge features, the four edges.
/// `grow` (by EdgeSide): how far each edge line lies outside the finished one — the sawn blank
/// before banding (pre-milling allowance minus the band); the features keep their finished
/// place. A rounded corner becomes `segments` straight pieces.
[[nodiscard]] std::vector<OutlineSide> panelOutline(Real length, Real width,
                                                    const std::vector<ShapeFeature>& features,
                                                    const std::array<Real, 4>& grow = {},
                                                    int segments = 8);

/// The points of an outline.
[[nodiscard]] std::vector<Vec2> outlinePoints(const std::vector<OutlineSide>& outline);

/// The openings of the features (inner cut-outs, round holes) in panel coordinates.
[[nodiscard]] std::vector<PanelCutout> featureOpenings(Real length, Real width,
                                                       const std::vector<ShapeFeature>& features);

/// The outline of an opening, counter-clockwise, starting at the lower end of its right side;
/// `segments` straight pieces per rounded corner.
[[nodiscard]] std::vector<Vec2> openingOutline(const PanelCutout& opening, int segments = 8);

/// Can the features be made on a panel of this size, with these openings (its own and the ones
/// it gets from elsewhere, like a worktop's)? At most one feature per corner; the outline does
/// not cut through itself (features meeting, a notch as deep as the panel); every opening lies
/// inside the panel and keeps `bridge` of material to the outline and to the other openings.
[[nodiscard]] bool shapeFits(Real length, Real width, const std::vector<ShapeFeature>& features,
                             const std::vector<PanelCutout>& openings, Real bridge);

} // namespace rc::core
