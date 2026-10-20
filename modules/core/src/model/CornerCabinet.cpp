#include "readycad/core/model/CornerCabinet.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace rc::core {

CornerCabinet::CornerCabinet(RowLevel level, Side end)
    : level_(level)
    , end_(end)
    , legBack_(level == RowLevel::Base ? kBaseLeg : kWallLeg)
    , legSide_(legBack_)
    , backMount_(makeBackMount(BackMountType::Overlay))
    // The wall at the end of the back leg is its left one at a right corner.
    , side_(std::make_shared<SideWall>(end == Side::Right ? Side::Left : Side::Right))
    , endWall_(std::make_shared<EndWall>(end))
    , bottom_(std::make_shared<CornerPanel>(CornerPanel::Role::Bottom, Axis::Y, Axis::X))
    , top_(std::make_shared<CornerPanel>(CornerPanel::Role::Top, Axis::Y, Axis::X))
    , back_(std::make_shared<BackPanel>())
    , endBack_(std::make_shared<CornerPanel>(CornerPanel::Role::Back, Axis::X, Axis::Z))
    // Hung on the wall at the end of the back leg; the second folds at the corner.
    , door_(std::make_shared<Door>(end == Side::Right ? HingeSide::Left : HingeSide::Right))
    , foldingDoor_(std::make_shared<FoldingDoor>(end == Side::Right ? HingeSide::Left
                                                                     : HingeSide::Right))
{
    adopt(side_);
    adopt(endWall_);
    adopt(bottom_);
    adopt(top_);
    adopt(back_);
    adopt(endBack_);
    adopt(door_);
    adopt(foldingDoor_);
    if (level == RowLevel::Base) {
        plinth_ = std::make_shared<Plinth>();
        endPlinth_ = std::make_shared<CornerPanel>(CornerPanel::Role::Plinth, Axis::X, Axis::Z);
        adopt(plinth_);
        adopt(endPlinth_);
    }
    // Handles where the hand finds them (at the top of a base cabinet, at the bottom of a wall
    // one), on the second door only: pulling it opens both.
    door_->setHandle(std::nullopt);
    HandlePlacement handle = *foldingDoor_->handle();
    handle.anchor = level == RowLevel::Base ? HandleAnchor::Top : HandleAnchor::Bottom;
    foldingDoor_->setHandle(handle);
    setShelfCount(1);
}

void CornerCabinet::setLegs(Real alongBack, Real alongSide)
{
    if (!(alongBack > 0.0) || !(alongSide > 0.0)) {
        throw std::invalid_argument("CornerCabinet: the legs must be longer than 0");
    }
    legBack_ = alongBack;
    legSide_ = alongSide;
    requestLayout();
}

void CornerCabinet::setShelfCount(int count)
{
    if (count < 0 || count > kMaxShelves) {
        throw std::invalid_argument("CornerCabinet: shelves 0.." + std::to_string(kMaxShelves));
    }
    while (shelfCount() < count) {
        auto shelf = std::make_shared<CornerShelf>();
        // After the other shelves: the tree reads walls, bottom, top, backs, shelves, doors.
        adoptAt(6 + shelves_.size(), shelf);
        shelves_.push_back(std::move(shelf));
    }
    while (shelfCount() > count) {
        const CornerShelf* last = shelves_.back().get();
        detachChildren([last](const FurnitureComponent& c) { return &c == last; });
        shelves_.pop_back();
    }
    requestLayout();
}

std::vector<CornerShelf*> CornerCabinet::shelves() const
{
    std::vector<CornerShelf*> all;
    for (const auto& shelf : shelves_) {
        all.push_back(shelf.get());
    }
    return all;
}

std::vector<PanelComponent*> CornerCabinet::frameParts() const
{
    return {side_.get(), endWall_.get(), bottom_.get(), top_.get()};
}

Vec3 CornerCabinet::updateSize(const LayoutSlot& slot)
{
    return {slot.box.size().x, slot.box.size().y, legSide_};
}

void CornerCabinet::layoutChildren(const LayoutSlot& slot)
{
    const ConstructionRules& r = slot.r();
    const Aabb& whole = box();
    const bool right = end_ == Side::Right;
    const Real depth = slot.box.size().z; // of the run (or of the wall cabinets)
    const Real z0 = whole.min().z;
    const Real legEnd = z0 + legSide_;
    const Real frontZ = z0 + depth; // the back leg's front

    // Across the back leg, as the distance u from the side wall: the corner at u = 0, the end
    // of the back leg at legBack_. The side leg's front is at u = depth.
    const Real corner = right ? whole.max().x : whole.min().x;
    const auto across = [&](Real u0, Real u1) {
        return right ? std::pair(corner - u1, corner - u0) : std::pair(corner + u0, corner + u1);
    };
    const auto boxOf = [&](Real u0, Real u1, Real y0, Real y1, Real za, Real zb) {
        const auto [x0, x1] = across(u0, u1);
        return Aabb(Vec3(x0, y0, za), Vec3(x1, y1, zb));
    };

    const Real plinthHeight =
        level_ == RowLevel::Base ? std::clamp(r.kitchen.plinthHeight, 0.0, whole.size().y) : 0.0;
    const Real y0 = whole.min().y + plinthHeight; // the carcass stands on its plinth
    const Real y1 = whole.max().y;

    // 1. The back leg as a carcass of its own: its wall, its back and its plinth have their
    //    usual formulas. The bottom carries the walls, the top stands between them.
    frame_.outer = boxOf(0.0, legBack_, y0, y1, z0, frontZ);
    frame_.leftThickness = frame_.rightThickness = side_->thickness(slot);
    frame_.topThickness = top_->thickness(slot);
    frame_.bottomThickness = bottom_->thickness(slot);
    frame_.topJoint = JointType::Inset;
    frame_.bottomJoint = JointType::Overlay;
    frame_.top = CarcassTop::Panel;
    frame_.backMount = backMount_.get();
    frame_.backThickness = back_->thickness(slot);

    LayoutSlot partSlot = slot.withBox(frame_.outer);
    partSlot.corner = nullptr;
    partSlot.frame = &frame_;
    partSlot.frontZ = frontZ;
    side_->layout(partSlot);
    back_->layout(partSlot);

    const Real tb = frame_.backThickness;
    const Real tWall = side_->thickness(slot);
    const Real tEnd = endWall_->thickness(slot);
    const Real tBottom = frame_.bottomThickness;
    const Real tTop = frame_.topThickness;
    const Real inset = r.backOverlayInset;
    const auto place = [&](PanelComponent& part, const Aabb& box) {
        LayoutSlot s = slot.withBox(box);
        s.corner = nullptr;
        s.frame = nullptr;
        part.layout(s);
    };

    // 2. The side leg: its back nailed on along the side wall (after the back leg's back), its
    //    wall at its end standing on the bottom.
    place(*endBack_, boxOf(0.0, tb, y0 + inset, y1 - inset, z0 + tb, legEnd - inset));
    place(*endWall_, boxOf(tb, depth, y0 + tBottom, y1, legEnd - tEnd, legEnd));

    // 3. The L-shaped bottom and top: rectangles with the open corner notched away. In panel
    //    coordinates x runs along X, y along Z, both from the box minimum: the notch is at the
    //    far end of the back leg (W1 at a right corner) and towards the room (L2).
    const PanelCorner notchCorner = right ? PanelCorner::L2W1 : PanelCorner::L2W2;
    const auto lShape = [&](LShapedPart& part, Real uEnd, Real zEnd, Real frontU, Real frontAtZ) {
        part.setNotch(CornerNotch{notchCorner, uEnd - frontU, zEnd - frontAtZ});
    };
    lShape(*bottom_, legBack_, legEnd, depth, frontZ);
    place(*bottom_, boxOf(tb, legBack_, y0, y0 + tBottom, z0 + tb, legEnd));
    lShape(*top_, legBack_ - tWall, legEnd - tEnd, depth, frontZ);
    place(*top_, boxOf(tb, legBack_ - tWall, y1 - tTop, y1, z0 + tb, legEnd - tEnd));

    // 4. Shelves: inside the walls and the backs, behind both fronts; evenly spread unless one
    //    has its own elevation; a shelf on supports stands on a System 32 hole.
    const Real interiorBottom = y0 + tBottom;
    const Real interiorTop = y1 - tTop;
    const Real gridOrigin = y0 + tBottom / 2.0;
    for (std::size_t i = 0; i < shelves_.size(); ++i) {
        CornerShelf& shelf = *shelves_[i];
        const ShelfOffsets o = shelfOffsets(shelf.mount(), r);
        const Real t = shelf.thickness(slot);
        const Real share = (interiorTop - interiorBottom) * static_cast<Real>(i + 1)
                         / static_cast<Real>(shelves_.size() + 1);
        Real y = shelf.elevation() ? interiorBottom + *shelf.elevation() : interiorBottom + share - t / 2.0;
        const Real pitch = r.system32.pitch;
        if (shelf.mount() == ShelfMount::Supports && r.system32.snapShelves && pitch > 0.0
            && !shelf.elevation()) {
            const Real hole = y - r.shelfSupport.holeBelowShelf;
            y = gridOrigin + std::round((hole - gridOrigin) / pitch) * pitch
              + r.shelfSupport.holeBelowShelf;
        }
        const Real uEnd = legBack_ - tWall - o.side;
        const Real zEnd = legEnd - tEnd - o.side;
        lShape(shelf, uEnd, zEnd, depth - o.front, frontZ - o.front);
        place(shelf, boxOf(tb + o.back, uEnd, y, y + t, z0 + tb + o.back, zEnd));
    }

    // 5. The doors. The first one in front of the back leg, from its end to the side leg's
    //    front; the second one in front of the side leg, from a front gap off the first one's
    //    face to the end — in its own coordinates, those of a run on the side wall (X along the
    //    wall from the back wall, Z from the wall into the room), where it is an ordinary door.
    const Real edgeGap = r.frontEdgeGap;
    const Real gap = r.frontGap;
    const Real frontThickness = foldingDoor_->thickness(slot);
    LayoutSlot doorSlot = slot.withBox(boxOf(depth + gap, legBack_ - edgeGap, y0 + edgeGap,
                                             y1 - edgeGap, frontZ, frontZ));
    doorSlot.corner = nullptr;
    doorSlot.frame = nullptr;
    doorSlot.frontZ = frontZ;
    door_->layout(doorSlot);

    const Real along0 = depth + frontThickness + gap; // from the back wall, along the side wall
    const Real along1 = legSide_ - edgeGap;
    Placement leg;
    Aabb local;
    if (right) {
        leg = Placement{3, Vec3(corner, 0.0, z0)};
        local = Aabb(Vec3(along0, y0 + edgeGap, depth), Vec3(along1, y1 - edgeGap, depth));
    } else {
        leg = Placement{1, Vec3(corner, 0.0, legEnd)};
        local = Aabb(Vec3(legSide_ - along1, y0 + edgeGap, depth),
                     Vec3(legSide_ - along0, y1 - edgeGap, depth));
    }
    foldingDoor_->setPlacement(leg);
    LayoutSlot foldSlot = slot.withBox(local);
    foldSlot.corner = nullptr;
    foldSlot.frame = nullptr;
    foldSlot.frontZ = depth;
    foldingDoor_->layout(foldSlot);

    // 6. A base cabinet's plinth turns the inner corner: the back leg's runs to the back of the
    //    side leg's, which starts at its face.
    if (plinth_) {
        const Real setback = r.kitchen.plinthSetback;
        const Real tPlinth = plinth_->thickness(slot);
        LayoutSlot plinthSlot = partSlot;
        plinthSlot.box = boxOf(depth - setback - tPlinth, legBack_, whole.min().y, y0, z0, frontZ);
        plinth_->layout(plinthSlot);
        place(*endPlinth_, boxOf(depth - setback - tPlinth, depth - setback, whole.min().y, y0,
                                 frontZ - setback, legEnd));
    }
}

} // namespace rc::core
