#include "readycad/ui/Labels.hpp"

#include <cmath>
#include <cstdio>

namespace rc::ui {

using core::ComponentKind;

TextId kindText(ComponentKind kind) noexcept
{
    switch (kind) {
    case ComponentKind::Product:       return TextId::KindProduct;
    case ComponentKind::Carcass:       return TextId::KindCarcass;
    case ComponentKind::Section:       return TextId::KindSection;
    case ComponentKind::DrawerStack:   return TextId::KindDrawerStack;
    case ComponentKind::Drawer:        return TextId::KindDrawer;
    case ComponentKind::SideWall:      return TextId::KindSideWall;
    case ComponentKind::TopPanel:      return TextId::KindTopPanel;
    case ComponentKind::BottomPanel:   return TextId::KindBottomPanel;
    case ComponentKind::BackPanel:     return TextId::KindBackPanel;
    case ComponentKind::Shelf:         return TextId::KindShelf;
    case ComponentKind::FixedShelf:    return TextId::KindFixedShelf;
    case ComponentKind::Partition:     return TextId::KindPartition;
    case ComponentKind::Door:          return TextId::KindDoor;
    case ComponentKind::DrawerFront:   return TextId::KindDrawerFront;
    case ComponentKind::DrawerBoxSide: return TextId::KindDrawerBoxSide;
    case ComponentKind::DrawerBoxEnd:  return TextId::KindDrawerBoxEnd;
    case ComponentKind::DrawerBottom:  return TextId::KindDrawerBottom;
    }
    return TextId::KindProduct;
}

TextId catalogText(core::CatalogItem item) noexcept
{
    switch (item) {
    case core::CatalogItem::Wardrobe:            return TextId::CatalogWardrobe;
    case core::CatalogItem::WardrobeWithDrawers: return TextId::CatalogWardrobeWithDrawers;
    case core::CatalogItem::ChestOfDrawers:      return TextId::CatalogChestOfDrawers;
    case core::CatalogItem::Bookcase:            return TextId::CatalogBookcase;
    }
    return TextId::CatalogWardrobe;
}

TextId holeText(core::FastenerType type) noexcept
{
    switch (type) {
    case core::FastenerType::Confirmat:    return TextId::HoleConfirmat;
    case core::FastenerType::Dowel:        return TextId::HoleDowel;
    case core::FastenerType::Eccentric:    return TextId::HoleEccentric;
    case core::FastenerType::ShelfSupport: return TextId::HoleShelfSupport;
    case core::FastenerType::Hinge:        return TextId::HoleHinge;
    case core::FastenerType::Groove:       return TextId::HoleGroove;
    case core::FastenerType::Handle:       return TextId::HoleHandle;
    }
    return TextId::Holes;
}

std::string displayName(const core::FurnitureComponent& component)
{
    if (!component.name().empty()) {
        return component.name();
    }
    const std::string kind(text(kindText(component.kind())));
    const core::FurnitureComponent* parent = component.parent();
    if (parent == nullptr) {
        return kind;
    }
    int number = 0;
    int total = 0;
    for (const auto& sibling : parent->children()) {
        if (sibling->kind() == component.kind()) {
            ++total;
            if (sibling.get() == &component) {
                number = total;
            }
        }
    }
    return total > 1 ? kind + " " + std::to_string(number) : kind;
}

std::string mm(core::Real value)
{
    char buffer[32];
    const double rounded = std::round(value * 10.0) / 10.0;
    if (std::abs(rounded - std::round(rounded)) < 1e-9) {
        std::snprintf(buffer, sizeof buffer, "%.0f", rounded);
    } else {
        std::snprintf(buffer, sizeof buffer, "%.1f", rounded);
    }
    return buffer;
}

} // namespace rc::ui
