#include "readycad/i18n/Labels.hpp"

#include <cmath>
#include <cstdio>

namespace rc::i18n {

using core::ComponentKind;

TextId kindText(ComponentKind kind) noexcept
{
    switch (kind) {
    case ComponentKind::Product:       return TextId::KindProduct;
    case ComponentKind::Carcass:       return TextId::KindCarcass;
    case ComponentKind::Section:       return TextId::KindSection;
    case ComponentKind::DrawerStack:   return TextId::KindDrawerStack;
    case ComponentKind::Drawer:        return TextId::KindDrawer;
    case ComponentKind::Kitchen:       return TextId::KindKitchen;
    case ComponentKind::KitchenRun:    return TextId::KindKitchenRun;
    case ComponentKind::KitchenCorner: return TextId::KindKitchenCorner;
    case ComponentKind::CabinetRow:    return TextId::KindCabinetRow;
    case ComponentKind::SideWall:      return TextId::KindSideWall;
    case ComponentKind::TopPanel:      return TextId::KindTopPanel;
    case ComponentKind::BottomPanel:   return TextId::KindBottomPanel;
    case ComponentKind::BackPanel:     return TextId::KindBackPanel;
    case ComponentKind::Rail:          return TextId::KindRail;
    case ComponentKind::Plinth:        return TextId::KindPlinth;
    case ComponentKind::Shelf:         return TextId::KindShelf;
    case ComponentKind::FixedShelf:    return TextId::KindFixedShelf;
    case ComponentKind::Partition:     return TextId::KindPartition;
    case ComponentKind::Door:          return TextId::KindDoor;
    case ComponentKind::DrawerFront:   return TextId::KindDrawerFront;
    case ComponentKind::DrawerBoxSide: return TextId::KindDrawerBoxSide;
    case ComponentKind::DrawerBoxEnd:  return TextId::KindDrawerBoxEnd;
    case ComponentKind::DrawerBottom:  return TextId::KindDrawerBottom;
    case ComponentKind::Worktop:       return TextId::KindWorktop;
    case ComponentKind::BlindPanel:    return TextId::KindBlindPanel;
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
    case core::CatalogItem::Kitchen:             return TextId::CatalogKitchen;
    case core::CatalogItem::KitchenL:            return TextId::CatalogKitchenL;
    case core::CatalogItem::KitchenU:            return TextId::CatalogKitchenU;
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

TextId edgeText(core::EdgeSide side) noexcept
{
    switch (side) {
    case core::EdgeSide::L1: return TextId::EdgeL1;
    case core::EdgeSide::L2: return TextId::EdgeL2;
    case core::EdgeSide::W1: return TextId::EdgeW1;
    case core::EdgeSide::W2: return TextId::EdgeW2;
    }
    return TextId::EdgeL1;
}

TextId hardwareText(core::HardwareKind kind) noexcept
{
    switch (kind) {
    case core::HardwareKind::Confirmat:    return TextId::HwConfirmat;
    case core::HardwareKind::Dowel:        return TextId::HwDowel;
    case core::HardwareKind::Eccentric:    return TextId::HwEccentric;
    case core::HardwareKind::ShelfSupport: return TextId::HwShelfSupport;
    case core::HardwareKind::Hinge:        return TextId::HwHinge;
    case core::HardwareKind::Handle:       return TextId::HwHandle;
    case core::HardwareKind::DrawerSlide:  return TextId::HwDrawerSlide;
    case core::HardwareKind::Leg:          return TextId::HwLeg;
    case core::HardwareKind::PlinthClip:   return TextId::HwPlinthClip;
    case core::HardwareKind::Hanger:       return TextId::HwHanger;
    }
    return TextId::HwConfirmat;
}

namespace {

/// Kind-specific words of the tree: a kitchen's walls, corners, rows and cabinets, front and
/// rear rails.
TextId treeText(const core::FurnitureComponent& component) noexcept
{
    const core::FurnitureComponent* parent = component.parent();
    switch (component.kind()) {
    case ComponentKind::KitchenRun:
        switch (static_cast<const core::KitchenRun&>(component).wall()) {
        case core::KitchenWall::Back:  return TextId::RunBack;
        case core::KitchenWall::Left:  return TextId::RunLeft;
        case core::KitchenWall::Right: return TextId::RunRight;
        }
        break;
    case ComponentKind::KitchenCorner:
        return static_cast<const core::KitchenCorner&>(component).end() == core::Side::Left
                   ? TextId::CornerLeft
                   : TextId::CornerRight;
    case ComponentKind::CabinetRow:
        return static_cast<const core::CabinetRow&>(component).level() == core::RowLevel::Base
                   ? TextId::BaseCabinets
                   : TextId::WallCabinets;
    case ComponentKind::Carcass:
        if (parent != nullptr && parent->kind() == ComponentKind::KitchenCorner) {
            return &static_cast<const core::KitchenCorner*>(parent)->baseCabinet() == &component
                       ? TextId::BlindBaseCabinet
                       : TextId::BlindWallCabinet;
        }
        if (parent != nullptr && parent->kind() == ComponentKind::CabinetRow) {
            return static_cast<const core::CabinetRow*>(parent)->level() == core::RowLevel::Base
                       ? TextId::BaseCabinet
                       : TextId::WallCabinet;
        }
        break;
    case ComponentKind::Rail:
        return static_cast<const core::Rail&>(component).position() == core::RailPosition::Front
                   ? TextId::FrontRail
                   : TextId::RearRail;
    default:
        break;
    }
    return kindText(component.kind());
}

} // namespace

std::string displayName(const core::FurnitureComponent& component)
{
    if (!component.name().empty()) {
        return component.name();
    }
    const std::string kind(text(treeText(component)));
    const core::FurnitureComponent* parent = component.parent();
    if (parent == nullptr) {
        return kind;
    }
    int number = 0;
    int total = 0;
    for (const auto& sibling : parent->children()) {
        if (treeText(*sibling) == treeText(component)) {
            ++total;
            if (sibling.get() == &component) {
                number = total;
            }
        }
    }
    return total > 1 ? kind + " " + std::to_string(number) : kind;
}

std::string partName(ComponentKind kind, const std::string& name, std::optional<core::Side> hand)
{
    if (!name.empty()) {
        return name;
    }
    std::string result(text(kindText(kind)));
    if (hand) {
        result += " (";
        result += text(*hand == core::Side::Left ? TextId::HandLeft : TextId::HandRight);
        result += ")";
    }
    return result;
}

std::string cornerText(core::PanelCorner corner)
{
    using core::EdgeSide;
    const bool l1 = corner == core::PanelCorner::L1W1 || corner == core::PanelCorner::L1W2;
    const bool w1 = corner == core::PanelCorner::L1W1 || corner == core::PanelCorner::L2W1;
    return std::string(text(edgeText(l1 ? EdgeSide::L1 : EdgeSide::L2))) + "–"
         + std::string(text(edgeText(w1 ? EdgeSide::W1 : EdgeSide::W2)));
}

TextId featureKindText(const core::ShapeFeature& feature) noexcept
{
    static constexpr TextId kinds[] = {TextId::ShapeCornerNotch, TextId::ShapeCornerRound,
                                       TextId::ShapeChamfer,     TextId::ShapeEdgeNotch,
                                       TextId::ShapeInnerCutout, TextId::ShapeRoundHole};
    return kinds[feature.index()];
}

std::string featureText(const core::ShapeFeature& feature)
{
    const std::string kind(text(featureKindText(feature)));
    const auto from = [](core::PanelCorner corner, core::Real alongL, core::Real alongW) {
        return cornerText(corner) + std::string(text(TextId::ShapeFromCorner)) + " " + mm(alongL) + " / "
               + mm(alongW);
    };
    return std::visit(
        [&](const auto& f) -> std::string {
            using T = std::decay_t<decltype(f)>;
            if constexpr (std::is_same_v<T, core::CornerNotch> || std::is_same_v<T, core::CornerChamfer>) {
                return kind + " " + cornerText(f.corner) + ": " + mm(f.alongLength) + " × "
                     + mm(f.alongWidth);
            } else if constexpr (std::is_same_v<T, core::CornerRound>) {
                return kind + " " + cornerText(f.corner) + ": R" + mm(f.radius);
            } else if constexpr (std::is_same_v<T, core::EdgeNotch>) {
                // The offset runs from the edge's corner with W1 / L1, or with W2 / L2.
                const bool longEdge = f.edge == core::EdgeSide::L1 || f.edge == core::EdgeSide::L2;
                const core::EdgeSide start = longEdge ? (f.fromEnd ? core::EdgeSide::W2 : core::EdgeSide::W1)
                                                      : (f.fromEnd ? core::EdgeSide::L2 : core::EdgeSide::L1);
                return kind + " " + std::string(text(edgeText(f.edge))) + ": "
                     + std::string(text(edgeText(start))) + std::string(text(TextId::ShapeFromEdge)) + " "
                     + mm(f.offset) + ", "
                     + mm(f.length) + " × " + mm(f.depth);
            } else if constexpr (std::is_same_v<T, core::InnerCutout>) {
                return kind + " " + mm(f.length) + " × " + mm(f.width) + " R" + mm(f.radius) + ": "
                     + from(f.from, f.alongLength, f.alongWidth);
            } else {
                return kind + " Ø" + mm(f.diameter) + ": " + from(f.from, f.alongLength, f.alongWidth);
            }
        },
        feature);
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

} // namespace rc::i18n
