#pragma once

#include "readycad/core/model/FurnitureComponent.hpp"

#include <array>
#include <optional>
#include <vector>

namespace rc::core {

/// Base for leaf components that are exactly one physical panel (side wall, shelf, door, ...).
///
/// Subclasses supply:
///  * updateSize / updatePosition — the part's own geometric formula;
///  * defaultMaterial             — which material of the ConstructionRules it is made of;
///  * defaultEdge                 — which edges get banding by default.
/// The user can override material and individual edges per part; overrides survive relayout.
class PanelComponent : public FurnitureComponent {
public:
    /// Material actually used (override or the rules' default).
    [[nodiscard]] const Material& material() const noexcept { return panel_->material(); }
    void setMaterialOverride(std::optional<Material> material);
    [[nodiscard]] const std::optional<Material>& materialOverride() const noexcept
    {
        return materialOverride_;
    }

    void setEdgeOverride(EdgeSide side, std::optional<EdgeBand> band);
    [[nodiscard]] const std::optional<EdgeBand>& edgeOverride(EdgeSide side) const noexcept
    {
        return edgeOverrides_[static_cast<std::size_t>(side)];
    }

    /// Whether the workshop cuts this part from its own sheets (the default). Off, the part stays
    /// in the cut list and the documents but not in the cutting plan and the CNC programs: a
    /// worktop or a drawer bottom ordered cut to size, a part cut by another workshop.
    [[nodiscard]] bool inCuttingPlan() const noexcept { return inCuttingPlan_; }
    void setInCuttingPlan(bool value) noexcept { inCuttingPlan_ = value; }

    /// Changes of the part's shape: notches, rounded or chamfered corners, openings. Bound to the
    /// part's edges, they follow it when it changes size. Throws std::invalid_argument for a
    /// feature with an impossible size (whether it fits is checked by the editor).
    [[nodiscard]] const std::vector<ShapeFeature>& shape() const noexcept { return shape_; }
    void setShape(std::vector<ShapeFeature> features);

    /// Thickness used in the formulas: that of the effective material under the slot's rules.
    /// Public because parents need it BEFORE laying the part out (to reserve its band).
    [[nodiscard]] Real thickness(const LayoutSlot& slot) const;

    [[nodiscard]] bool canAccept(const FurnitureComponent&) const override { return false; }

protected:
    PanelComponent(Axis thicknessAxis, Axis lengthAxis);

    [[nodiscard]] virtual const Material& defaultMaterial(const ConstructionRules& rules) const = 0;
    [[nodiscard]] virtual EdgeBand defaultEdge(EdgeSide side, const LayoutSlot& slot) const;
    /// Openings the part gets from where it stands (a worktop: the sink and the hob of the
    /// cabinets below). Default: none.
    [[nodiscard]] virtual std::vector<PanelCutout> automaticOpenings(const LayoutSlot& slot) const;

    void updatePanel(const LayoutSlot& slot) override;

private:
    std::optional<Material> materialOverride_;
    std::array<std::optional<EdgeBand>, 4> edgeOverrides_{};
    bool inCuttingPlan_ = true;
    std::vector<ShapeFeature> shape_;
};

} // namespace rc::core
