#pragma once

#include "readycad/core/ConstructionRules.hpp"
#include "readycad/core/model/Carcass.hpp"
#include "readycad/core/model/FurnitureComponent.hpp"
#include "readycad/core/model/Kitchen.hpp"

#include <memory>
#include <string>

namespace rc::core {

/// Изделие — the root of a furniture tree. Owns the ConstructionRules and the overall size.
/// It holds ONE child: a Carcass (a wardrobe, a chest of drawers...) or a Kitchen (a run of
/// cabinets with a worktop).
///
/// Size convention: width/height/depth are those of the CARCASS (what fits into the niche);
/// fronts stand in front of it and add their thickness to the real depth. A kitchen: length of
/// the run, top line of the wall cabinets, depth of the base cabinets (see Kitchen).
class Product final : public FurnitureComponent {
public:
    explicit Product(std::string name = {}, ConstructionRules rules = {});
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::Product; }

    [[nodiscard]] const ConstructionRules& rules() const noexcept { return rules_; }
    void setRules(ConstructionRules rules);

    [[nodiscard]] const Size3D& dimensions() const noexcept { return dimensions_; }
    /// Changes the overall size and recomputes everything. Throws std::invalid_argument
    /// for non-positive dimensions.
    void setDimensions(const Size3D& dimensions);

    /// Recomputes the whole tree from the current dimensions and rules.
    void update();

    /// The product's carcass (nullptr for a kitchen or if none was added yet).
    [[nodiscard]] Carcass* carcass() const noexcept;
    /// The product's kitchen (nullptr for a single carcass).
    [[nodiscard]] Kitchen* kitchen() const noexcept;

    [[nodiscard]] bool canAccept(const FurnitureComponent& child) const override;

private:
    ConstructionRules rules_;
    Size3D dimensions_{};
};

} // namespace rc::core
