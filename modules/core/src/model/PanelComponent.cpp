#include "readycad/core/model/PanelComponent.hpp"

#include <utility>

namespace rc::core {

PanelComponent::PanelComponent(Axis thicknessAxis, Axis lengthAxis)
{
    // The real material is assigned on the first layout (it comes from the product's rules).
    panel_.emplace(Material{}, thicknessAxis, lengthAxis);
}

void PanelComponent::setMaterialOverride(std::optional<Material> material)
{
    materialOverride_ = std::move(material);
    requestLayout(); // thickness may change -> neighbours move
}

void PanelComponent::setEdgeOverride(EdgeSide side, std::optional<EdgeBand> band)
{
    edgeOverrides_[static_cast<std::size_t>(side)] = std::move(band);
    requestLayout();
}

EdgeBand PanelComponent::defaultEdge(EdgeSide, const LayoutSlot&) const
{
    return {};
}

Real PanelComponent::thickness(const LayoutSlot& slot) const
{
    return materialOverride_ ? materialOverride_->thickness
                             : defaultMaterial(slot.r()).thickness;
}

void PanelComponent::updatePanel(const LayoutSlot& slot)
{
    panel_->setMaterial(materialOverride_ ? *materialOverride_ : defaultMaterial(slot.r()));
    panel_->fitTo(box());
    // Holes and grooves depend on the geometry: stale after any relayout, the detailing
    // module regenerates them.
    panel_->clearMachining();
    for (const EdgeSide side : {EdgeSide::L1, EdgeSide::L2, EdgeSide::W1, EdgeSide::W2}) {
        const auto& custom = edgeOverrides_[static_cast<std::size_t>(side)];
        panel_->setEdge(side, custom ? *custom : defaultEdge(side, slot));
    }
}

} // namespace rc::core
