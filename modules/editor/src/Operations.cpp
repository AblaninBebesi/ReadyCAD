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

core::Carcass& requireCarcass(Product& product)
{
    core::Carcass* carcass = product.carcass();
    if (carcass == nullptr) {
        throw std::invalid_argument("The product has no carcass");
    }
    return *carcass;
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

void setTopJoint(Document& doc, core::JointType joint)
{
    doc.edit("Top joint", [&](Product& p) { requireCarcass(p).setTopJoint(joint); });
}

void setBottomJoint(Document& doc, core::JointType joint)
{
    doc.edit("Bottom joint", [&](Product& p) { requireCarcass(p).setBottomJoint(joint); });
}

void setBackMount(Document& doc, core::BackMountType mount)
{
    doc.edit("Back panel mount", [&](Product& p) { requireCarcass(p).setBackMount(mount); });
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

void removeComponent(Document& doc, ComponentId component)
{
    doc.edit("Remove", [&](Product& p) {
        core::FurnitureComponent* node = p.find(component);
        if (node == nullptr || node->parent() == nullptr) {
            throw std::invalid_argument("Component not found");
        }
        const core::ComponentKind kind = node->kind();
        const bool removable = kind == core::ComponentKind::Shelf
                            || kind == core::ComponentKind::Door
                            || kind == core::ComponentKind::DrawerStack;
        if (!removable) {
            throw std::logic_error("This part belongs to the construction and cannot be removed");
        }
        node->parent()->remove(component);
    });
}

} // namespace rc::editor
