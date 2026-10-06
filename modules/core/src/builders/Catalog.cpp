#include "readycad/core/builders/Catalog.hpp"

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
    }
    throw std::invalid_argument("Unknown CatalogItem");
}

} // namespace rc::core
