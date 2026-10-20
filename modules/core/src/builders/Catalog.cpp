#include "readycad/core/builders/Catalog.hpp"

#include "readycad/core/builders/KitchenBuilder.hpp"
#include "readycad/core/builders/WardrobeBuilder.hpp"

#include <array>
#include <stdexcept>

namespace rc::core {

std::span<const CatalogItem> catalogItems() noexcept
{
    static constexpr std::array kItems = {
        CatalogItem::Wardrobe,
        CatalogItem::WardrobeWithDrawers,
        CatalogItem::ChestOfDrawers,
        CatalogItem::Bookcase,
        CatalogItem::Kitchen,
        CatalogItem::KitchenL,
        CatalogItem::KitchenU,
    };
    return kItems;
}

std::shared_ptr<Product> createProduct(CatalogItem item, const ConstructionRules& rules)
{
    WardrobeBuilder builder;
    builder.rules(rules);

    switch (item) {
    case CatalogItem::Wardrobe:
        return builder.dimensions({800, 2000, 560}).shelvesPerSection(4).doorsPerSection(2).build();

    case CatalogItem::WardrobeWithDrawers:
        return builder.dimensions({1600, 2200, 600})
            .sections(2)
            .shelvesPerSection(3)
            .doorsPerSection(2)
            .drawers(2, 400.0)
            .build();

    case CatalogItem::ChestOfDrawers:
        return builder.dimensions({800, 850, 450})
            .shelvesPerSection(0)
            .doorsPerSection(0)
            .drawers(4)
            .build();

    case CatalogItem::Bookcase:
        return builder.dimensions({900, 1800, 320})
            .sections(2)
            .shelvesPerSection(4)
            .doorsPerSection(0)
            .backMount(BackMountType::Overlay)
            .build();

    case CatalogItem::Kitchen:
        // 2.4 m. The sink and the hob are not at the ends: the worktop needs material around their
        // cut-outs; the drawers keep them apart. The last cabinet takes whatever length is left
        // when the run is resized.
        return KitchenBuilder()
            .rules(rules)
            .length(2400)
            .add(CabinetModule::BaseDoors, 400)
            .add(CabinetModule::Sink, 600)
            .add(CabinetModule::BaseDrawers, 400)
            .add(CabinetModule::Hob, 600)
            .add(CabinetModule::BaseDoors)
            .add(CabinetModule::WallDoors, 400)
            .add(CabinetModule::WallDoors, 600)
            .add(CabinetModule::WallDoors, 400)
            .add(CabinetModule::WallDoors, 600)
            .add(CabinetModule::WallDoors)
            .build();

    case CatalogItem::KitchenL:
        // 3 m on the back wall, 2.4 m on the right one. The hob is not the first cabinet of the
        // side run: its worktop starts there, at the back run's edge, and needs material around
        // the cut-out.
        return KitchenBuilder()
            .rules(rules)
            .length(3000)
            .sideWall(Side::Right, 2400)
            .add(CabinetModule::BaseDoors, 400)
            .add(CabinetModule::Sink, 600)
            .add(CabinetModule::BaseDrawers, 400)
            .add(CabinetModule::BaseDoors)
            .add(CabinetModule::WallDoors, 600)
            .add(CabinetModule::WallDoors, 600)
            .add(CabinetModule::WallDoors)
            .add(CabinetModule::BaseDrawers, 400, KitchenWall::Right)
            .add(CabinetModule::Hob, 600, KitchenWall::Right)
            .add(CabinetModule::BaseDoors, std::nullopt, KitchenWall::Right)
            .add(CabinetModule::WallDoors, 600, KitchenWall::Right)
            .add(CabinetModule::WallDoors, std::nullopt, KitchenWall::Right)
            .build();

    case CatalogItem::KitchenU:
        // 3.2 m between two side walls of 2 m: the sink in the middle of the back wall, the
        // drawers at the left, the hob at the right.
        return KitchenBuilder()
            .rules(rules)
            .length(3200)
            .sideWall(Side::Left, 2000)
            .sideWall(Side::Right, 2000)
            .add(CabinetModule::Sink, 600)
            .add(CabinetModule::BaseDoors)
            .add(CabinetModule::WallDoors)
            .add(CabinetModule::WallDoors)
            .add(CabinetModule::BaseDoors, std::nullopt, KitchenWall::Left)
            .add(CabinetModule::BaseDrawers, 600, KitchenWall::Left)
            .add(CabinetModule::WallDoors, std::nullopt, KitchenWall::Left)
            .add(CabinetModule::WallDoors, std::nullopt, KitchenWall::Left)
            .add(CabinetModule::BaseDrawers, 400, KitchenWall::Right)
            .add(CabinetModule::Hob, 600, KitchenWall::Right)
            .add(CabinetModule::BaseDoors, std::nullopt, KitchenWall::Right)
            .add(CabinetModule::WallDoors, std::nullopt, KitchenWall::Right)
            .add(CabinetModule::WallDoors, std::nullopt, KitchenWall::Right)
            .build();
    }
    throw std::invalid_argument("Unknown CatalogItem");
}

} // namespace rc::core
