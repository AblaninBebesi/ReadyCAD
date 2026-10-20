#pragma once

#include <string_view>

namespace rc::core {

/// Concrete type of a node in the furniture tree. Used by the UI (icons, localised names),
/// serialisation and rules like "a Section accepts Shelves but not SideWalls".
enum class ComponentKind {
    // Composites
    Product,
    Carcass,
    Section,
    DrawerStack,
    Drawer,
    Kitchen,
    KitchenRun,
    KitchenCorner,
    CabinetRow,
    // Carcass parts
    SideWall,
    TopPanel,
    BottomPanel,
    BackPanel,
    Rail,
    Plinth,
    // Interior parts
    Shelf,
    FixedShelf,
    Partition,
    // Fronts
    Door,
    DrawerFront,
    // Drawer box parts
    DrawerBoxSide,
    DrawerBoxEnd,
    DrawerBottom,
    // Kitchen parts
    Worktop,
    BlindPanel,
};

/// Stable English identifier, e.g. "SideWall" (logs, file format). Not for the UI.
[[nodiscard]] std::string_view toString(ComponentKind kind) noexcept;

} // namespace rc::core
