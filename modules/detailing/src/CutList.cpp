#include "readycad/detailing/CutList.hpp"

#include "readycad/core/model/PanelComponent.hpp"
#include "readycad/core/model/Parts.hpp"
#include "readycad/core/model/Queries.hpp"
#include "readycad/detailing/FlatPart.hpp"

#include <algorithm>

namespace rc::detailing {

using core::EdgeBand;
using core::EdgeSide;
using core::Real;

namespace {

Real allowanceFor(const EdgeBand& a, const EdgeBand& b, const core::ConstructionRules& rules)
{
    Real result = 0.0;
    for (const EdgeBand* band : {&a, &b}) {
        if (band->present()) {
            result += rules.cutting.premillAllowance - band->thickness;
        }
    }
    return result;
}

Real rounded(Real value, Real step)
{
    return step > 0.0 ? core::roundToStep(value, step) : value;
}

bool sameEdges(const std::array<EdgeBand, 4>& a, const std::array<EdgeBand, 4>& b)
{
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].name != b[i].name || !core::nearlyEqual(a[i].thickness, b[i].thickness)) {
            return false;
        }
    }
    return true;
}

constexpr Real kTol = 0.01;

bool samePart(const CutListRow& row, const CutListRow& candidate)
{
    return row.kind == candidate.kind && row.name == candidate.name && row.hand == candidate.hand
        && row.material.name == candidate.material.name
        && core::nearlyEqual(row.thickness, candidate.thickness, kTol)
        && core::nearlyEqual(row.finishLength, candidate.finishLength, kTol)
        && core::nearlyEqual(row.finishWidth, candidate.finishWidth, kTol)
        && sameEdges(row.edges, candidate.edges) && row.inCuttingPlan == candidate.inCuttingPlan;
}

std::optional<core::Side> handOf(const core::FurnitureComponent& component)
{
    switch (component.kind()) {
    case core::ComponentKind::SideWall:
        return static_cast<const core::SideWall&>(component).side();
    case core::ComponentKind::DrawerBoxSide:
        return static_cast<const core::DrawerBoxSide&>(component).side();
    case core::ComponentKind::Door:
        return static_cast<const core::Door&>(component).hinge() == core::HingeSide::Left
                   ? core::Side::Left
                   : core::Side::Right;
    default:
        return std::nullopt;
    }
}

bool near(core::Vec2 a, core::Vec2 b)
{
    return core::nearlyEqual(a.x, b.x, kTol) && core::nearlyEqual(a.y, b.y, kTol);
}

bool sameHole(const FlatHole& a, const FlatHole& b)
{
    return a.fastener == b.fastener && a.side == b.side
        && (a.side != FlatSide::Edge || a.edge == b.edge) && near(a.center, b.center)
        && core::nearlyEqual(a.diameter, b.diameter, kTol)
        && core::nearlyEqual(a.depth, b.depth, kTol) && core::nearlyEqual(a.height, b.height, kTol);
}

bool sameGroove(const FlatGroove& a, const FlatGroove& b)
{
    const bool sameLine = (near(a.start, b.start) && near(a.end, b.end))
                       || (near(a.start, b.end) && near(a.end, b.start));
    return a.side == b.side && sameLine && core::nearlyEqual(a.width, b.width, kTol)
        && core::nearlyEqual(a.depth, b.depth, kTol);
}

/// Every element of `a` has its own equal element in `b` (order-free: the sort order of holes
/// may differ by rounding noise).
template <typename T, typename Equal>
bool sameSet(const std::vector<T>& a, const std::vector<T>& b, Equal equal)
{
    if (a.size() != b.size()) {
        return false;
    }
    std::vector<char> used(b.size(), 0);
    for (const T& item : a) {
        bool found = false;
        for (std::size_t i = 0; i < b.size() && !found; ++i) {
            if (used[i] == 0 && equal(item, b[i])) {
                used[i] = 1;
                found = true;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool sameCutout(const FlatCutout& a, const FlatCutout& b)
{
    return near(a.center, b.center) && core::nearlyEqual(a.length, b.length, kTol)
        && core::nearlyEqual(a.width, b.width, kTol) && core::nearlyEqual(a.radius, b.radius, kTol);
}

/// The same holes, grooves and cut-outs as seen in the drawing of the part.
bool sameMachining(const FlatPart& a, const FlatPart& b)
{
    return sameSet(a.holes, b.holes, sameHole) && sameSet(a.grooves, b.grooves, sameGroove)
        && sameSet(a.cutouts, b.cutouts, sameCutout);
}

} // namespace

CutSize cutSize(const core::Panel& panel, const core::ConstructionRules& rules)
{
    // Edges on the short ends (W1/W2) change the length, edges on the long sides the width.
    const Real length = panel.finishLength()
                      + allowanceFor(panel.edge(EdgeSide::W1), panel.edge(EdgeSide::W2), rules);
    const Real width = panel.finishWidth()
                     + allowanceFor(panel.edge(EdgeSide::L1), panel.edge(EdgeSide::L2), rules);
    return {rounded(length, rules.cutting.rounding), rounded(width, rules.cutting.rounding)};
}

std::vector<CutListRow> buildCutList(const core::FurnitureComponent& root,
                                     const core::ConstructionRules& rules)
{
    std::vector<CutListRow> rows;
    std::vector<FlatPart> machining; // of each row's first part: what its drawing shows
    for (const core::FurnitureComponent* component : core::collectPanels(root)) {
        const core::Panel& panel = *component->panel();

        CutListRow candidate;
        candidate.kind = component->kind();
        candidate.name = component->name();
        candidate.hand = handOf(*component);
        candidate.material = panel.material();
        candidate.finishLength = panel.finishLength();
        candidate.finishWidth = panel.finishWidth();
        candidate.thickness = panel.thickness();
        candidate.cut = cutSize(panel, rules);
        candidate.edges = panel.edges();
        candidate.keepGrainDirection = panel.material().hasGrain;
        const auto* part = dynamic_cast<const core::PanelComponent*>(component);
        candidate.inCuttingPlan = part == nullptr || part->inCuttingPlan();

        FlatPart flat = flatten(panel, rules);
        std::size_t index = 0;
        while (index < rows.size()
               && !(samePart(rows[index], candidate) && sameMachining(machining[index], flat))) {
            ++index;
        }
        if (index == rows.size()) {
            rows.push_back(candidate);
            machining.push_back(std::move(flat));
        }
        CutListRow& row = rows[index];
        ++row.quantity;
        row.components.push_back(component->id());
    }
    return rows;
}

std::vector<EdgeBandUsage> edgeBandUsage(const std::vector<CutListRow>& cutList,
                                         const core::ConstructionRules& rules)
{
    std::vector<EdgeBandUsage> usage;
    for (const CutListRow& row : cutList) {
        for (std::size_t side = 0; side < row.edges.size(); ++side) {
            const EdgeBand& band = row.edges[side];
            if (!band.present()) {
                continue;
            }
            // L1/L2 run along the length, W1/W2 along the width.
            const auto edgeSide = static_cast<EdgeSide>(side);
            const bool alongLength = edgeSide == EdgeSide::L1 || edgeSide == EdgeSide::L2;
            const Real piece =
                (alongLength ? row.finishLength : row.finishWidth) + rules.cutting.edgeTrimAllowance;

            auto it = std::find_if(usage.begin(), usage.end(),
                                   [&band](const EdgeBandUsage& u) { return u.band == band; });
            EdgeBandUsage& entry = it != usage.end() ? *it : usage.emplace_back(EdgeBandUsage{band});
            entry.totalLength += piece * row.quantity;
            entry.pieces += row.quantity;
        }
    }
    return usage;
}

} // namespace rc::detailing
