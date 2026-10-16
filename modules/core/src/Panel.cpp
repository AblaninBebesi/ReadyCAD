#include "readycad/core/Panel.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rc::core {

Panel::Panel(Material material, Axis thicknessAxis, Axis lengthAxis)
    : material_(std::move(material)), thicknessAxis_(thicknessAxis), lengthAxis_(lengthAxis)
{
    if (thicknessAxis == lengthAxis) {
        throw std::invalid_argument("Panel: thickness and length axes must differ");
    }
}

Axis Panel::widthAxis() const noexcept
{
    // The three axis indices sum to 0 + 1 + 2 = 3.
    return static_cast<Axis>(3 - index(thicknessAxis_) - index(lengthAxis_));
}

void Panel::fitTo(const Aabb& box)
{
    const Vec3 size = box.size();
    finishLength_ = size[index(lengthAxis_)];
    finishWidth_ = size[index(widthAxis())];
}

Real Panel::cutLength() const noexcept
{
    return finishLength_ - edge(EdgeSide::W1).thickness - edge(EdgeSide::W2).thickness;
}

Real Panel::cutWidth() const noexcept
{
    return finishWidth_ - edge(EdgeSide::L1).thickness - edge(EdgeSide::L2).thickness;
}

const EdgeBand& Panel::edge(EdgeSide side) const noexcept
{
    return edges_[static_cast<std::size_t>(side)];
}

void Panel::setEdge(EdgeSide side, EdgeBand band)
{
    edges_[static_cast<std::size_t>(side)] = std::move(band);
}

void Panel::setShape(std::vector<ShapeFeature> features, std::vector<PanelCutout> automaticOpenings)
{
    const bool shaped = std::any_of(features.begin(), features.end(),
                                    [](const ShapeFeature& f) { return changesOutline(f); });
    outline_ = shaped ? panelOutline(finishLength_, finishWidth_, features)
                      : std::vector<OutlineSide>{};
    cutouts_ = std::move(automaticOpenings);
    for (const PanelCutout& opening : featureOpenings(finishLength_, finishWidth_, features)) {
        cutouts_.push_back(opening);
    }
    shape_ = std::move(features);
}

} // namespace rc::core
