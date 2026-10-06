#include "readycad/detailing/CutList.hpp"

#include "readycad/core/model/Queries.hpp"

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

bool samePart(const CutListRow& row, const CutListRow& candidate)
{
    constexpr Real kTol = 0.01;
    return row.kind == candidate.kind && row.name == candidate.name
        && row.material.name == candidate.material.name
        && core::nearlyEqual(row.thickness, candidate.thickness, kTol)
        && core::nearlyEqual(row.finishLength, candidate.finishLength, kTol)
        && core::nearlyEqual(row.finishWidth, candidate.finishWidth, kTol)
        && sameEdges(row.edges, candidate.edges);
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
    for (const core::FurnitureComponent* component : core::collectPanels(root)) {
        const core::Panel& panel = *component->panel();

        CutListRow candidate;
        candidate.kind = component->kind();
        candidate.name = component->name();
        candidate.material = panel.material();
        candidate.finishLength = panel.finishLength();
        candidate.finishWidth = panel.finishWidth();
        candidate.thickness = panel.thickness();
        candidate.cut = cutSize(panel, rules);
        candidate.edges = panel.edges();
        candidate.keepGrainDirection = panel.material().hasGrain;

        const auto existing = std::find_if(rows.begin(), rows.end(), [&](const CutListRow& row) {
            return samePart(row, candidate);
        });
        CutListRow& row = existing != rows.end() ? *existing : rows.emplace_back(candidate);
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
