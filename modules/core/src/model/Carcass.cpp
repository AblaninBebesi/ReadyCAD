#include "readycad/core/model/Carcass.hpp"

#include <algorithm>
#include <stdexcept>

namespace rc::core {

Carcass::Carcass()
    : backMount_(makeBackMount(BackMountType::Groove))
    , left_(std::make_shared<SideWall>(Side::Left))
    , right_(std::make_shared<SideWall>(Side::Right))
    , top_(std::make_shared<TopPanel>())
    , bottom_(std::make_shared<BottomPanel>())
    , back_(std::make_shared<BackPanel>())
    , interior_(std::make_shared<Section>())
    , plinth_(std::make_shared<Plinth>())
    , blindPanel_(std::make_shared<BlindPanel>())
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

std::size_t Carcass::indexOf(const FurnitureComponent& child) const noexcept
{
    const auto& all = children();
    const auto it = std::find_if(all.begin(), all.end(),
                                 [&child](const Ptr& c) { return c.get() == &child; });
    return static_cast<std::size_t>(it - all.begin());
}

void Carcass::setTopStyle(CarcassTop style)
{
    if (style == topStyle_) {
        return;
    }
    // The top part(s) keep their place among the children: the tree reads the same way.
    const std::size_t at = indexOf(topStyle_ == CarcassTop::Panel
                                       ? static_cast<const FurnitureComponent&>(*top_)
                                       : static_cast<const FurnitureComponent&>(*frontRail_));
    const std::shared_ptr<Rail> oldFront = frontRail_;
    const std::shared_ptr<Rail> oldRear = rearRail_;
    detachChildren([this](const FurnitureComponent& c) {
        return &c == top_.get() || &c == frontRail_.get() || &c == rearRail_.get();
    });
    topStyle_ = style;

    if (style == CarcassTop::Panel) {
        frontRail_.reset();
        rearRail_.reset();
        adoptAt(at, top_);
    } else {
        // Rails of the other orientation are other parts (their axes differ): keep what still
        // applies of the user's choices — name, material, cutting plan; not the edges.
        const bool upright = style == CarcassTop::UprightRails;
        frontRail_ = std::make_shared<Rail>(RailPosition::Front, upright);
        rearRail_ = std::make_shared<Rail>(RailPosition::Rear, upright);
        for (const auto& [from, to] : {std::pair{oldFront, frontRail_}, std::pair{oldRear, rearRail_}}) {
            if (from) {
                to->setName(from->name());
                to->setMaterialOverride(from->materialOverride());
                to->setInCuttingPlan(from->inCuttingPlan());
            }
        }
        adoptAt(at, frontRail_);
        adoptAt(at + 1, rearRail_);
    }
    requestLayout();
}

bool Carcass::hasPlinth() const noexcept
{
    return plinth_->parent() == this;
}

void Carcass::setPlinth(bool plinth)
{
    if (plinth == hasPlinth()) {
        return;
    }
    if (plinth) {
        adopt(plinth_);
    } else {
        detachChildren([this](const FurnitureComponent& c) { return &c == plinth_.get(); });
    }
    requestLayout();
}

void Carcass::setFixedWidth(std::optional<Real> width)
{
    if (width && !(*width > 0.0)) {
        throw std::invalid_argument("Carcass: width must be > 0");
    }
    fixedWidth_ = width;
    requestLayout();
}

void Carcass::setCutout(std::optional<WorktopCutout> cutout)
{
    if (cutout) {
        const bool sized = cutout->width > 0.0 && cutout->depth > 0.0 && cutout->radius >= 0.0
                        && cutout->frontOffset >= 0.0;
        if (!sized || 2.0 * cutout->radius > std::min(cutout->width, cutout->depth)) {
            throw std::invalid_argument("Carcass: invalid worktop cut-out");
        }
    }
    cutout_ = cutout;
    requestLayout();
}

void Carcass::setBlindCorner(std::optional<Real> filler)
{
    if (filler && !(*filler >= 0.0)) {
        throw std::invalid_argument("Carcass: the filler must not be negative");
    }
    const bool wasBlind = blindFiller_.has_value();
    blindFiller_ = filler;
    if (filler && !wasBlind) {
        adopt(blindPanel_);
    } else if (!filler && wasBlind) {
        detachChildren([this](const FurnitureComponent& c) { return &c == blindPanel_.get(); });
    }
    requestLayout();
}

void Carcass::setWorktopJoint(bool joint)
{
    worktopJoint_ = joint;
    requestLayout();
}

BlindPanel* Carcass::blindPanel() const noexcept
{
    return blindFiller_ ? blindPanel_.get() : nullptr;
}

Rail* Carcass::frontRail() const noexcept
{
    return frontRail_.get();
}

Rail* Carcass::rearRail() const noexcept
{
    return rearRail_.get();
}

Plinth* Carcass::plinth() const noexcept
{
    return hasPlinth() ? plinth_.get() : nullptr;
}

std::vector<PanelComponent*> Carcass::frameParts() const
{
    std::vector<PanelComponent*> parts{left_.get(), right_.get()};
    if (topStyle_ == CarcassTop::Panel) {
        parts.push_back(top_.get());
    } else {
        parts.push_back(frontRail_.get());
        parts.push_back(rearRail_.get());
    }
    parts.push_back(bottom_.get());
    return parts;
}

void Carcass::layoutChildren(const LayoutSlot& slot)
{
    // 0. A plinth takes the bottom of the box: the carcass itself stands on it.
    const Aabb& whole = box();
    const Real plinthHeight =
        hasPlinth() ? std::clamp(slot.r().kitchen.plinthHeight, 0.0, whole.size().y) : 0.0;
    const Aabb outer(whole.min() + Vec3(0.0, plinthHeight, 0.0), whole.max());
    const bool panelTop = topStyle_ == CarcassTop::Panel;

    // 1. Context for the structural parts' own formulas.
    frame_.outer = outer;
    frame_.leftThickness = left_->thickness(slot);
    frame_.rightThickness = right_->thickness(slot);
    frame_.topThickness = panelTop ? top_->thickness(slot) : frontRail_->thickness(slot);
    frame_.bottomThickness = bottom_->thickness(slot);
    frame_.topJoint = panelTop ? topJoint_ : JointType::Inset; // rails stand between the sides
    frame_.bottomJoint = bottomJoint_;
    frame_.top = topStyle_;
    frame_.backMount = backMount_.get();
    frame_.backThickness = back_->thickness(slot);

    LayoutSlot partSlot = slot.withBox(outer);
    partSlot.corner = nullptr; // for this carcass only, not for its parts
    partSlot.frame = &frame_;
    partSlot.frontZ = outer.max().z;
    for (PanelComponent* part : frameParts()) {
        part->layout(partSlot);
    }
    back_->layout(partSlot);
    if (Plinth* plinth = this->plinth()) {
        LayoutSlot plinthSlot = partSlot;
        plinthSlot.box = Aabb(Vec3(whole.min().x - slot.plinthBeyondStart, whole.min().y, whole.min().z),
                              Vec3(whole.max().x + slot.plinthBeyondEnd, outer.min().y, whole.max().z));
        plinth->layout(plinthSlot);
    }

    // 2. The opening is whatever the walls left free.
    const Real top = panelTop ? top_->box().min().y
                              : std::min(frontRail_->box().min().y, rearRail_->box().min().y);
    const Aabb opening(Vec3(left_->box().max().x, bottom_->box().max().y, back_->box().max().z),
                       Vec3(right_->box().min().x, top, outer.max().z));

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

    // 4. A blind corner cabinet: the fronts stop a front gap before the part the other wall's
    //    cabinets hide and the filler; the blind panel covers that, flush with them.
    if (BlindPanel* blind = blindPanel()) {
        const bool right = slot.corner == nullptr || slot.corner->blindEnd == Side::Right;
        const Real hidden = slot.corner != nullptr ? slot.corner->hidden : 0.0;
        const Real width = std::clamp(hidden + *blindFiller_, 0.0, outer.size().x);
        const Real gap = slot.r().frontGap;
        Real from = outer.min().x + edgeGap;
        Real to = outer.max().x - edgeGap;
        if (right) {
            from = outer.max().x - width;
            interiorSlot.overlays.right = (from - gap) - opening.max().x;
        } else {
            to = outer.min().x + width;
            interiorSlot.overlays.left = opening.min().x - (to + gap);
        }
        LayoutSlot blindSlot = partSlot.withBox(Aabb(Vec3(from, outer.min().y + edgeGap, outer.max().z),
                                                     Vec3(to, outer.max().y - edgeGap, outer.max().z)));
        blindSlot.frame = nullptr;
        blind->layout(blindSlot);
    }
    interiorSlot.corner = nullptr;
    interior_->layout(interiorSlot);
}

} // namespace rc::core
