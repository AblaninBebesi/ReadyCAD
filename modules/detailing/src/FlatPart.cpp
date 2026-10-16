#include "readycad/detailing/FlatPart.hpp"

#include <glm/trigonometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <tuple>

namespace rc::detailing {

using core::EdgeSide;
using core::PanelFace;
using core::Real;
using core::Vec2;

namespace {

/// How the panel's local coordinates map into the view.
struct ViewMapping {
    Real width = 0.0;
    Real thickness = 0.0;
    PanelFace viewFace = PanelFace::Top;
    bool flipped = false;

    [[nodiscard]] Vec2 point(Real x, Real y) const noexcept
    {
        return {x, flipped ? width - y : y};
    }
    /// Local y direction (+1 / -1) as seen in the view.
    [[nodiscard]] Real yDirection() const noexcept { return flipped ? -1.0 : 1.0; }
    /// Distance from the view face of a point at local z.
    [[nodiscard]] Real heightOf(Real z) const noexcept
    {
        return viewFace == PanelFace::Top ? thickness - z : z;
    }
};

PanelFace chooseFace(const core::Panel& panel, ViewFace choice)
{
    switch (choice) {
    case ViewFace::Top:    return PanelFace::Top;
    case ViewFace::Bottom: return PanelFace::Bottom;
    case ViewFace::Auto:   break;
    }
    int top = 0;
    int bottom = 0;
    for (const core::Drilling& d : panel.drillings()) {
        top += d.face == PanelFace::Top ? 1 : 0;
        bottom += d.face == PanelFace::Bottom ? 1 : 0;
    }
    for (const core::Groove& g : panel.grooves()) {
        top += g.face == PanelFace::Top ? 1 : 0;
        bottom += g.face == PanelFace::Bottom ? 1 : 0;
    }
    return bottom > top ? PanelFace::Bottom : PanelFace::Top;
}

/// Material removed (negative) or left (positive) on one side of the blank compared to the
/// finished part: the band is glued on after pre-milling.
Real cutDelta(const core::EdgeBand& band, const core::ConstructionRules& rules)
{
    return band.present() ? rules.cutting.premillAllowance - band.thickness : 0.0;
}

FlatHole flattenHole(const core::Drilling& d, const ViewMapping& view)
{
    FlatHole hole;
    hole.fastener = d.fastener;
    hole.diameter = d.diameter;
    hole.depth = d.depth;
    hole.center = view.point(d.position.x, d.position.y);
    switch (d.face) {
    case PanelFace::Top:
    case PanelFace::Bottom:
        hole.side = d.face == view.viewFace ? FlatSide::Near : FlatSide::Far;
        return hole;
    case PanelFace::EdgeL1:
        hole.edge = EdgeSide::L1;
        hole.direction = {0.0, view.yDirection()};
        break;
    case PanelFace::EdgeL2:
        hole.edge = EdgeSide::L2;
        hole.direction = {0.0, -view.yDirection()};
        break;
    case PanelFace::EdgeW1:
        hole.edge = EdgeSide::W1;
        hole.direction = {1.0, 0.0};
        break;
    case PanelFace::EdgeW2:
        hole.edge = EdgeSide::W2;
        hole.direction = {-1.0, 0.0};
        break;
    }
    hole.side = FlatSide::Edge;
    hole.height = view.heightOf(d.position.z);
    return hole;
}

} // namespace

std::size_t FlatPart::count(FlatSide side) const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        holes.begin(), holes.end(), [side](const FlatHole& h) { return h.side == side; }));
}

const FlatEdge& FlatPart::edge(EdgeSide side) const
{
    for (const FlatEdge& e : outline) {
        if (e.side == side) {
            return e;
        }
    }
    throw std::logic_error("FlatPart: outline has no such edge");
}

std::vector<Vec2> cutoutOutline(const FlatCutout& cutout, int segments)
{
    return core::openingOutline({cutout.center, cutout.length, cutout.width, cutout.radius},
                                segments);
}

bool FlatPart::shaped() const noexcept
{
    return contour.size() != 4
        || std::any_of(contour.begin(), contour.end(), [](const FlatContourSide& s) {
               return s.kind != core::OutlineSideKind::Edge;
           });
}

bool isRightHanded(const core::Panel& panel) noexcept
{
    // (length, width, thickness) is an even permutation of (X, Y, Z) exactly when every axis is
    // followed by the next one: XYZ, YZX, ZXY.
    const int l = core::index(panel.lengthAxis());
    const int w = core::index(panel.widthAxis());
    const int t = core::index(panel.thicknessAxis());
    return (w - l + 3) % 3 == 1 && (t - w + 3) % 3 == 1;
}

FlatPart flatten(const core::Panel& panel, const core::ConstructionRules& rules, ViewFace choice)
{
    ViewMapping view;
    view.width = panel.finishWidth();
    view.thickness = panel.thickness();
    view.viewFace = chooseFace(panel, choice);
    // Looking at the Top face of a right-handed panel shows local y going up; the Bottom face
    // (seen from the other side) or a left-handed frame shows it going down: flip it back.
    view.flipped = isRightHanded(panel) != (view.viewFace == PanelFace::Top);

    FlatPart part;
    part.length = panel.finishLength();
    part.width = panel.finishWidth();
    part.thickness = panel.thickness();
    part.viewFace = view.viewFace;
    part.flipped = view.flipped;

    const Real l = part.length;
    const Real w = part.width;
    const EdgeSide bottomEdge = view.flipped ? EdgeSide::L2 : EdgeSide::L1;
    const EdgeSide topEdge = view.flipped ? EdgeSide::L1 : EdgeSide::L2;
    const auto edgeOf = [&](EdgeSide side, Vec2 a, Vec2 b) {
        return FlatEdge{side, panel.edge(side), a, b};
    };
    part.outline = {
        edgeOf(bottomEdge, {0.0, 0.0}, {l, 0.0}),
        edgeOf(EdgeSide::W2, {l, 0.0}, {l, w}),
        edgeOf(topEdge, {l, w}, {0.0, w}),
        edgeOf(EdgeSide::W1, {0.0, w}, {0.0, 0.0}),
    };

    part.cut.min = {-cutDelta(panel.edge(EdgeSide::W1), rules),
                    -cutDelta(panel.edge(bottomEdge), rules)};
    part.cut.max = {l + cutDelta(panel.edge(EdgeSide::W2), rules),
                    w + cutDelta(panel.edge(topEdge), rules)};

    part.holes.reserve(panel.drillings().size());
    for (const core::Drilling& d : panel.drillings()) {
        part.holes.push_back(flattenHole(d, view));
    }
    std::stable_sort(part.holes.begin(), part.holes.end(),
                     [](const FlatHole& a, const FlatHole& b) {
                         return std::tuple(a.side, a.center.y, a.center.x)
                              < std::tuple(b.side, b.center.y, b.center.x);
                     });

    for (const core::Groove& g : panel.grooves()) {
        FlatGroove groove;
        groove.side = g.face == view.viewFace ? FlatSide::Near : FlatSide::Far;
        groove.start = view.point(g.start.x, g.start.y);
        groove.end = view.point(g.end.x, g.end.y);
        groove.width = g.width;
        groove.depth = g.depth;
        part.grooves.push_back(groove);
    }
    for (const core::PanelCutout& c : panel.cutouts()) {
        part.cutouts.push_back({view.point(c.center.x, c.center.y), c.length, c.width, c.radius});
    }

    // The outline and the blank, with the shape features. Seen through a flipped view the
    // panel's counter-clockwise order turns clockwise: walk it backwards.
    const auto bandOf = [&panel](const core::OutlineSide& side) -> core::EdgeBand {
        switch (side.kind) {
        case core::OutlineSideKind::Edge:
            return panel.edge(side.edge);
        case core::OutlineSideKind::Corner: {
            const bool l1 = side.corner == core::PanelCorner::L1W1 || side.corner == core::PanelCorner::L1W2;
            const bool w1 = side.corner == core::PanelCorner::L1W1 || side.corner == core::PanelCorner::L2W1;
            const core::EdgeBand& along = panel.edge(l1 ? EdgeSide::L1 : EdgeSide::L2);
            return along.present() ? along : panel.edge(w1 ? EdgeSide::W1 : EdgeSide::W2);
        }
        case core::OutlineSideKind::Notch:
            break;
        }
        return {};
    };
    const std::vector<core::OutlineSide> finished =
        core::panelOutline(l, w, panel.shape());
    std::array<Real, 4> grow{};
    for (const EdgeSide side : {EdgeSide::L1, EdgeSide::L2, EdgeSide::W1, EdgeSide::W2}) {
        grow[static_cast<std::size_t>(side)] = cutDelta(panel.edge(side), rules);
    }
    const std::vector<core::OutlineSide> grown = core::panelOutline(l, w, panel.shape(), grow);
    const std::size_t n = finished.size();
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t i = view.flipped ? n - 1 - k : k;
        const Vec2 a = view.point(finished[i].start.x, finished[i].start.y);
        const Vec2 b = view.point(finished[(i + 1) % n].start.x, finished[(i + 1) % n].start.y);
        part.contour.push_back(
            {view.flipped ? b : a, view.flipped ? a : b, finished[i].kind, bandOf(finished[i])});
    }
    for (std::size_t k = 0; k < grown.size(); ++k) {
        const std::size_t i = view.flipped ? grown.size() - 1 - k : k;
        part.blank.push_back(view.point(grown[i].start.x, grown[i].start.y));
    }
    return part;
}

FlatPart flatPartOf(const core::Product& product, const CutListRow& row)
{
    if (row.components.empty()) {
        throw std::invalid_argument("cut list row without components");
    }
    const core::FurnitureComponent* component = product.find(row.components.front());
    if (component == nullptr || !component->hasPanel()) {
        throw std::invalid_argument("cut list row does not belong to this product");
    }
    return flatten(*component->panel(), product.rules());
}

} // namespace rc::detailing
