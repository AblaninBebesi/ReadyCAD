#include "readycad/core/model/ComponentKind.hpp"

namespace rc::core {

std::string_view toString(ComponentKind kind) noexcept
{
    switch (kind) {
    case ComponentKind::Product:       return "Product";
    case ComponentKind::Carcass:       return "Carcass";
    case ComponentKind::Section:       return "Section";
    case ComponentKind::DrawerStack:   return "DrawerStack";
    case ComponentKind::Drawer:        return "Drawer";
    case ComponentKind::SideWall:      return "SideWall";
    case ComponentKind::TopPanel:      return "TopPanel";
    case ComponentKind::BottomPanel:   return "BottomPanel";
    case ComponentKind::BackPanel:     return "BackPanel";
    case ComponentKind::Shelf:         return "Shelf";
    case ComponentKind::FixedShelf:    return "FixedShelf";
    case ComponentKind::Partition:     return "Partition";
    case ComponentKind::Door:          return "Door";
    case ComponentKind::DrawerFront:   return "DrawerFront";
    case ComponentKind::DrawerBoxSide: return "DrawerBoxSide";
    case ComponentKind::DrawerBoxEnd:  return "DrawerBoxEnd";
    case ComponentKind::DrawerBottom:  return "DrawerBottom";
    }
    return "Unknown";
}

} // namespace rc::core
