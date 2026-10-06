#pragma once

#include "readycad/core/model/Product.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace rc::core {

/// Builder: assembles a standard case-furniture product (wardrobe, chest of drawers, bookcase)
/// from components, step by step, with sensible defaults:
///
///     auto wardrobe = WardrobeBuilder()
///                         .dimensions({1600, 2400, 600})
///                         .sections(2)
///                         .shelvesPerSection(4)
///                         .doorsPerSection(2)
///                         .drawers(2, 360.0)   // a 360 mm drawer compartment at the bottom
///                         .build();
///
/// The result is an ordinary tree: after build() everything (dimensions, shelves, doors) can be
/// edited and the product re-lays itself out.
class WardrobeBuilder {
public:
    WardrobeBuilder& name(std::string name);
    WardrobeBuilder& dimensions(const Size3D& dimensions);
    WardrobeBuilder& rules(ConstructionRules rules);
    WardrobeBuilder& joints(JointType top, JointType bottom);
    WardrobeBuilder& backMount(BackMountType type);

    /// Number of vertical columns, separated by partitions.
    WardrobeBuilder& sections(int count);
    /// Fixed clear width of one column (0-based index); the others share the rest.
    WardrobeBuilder& sectionWidth(int index, Real width);
    WardrobeBuilder& shelvesPerSection(int count);
    /// How the generated shelves are mounted (individual shelves can be changed later).
    WardrobeBuilder& shelfMount(ShelfMount mount);
    WardrobeBuilder& doorsPerSection(int count);
    /// `count` drawers in every column. With `compartmentHeight` they go into a bottom
    /// compartment of that clear height (fixed shelf above, shelves and doors above that);
    /// without it the drawers fill the whole column and there are no shelves or doors.
    WardrobeBuilder& drawers(int count, std::optional<Real> compartmentHeight = std::nullopt);

    /// Throws std::invalid_argument for inconsistent parameters.
    [[nodiscard]] std::shared_ptr<Product> build() const;

private:
    std::string name_;
    Size3D dimensions_{800.0, 2000.0, 560.0};
    ConstructionRules rules_;
    JointType topJoint_ = JointType::Overlay;
    JointType bottomJoint_ = JointType::Inset;
    BackMountType backMount_ = BackMountType::Groove;
    int sections_ = 1;
    std::map<int, Real> sectionWidths_;
    int shelves_ = 4;
    ShelfMount shelfMount_ = ShelfMount::Supports;
    int doors_ = 2;
    int drawers_ = 0;
    std::optional<Real> drawerCompartmentHeight_;
};

} // namespace rc::core
