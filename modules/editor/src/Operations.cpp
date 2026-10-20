#include "readycad/editor/Operations.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace rc::editor {

using core::ComponentId;
using core::Product;

namespace {

template <typename T>
T& require(Product& product, ComponentId id)
{
    auto* typed = dynamic_cast<T*>(product.find(id));
    if (typed == nullptr) {
        throw std::invalid_argument("Component not found or of the wrong type");
    }
    return *typed;
}

core::Kitchen& requireKitchen(Product& product)
{
    core::Kitchen* kitchen = product.kitchen();
    if (kitchen == nullptr) {
        throw std::invalid_argument("The product is not a kitchen");
    }
    return *kitchen;
}

/// A cabinet of a kitchen: of a row, or of a corner when `corners`.
core::Carcass& requireCabinet(Product& product, ComponentId id, bool corners = false)
{
    auto& cabinet = require<core::Carcass>(product, id);
    const core::FurnitureComponent* parent = cabinet.parent();
    const bool inRow = parent != nullptr && parent->kind() == core::ComponentKind::CabinetRow;
    const bool inCorner = parent != nullptr && parent->kind() == core::ComponentKind::KitchenCorner;
    if (!inRow && !(corners && inCorner)) {
        throw std::invalid_argument("Not a cabinet of a kitchen row");
    }
    return cabinet;
}

core::KitchenRun& requireRun(core::Kitchen& kitchen, core::KitchenWall wall)
{
    core::KitchenRun* run = wall == core::KitchenWall::Back ? &kitchen.backRun()
                          : kitchen.sideRun(wall == core::KitchenWall::Left ? core::Side::Left
                                                                            : core::Side::Right);
    if (run == nullptr) {
        throw std::invalid_argument("The kitchen has no cabinets on that wall");
    }
    return *run;
}

/// Merge key unique per component, so dragging one value never merges with another part.
std::string key(std::string_view what, ComponentId id)
{
    return std::string(what) + ":" + std::to_string(id.value);
}

} // namespace

void setDimensions(Document& doc, const core::Size3D& dimensions)
{
    doc.edit("Dimensions", [&](Product& p) { p.setDimensions(dimensions); }, "dimensions");
}

void setProductName(Document& doc, std::string name)
{
    doc.edit("Name", [&](Product& p) { p.setName(name); }, "name");
}

void setRules(Document& doc, core::ConstructionRules rules)
{
    doc.edit("Workshop profile", [&](Product& p) { p.setRules(std::move(rules)); });
}

void setTopJoint(Document& doc, ComponentId carcass, core::JointType joint)
{
    doc.edit("Top joint",
             [&](Product& p) { require<core::Carcass>(p, carcass).setTopJoint(joint); });
}

void setBottomJoint(Document& doc, ComponentId carcass, core::JointType joint)
{
    doc.edit("Bottom joint",
             [&](Product& p) { require<core::Carcass>(p, carcass).setBottomJoint(joint); });
}

void setBackMount(Document& doc, ComponentId carcass, core::BackMountType mount)
{
    doc.edit("Back panel mount",
             [&](Product& p) { require<core::Carcass>(p, carcass).setBackMount(mount); });
}

void setTopStyle(Document& doc, ComponentId carcass, core::CarcassTop style)
{
    doc.edit("Carcass top",
             [&](Product& p) { require<core::Carcass>(p, carcass).setTopStyle(style); });
}

void setPlinth(Document& doc, ComponentId carcass, bool plinth)
{
    doc.edit("Plinth", [&](Product& p) { require<core::Carcass>(p, carcass).setPlinth(plinth); });
}

void setKitchenSizes(Document& doc, const core::Kitchen::Sizes& sizes)
{
    doc.edit(
        "Kitchen sizes", [&](Product& p) { requireKitchen(p).setSizes(sizes); }, "kitchen-sizes");
}

void setSideRun(Document& doc, core::Side side, std::optional<core::Real> length)
{
    doc.edit(
        "Side wall",
        [&](Product& p) {
            core::Kitchen& kitchen = requireKitchen(p);
            if (!length) {
                kitchen.setSideRun(side, nullptr, nullptr);
                return;
            }
            if (core::KitchenRun* run = kitchen.sideRun(side)) {
                run->setLength(*length);
                return;
            }
            auto run = std::make_shared<core::KitchenRun>(side == core::Side::Left ? core::KitchenWall::Left
                                                                                   : core::KitchenWall::Right);
            run->setLength(*length);
            run->baseRow().add(core::makeCabinet(core::CabinetModule::BaseDoors, std::nullopt, *length));
            run->wallRow().add(core::makeCabinet(core::CabinetModule::WallDoors, std::nullopt, *length));
            kitchen.setSideRun(side, std::move(run), core::makeCorner(side));
        },
        side == core::Side::Left ? "side-run-left" : "side-run-right");
}

void addCabinet(Document& doc, core::CabinetModule module, core::Real width, core::KitchenWall wall)
{
    doc.edit("Add cabinet", [&](Product& p) {
        core::KitchenRun& run = requireRun(requireKitchen(p), wall);
        core::CabinetRow& row = core::levelOf(module) == core::RowLevel::Base ? run.baseRow()
                                                                              : run.wallRow();
        row.add(core::makeCabinet(module, width, width));
    });
}

void setCabinetWidth(Document& doc, ComponentId cabinet, std::optional<core::Real> width)
{
    doc.edit(
        "Cabinet width",
        [&](Product& p) {
            core::Carcass& c = requireCabinet(p, cabinet, true);
            if (!width && c.parent()->kind() == core::ComponentKind::KitchenCorner) {
                throw std::invalid_argument("A corner cabinet has a width of its own");
            }
            c.setFixedWidth(width);
        },
        key("cabinet-width", cabinet));
}

void setWorktopJoint(Document& doc, ComponentId cabinet, bool joint)
{
    doc.edit("Worktop joint", [&](Product& p) {
        core::Carcass& c = requireCabinet(p, cabinet);
        if (static_cast<const core::CabinetRow*>(c.parent())->level() != core::RowLevel::Base) {
            throw std::invalid_argument("Only a base cabinet carries the worktop");
        }
        c.setWorktopJoint(joint);
    });
}

void setBlindFiller(Document& doc, ComponentId cabinet, core::Real filler)
{
    doc.edit(
        "Filler",
        [&](Product& p) {
            core::Carcass& c = require<core::Carcass>(p, cabinet);
            if (!c.blindFiller()) {
                throw std::invalid_argument("Not a blind corner cabinet");
            }
            c.setBlindCorner(filler);
        },
        key("filler", cabinet));
}

void moveCabinet(Document& doc, ComponentId cabinet, int steps)
{
    doc.edit("Move cabinet", [&](Product& p) {
        core::Carcass& c = requireCabinet(p, cabinet);
        static_cast<core::CabinetRow*>(c.parent())->moveCabinet(cabinet, steps);
    });
}

void setCutout(Document& doc, ComponentId cabinet, std::optional<core::WorktopCutout> cutout)
{
    doc.edit(
        "Worktop cut-out", [&](Product& p) { requireCabinet(p, cabinet).setCutout(cutout); },
        key("cutout", cabinet));
}

void splitSection(Document& doc, ComponentId section, core::Axis axis, int count)
{
    doc.edit("Split section", [&](Product& p) {
        auto& s = require<core::Section>(p, section);
        if (axis == core::Axis::X) {
            s.splitHorizontally(count);
        } else if (axis == core::Axis::Y) {
            s.splitVertically(count);
        } else {
            throw std::invalid_argument("A section can only be split along X or Y");
        }
    });
}

void clearSection(Document& doc, ComponentId section)
{
    doc.edit("Clear section", [&](Product& p) {
        auto& s = require<core::Section>(p, section);
        while (!s.children().empty()) {
            s.remove(s.children().back()->id());
        }
    });
}

void setSectionFixedSize(Document& doc, ComponentId section, std::optional<core::Real> size)
{
    doc.edit(
        "Section size",
        [&](Product& p) { require<core::Section>(p, section).setFixedSize(size); },
        key("section-size", section));
}

void setShelfCount(Document& doc, ComponentId section, int count, core::ShelfMount mount)
{
    doc.edit("Shelves", [&](Product& p) {
        require<core::Section>(p, section).setAutoShelfCount(count, mount);
    });
}

void setDoorCount(Document& doc, ComponentId section, int count)
{
    doc.edit("Doors", [&](Product& p) { require<core::Section>(p, section).setDoors(count); });
}

void setDrawerCount(Document& doc, ComponentId section, int count)
{
    doc.edit("Drawers",
             [&](Product& p) { require<core::Section>(p, section).setDrawers(count); });
}

void setShelfMount(Document& doc, ComponentId shelf, core::ShelfMount mount)
{
    doc.edit("Shelf mount", [&](Product& p) { require<core::Shelf>(p, shelf).setMount(mount); });
}

void setShelfElevation(Document& doc, ComponentId shelf, std::optional<core::Real> elevation)
{
    doc.edit(
        "Shelf height",
        [&](Product& p) { require<core::Shelf>(p, shelf).setElevation(elevation); },
        key("shelf-elevation", shelf));
}

void setHinge(Document& doc, ComponentId door, core::HingeSide side)
{
    doc.edit("Hinge side", [&](Product& p) { require<core::Door>(p, door).setHinge(side); });
}

void setHandle(Document& doc, ComponentId front, std::optional<core::HandlePlacement> placement)
{
    doc.edit(
        "Handle",
        [&](Product& p) { require<core::FrontPanel>(p, front).setHandle(placement); },
        key("handle", front));
}

void setMaterialOverride(Document& doc, ComponentId part, std::optional<core::Material> material)
{
    doc.edit("Material", [&](Product& p) {
        require<core::PanelComponent>(p, part).setMaterialOverride(material);
    });
}

void setShape(Document& doc, ComponentId part, std::vector<core::ShapeFeature> features)
{
    doc.edit(
        "Part shape",
        [&](Product& p) { require<core::PanelComponent>(p, part).setShape(std::move(features)); },
        key("shape", part));
}

void setInCuttingPlan(Document& doc, const std::vector<ComponentId>& parts, bool inPlan)
{
    doc.edit("Cutting plan", [&](Product& p) {
        for (const ComponentId id : parts) {
            require<core::PanelComponent>(p, id).setInCuttingPlan(inPlan);
        }
    });
}

void removeComponent(Document& doc, ComponentId component)
{
    doc.edit("Remove", [&](Product& p) {
        core::FurnitureComponent* node = p.find(component);
        if (node == nullptr || node->parent() == nullptr) {
            throw std::invalid_argument("Component not found");
        }
        const core::ComponentKind kind = node->kind();
        const bool cabinet = kind == core::ComponentKind::Carcass
                          && node->parent()->kind() == core::ComponentKind::CabinetRow;
        const bool removable = kind == core::ComponentKind::Shelf
                            || kind == core::ComponentKind::Door
                            || kind == core::ComponentKind::DrawerStack || cabinet;
        if (!removable) {
            throw std::logic_error("This part belongs to the construction and cannot be removed");
        }
        node->parent()->remove(component);
    });
}

} // namespace rc::editor
