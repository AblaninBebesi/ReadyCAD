#include "readycad/detailing/ShapeCheck.hpp"

#include "readycad/core/Polygon.hpp"
#include "readycad/core/model/Queries.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace rc::detailing {

using core::PanelFace;
using core::Real;
using core::Vec2;

namespace {

constexpr Real kTol = 0.01;

/// The part as material: its outline and its openings, in panel coordinates.
struct Material {
    std::vector<Vec2> outline;
    std::vector<std::vector<Vec2>> openings;

    /// The point lies in material with `clearance` to every boundary (0: on a boundary is fine).
    [[nodiscard]] bool solidAt(Vec2 p, Real clearance) const
    {
        const bool inOutline = core::contains(outline, p)
                            || core::distanceToOutline(p, outline) <= kTol;
        if (!inOutline || core::distanceToOutline(p, outline) < clearance - kTol) {
            return false;
        }
        return std::none_of(openings.begin(), openings.end(), [&](const auto& hole) {
            return core::contains(hole, p) || core::distanceToOutline(p, hole) < clearance - kTol;
        });
    }
};

/// The edge a hole in an edge face enters through, and the direction into the panel.
bool edgeOf(PanelFace face, core::EdgeSide& edge, Vec2& inward)
{
    switch (face) {
    case PanelFace::EdgeL1: edge = core::EdgeSide::L1; inward = {0.0, 1.0}; return true;
    case PanelFace::EdgeL2: edge = core::EdgeSide::L2; inward = {0.0, -1.0}; return true;
    case PanelFace::EdgeW1: edge = core::EdgeSide::W1; inward = {1.0, 0.0}; return true;
    case PanelFace::EdgeW2: edge = core::EdgeSide::W2; inward = {-1.0, 0.0}; return true;
    case PanelFace::Top:
    case PanelFace::Bottom:
        break;
    }
    return false;
}

bool machiningFits(const core::Panel& panel, const Material& m,
                   const std::vector<core::OutlineSide>& outline)
{
    for (const core::Drilling& d : panel.drillings()) {
        const Vec2 at(d.position.x, d.position.y);
        const Real r = d.diameter / 2.0;
        core::EdgeSide edge{};
        Vec2 inward{};
        if (!edgeOf(d.face, edge, inward)) {
            if (!m.solidAt(at, r)) {
                return false;
            }
            continue;
        }
        // An edge hole enters through a piece of its edge that is still there...
        bool entered = false;
        const std::size_t n = outline.size();
        for (std::size_t i = 0; i < n && !entered; ++i) {
            const core::OutlineSide& side = outline[i];
            const Vec2 a = side.start;
            const Vec2 b = outline[(i + 1) % n].start;
            entered = side.kind == core::OutlineSideKind::Edge && side.edge == edge
                   && core::distanceToSegment(at, a, b) <= kTol
                   && glm::length(at - a) >= r - kTol && glm::length(at - b) >= r - kTol;
        }
        // ...and ends in material.
        if (!entered || !m.solidAt(at + inward * d.depth, 0.0)) {
            return false;
        }
    }
    for (const core::Groove& g : panel.grooves()) {
        const Real length = glm::length(g.end - g.start);
        const int steps = std::max(1, static_cast<int>(std::ceil(length / 5.0)));
        for (int i = 0; i <= steps; ++i) {
            if (!m.solidAt(g.start + (g.end - g.start) * (static_cast<Real>(i) / steps), 0.0)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

std::vector<ShapeConflict> findShapeConflicts(const core::Product& product)
{
    std::vector<ShapeConflict> conflicts;
    const core::ConstructionRules& rules = product.rules();
    for (const core::FurnitureComponent* part : core::collectPanels(product)) {
        const core::Panel& panel = *part->panel();
        if (panel.shape().empty() && panel.cutouts().empty()) {
            continue; // a plain rectangle: detailing places everything inside it
        }
        const Real l = panel.finishLength();
        const Real w = panel.finishWidth();
        if (!core::shapeFits(l, w, panel.shape(), panel.cutouts(), rules.minBridge)) {
            conflicts.push_back({part, ShapeConflict::Reason::DoesNotFit});
            continue;
        }
        const std::vector<core::OutlineSide> outline = core::panelOutline(l, w, panel.shape());
        Material m;
        m.outline = core::outlinePoints(outline);
        for (const core::PanelCutout& opening : panel.cutouts()) {
            m.openings.push_back(core::openingOutline(opening));
        }
        if (!machiningFits(panel, m, outline)) {
            conflicts.push_back({part, ShapeConflict::Reason::MachiningInCut});
        }
    }
    return conflicts;
}

} // namespace rc::detailing
