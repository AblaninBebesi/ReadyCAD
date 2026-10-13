#pragma once

#include "readycad/core/model/Product.hpp"

#include <memory>
#include <span>

namespace rc::core {

/// Ready-made models of the catalogue (stage 5 shows them in the UI).
enum class CatalogItem {
    Wardrobe,            // 1 section, 4 shelves, 2 doors
    WardrobeWithDrawers, // 2 sections, drawers at the bottom, shelves and doors above
    ChestOfDrawers,      // 4 drawers
    Bookcase,            // open shelving, 2 sections, no doors
    Kitchen,             // straight run: sink, drawers, hob, doors; wall cabinets above
};

/// All catalogue items, in display order.
[[nodiscard]] std::span<const CatalogItem> catalogItems() noexcept;

/// Factory method: creates a fully laid-out product for a catalogue item.
[[nodiscard]] std::shared_ptr<Product> createProduct(CatalogItem item,
                                                     const ConstructionRules& rules = {});

} // namespace rc::core
