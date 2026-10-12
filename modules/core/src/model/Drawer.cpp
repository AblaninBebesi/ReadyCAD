#include "readycad/core/model/Drawer.hpp"

#include <algorithm>
#include <stdexcept>

namespace rc::core {

// ---------------------------------------------------------------------------------------------
// Drawer
// ---------------------------------------------------------------------------------------------

Drawer::Drawer() : front_(std::make_shared<DrawerFront>())
{
    adopt(front_);
    adopt(std::make_shared<DrawerBoxSide>(Side::Left));
    adopt(std::make_shared<DrawerBoxSide>(Side::Right));
    adopt(std::make_shared<DrawerBoxEnd>(DrawerEnd::Front));
    adopt(std::make_shared<DrawerBoxEnd>(DrawerEnd::Back));
    adopt(std::make_shared<DrawerBottom>());
}

void Drawer::layoutChildren(const LayoutSlot& slot)
{
    const ConstructionRules& r = slot.r();
    const Aabb& zone = box();
    const Vec3 zoneSize = zone.size();

    // Longest standard slide that fits in front of the back panel.
    const Real availableDepth = zoneSize.z - r.drawerRearClearance;
    slideLength_ = 0.0;
    for (const Real length : r.drawerSlideLengths) {
        if (length <= availableDepth + kEpsilon) {
            slideLength_ = std::max(slideLength_, length);
        }
    }
    const Real depth = slideLength_ > 0.0 ? slideLength_ : std::max(0.0, availableDepth);

    const Real width = std::max(0.0, zoneSize.x - 2.0 * r.drawerSlideClearance);
    const Real height = std::clamp(
        zoneSize.y - r.drawerBoxBottomClearance - r.drawerBoxTopClearance, 0.0,
        r.drawerBoxMaxHeight);

    const Vec3 lo(zone.min().x + r.drawerSlideClearance, zone.min().y + r.drawerBoxBottomClearance,
                  zone.max().z - depth);
    body_ = Aabb::fromOriginAndSize(lo, Vec3(width, height, depth));

    // The box parts meet each other: tell them the materials their neighbours really have.
    boxFrame_ = DrawerBoxFrame{r.drawerBoxMaterial.thickness, r.drawerBoxMaterial.thickness,
                               r.drawerBottomMaterial.thickness};
    for (const Ptr& child : children()) {
        const auto* part = dynamic_cast<const PanelComponent*>(child.get());
        if (part == nullptr) {
            continue;
        }
        if (child->kind() == ComponentKind::DrawerBoxSide) {
            const bool left = static_cast<const DrawerBoxSide*>(part)->side() == Side::Left;
            (left ? boxFrame_.leftSide : boxFrame_.rightSide) = part->thickness(slot);
        } else if (child->kind() == ComponentKind::DrawerBottom) {
            boxFrame_.bottom = part->thickness(slot);
        }
    }
    LayoutSlot boxSlot = slot.withBox(body_);
    boxSlot.drawerBox = &boxFrame_;

    for (const Ptr& child : children()) {
        if (child.get() == front_.get()) {
            front_->layout(slot); // same zone and overlays: the front covers them
        } else {
            child->layout(boxSlot);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// DrawerStack
// ---------------------------------------------------------------------------------------------

DrawerStack::DrawerStack(int count)
{
    setCount(count);
}

void DrawerStack::setCount(int count)
{
    if (count < 0) {
        throw std::invalid_argument("DrawerStack: count must be >= 0");
    }
    detachChildren([](const FurnitureComponent&) { return true; });
    for (int i = 0; i < count; ++i) {
        adopt(std::make_shared<Drawer>());
    }
    requestLayout();
}

std::vector<Drawer*> DrawerStack::drawers() const
{
    std::vector<Drawer*> result;
    for (const Ptr& child : children()) {
        result.push_back(static_cast<Drawer*>(child.get()));
    }
    return result;
}

bool DrawerStack::canAccept(const FurnitureComponent& child) const
{
    return child.kind() == ComponentKind::Drawer;
}

void DrawerStack::layoutChildren(const LayoutSlot& slot)
{
    const std::vector<Drawer*> drawers = this->drawers();
    if (drawers.empty()) {
        return;
    }
    LayoutSlot own = slot.withBox(box());
    own.overlays = slot.overlays;
    const Aabb fronts = own.frontRect();

    const Real gap = slot.r().frontGap;
    const auto count = static_cast<Real>(drawers.size());
    const Real frontHeight = (fronts.size().y - (count - 1.0) * gap) / count;

    Real frontBottom = fronts.min().y;
    for (Drawer* drawer : drawers) {
        const Real frontTop = frontBottom + frontHeight;
        // The drawer's zone is the part of the opening behind its front.
        const Real y0 = std::max(frontBottom, box().min().y);
        const Real y1 = std::max(y0, std::min(frontTop, box().max().y));

        LayoutSlot drawerSlot = slot.withBox(Aabb(Vec3(box().min().x, y0, box().min().z),
                                                  Vec3(box().max().x, y1, box().max().z)));
        drawerSlot.overlays.left = slot.overlays.left;
        drawerSlot.overlays.right = slot.overlays.right;
        drawerSlot.overlays.bottom = y0 - frontBottom;
        drawerSlot.overlays.top = frontTop - y1;
        drawer->layout(drawerSlot);

        frontBottom = frontTop + gap;
    }
}

} // namespace rc::core
