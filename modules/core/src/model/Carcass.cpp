#include "readycad/core/model/Carcass.hpp"

namespace rc::core {

Carcass::Carcass()
    : backMount_(makeBackMount(BackMountType::Groove))
    , left_(std::make_shared<SideWall>(Side::Left))
    , right_(std::make_shared<SideWall>(Side::Right))
    , top_(std::make_shared<TopPanel>())
    , bottom_(std::make_shared<BottomPanel>())
    , back_(std::make_shared<BackPanel>())
    , interior_(std::make_shared<Section>())
{
    adopt(left_);
    adopt(right_);
    adopt(top_);
    adopt(bottom_);
    adopt(back_);
    adopt(interior_);
}

void Carcass::setTopJoint(JointType joint)
{
    topJoint_ = joint;
    requestLayout();
}

void Carcass::setBottomJoint(JointType joint)
{
    bottomJoint_ = joint;
    requestLayout();
}

void Carcass::setBackMount(BackMountType type)
{
    backMount_ = makeBackMount(type);
    requestLayout();
}

void Carcass::layoutChildren(const LayoutSlot& slot)
{
    const Aabb& outer = box();

    // 1. Context for the structural parts' own formulas.
    frame_.outer = outer;
    frame_.leftThickness = left_->thickness(slot);
    frame_.rightThickness = right_->thickness(slot);
    frame_.topThickness = top_->thickness(slot);
    frame_.bottomThickness = bottom_->thickness(slot);
    frame_.topJoint = topJoint_;
    frame_.bottomJoint = bottomJoint_;
    frame_.backMount = backMount_.get();
    frame_.backThickness = back_->thickness(slot);

    LayoutSlot partSlot = slot.withBox(outer);
    partSlot.frame = &frame_;
    partSlot.frontZ = outer.max().z;
    for (FurnitureComponent* part : {static_cast<FurnitureComponent*>(left_.get()),
                                     static_cast<FurnitureComponent*>(right_.get()),
                                     static_cast<FurnitureComponent*>(top_.get()),
                                     static_cast<FurnitureComponent*>(bottom_.get()),
                                     static_cast<FurnitureComponent*>(back_.get())}) {
        part->layout(partSlot);
    }

    // 2. The opening is whatever the walls left free.
    const Aabb opening(Vec3(left_->box().max().x, bottom_->box().max().y, back_->box().max().z),
                       Vec3(right_->box().min().x, top_->box().min().y, outer.max().z));

    // 3. Fronts may cover the outer walls up to frontEdgeGap from the carcass edge.
    const Real edgeGap = slot.r().frontEdgeGap;
    LayoutSlot interiorSlot = slot.withBox(opening);
    interiorSlot.frontZ = outer.max().z;
    interiorSlot.frame = nullptr; // interior parts must not depend on carcass internals
    interiorSlot.holeGridOriginY = system32OriginY();
    interiorSlot.overlays.left = opening.min().x - (outer.min().x + edgeGap);
    interiorSlot.overlays.right = (outer.max().x - edgeGap) - opening.max().x;
    interiorSlot.overlays.bottom = opening.min().y - (outer.min().y + edgeGap);
    interiorSlot.overlays.top = (outer.max().y - edgeGap) - opening.max().y;
    interior_->layout(interiorSlot);
}

} // namespace rc::core
