#include "readycad/core/model/Product.hpp"

#include <stdexcept>
#include <utility>

namespace rc::core {

Product::Product(std::string name, ConstructionRules rules)
    : FurnitureComponent(std::move(name)), rules_(std::move(rules))
{
}

void Product::setRules(ConstructionRules rules)
{
    rules_ = std::move(rules);
    if (isLaidOut()) {
        update();
    }
}

void Product::setDimensions(const Size3D& dimensions)
{
    if (dimensions.width <= 0.0 || dimensions.height <= 0.0 || dimensions.depth <= 0.0) {
        throw std::invalid_argument("Product: dimensions must be positive");
    }
    dimensions_ = dimensions;
    update();
}

void Product::update()
{
    LayoutSlot slot;
    slot.box = Aabb::fromOriginAndSize(Vec3(0.0), dimensions_.toVec3());
    slot.frontZ = dimensions_.depth;
    slot.rules = &rules_;
    layout(slot);
}

Carcass* Product::carcass() const noexcept
{
    for (const Ptr& child : children()) {
        if (child->kind() == ComponentKind::Carcass) {
            return static_cast<Carcass*>(child.get());
        }
    }
    return nullptr;
}

bool Product::canAccept(const FurnitureComponent& child) const
{
    // One carcass per product for now; multi-carcass products (kitchens) come later.
    return child.kind() == ComponentKind::Carcass && carcass() == nullptr;
}

} // namespace rc::core
