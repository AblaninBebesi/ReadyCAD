#include "readycad/core/model/Parts.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace rc::core {

namespace {

const CarcassFrame& requireFrame(const LayoutSlot& slot, ComponentKind kind)
{
    if (slot.frame == nullptr) {
        throw std::logic_error(std::string(toString(kind)) + " must be laid out inside a Carcass");
    }
    return *slot.frame;
}

Real overlayAmount(JointType joint, Real thickness) noexcept
{
    return joint == JointType::Overlay ? thickness : 0.0;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// SideWall
// ---------------------------------------------------------------------------------------------

SideWall::SideWall(Side side) : PanelComponent(Axis::X, Axis::Y), side_(side) {}

SideWall::SideWall(Side side, Axis thicknessAxis, Axis lengthAxis)
    : PanelComponent(thicknessAxis, lengthAxis), side_(side)
{
}

Vec3 SideWall::updateSize(const LayoutSlot& slot)
{
    const CarcassFrame& f = requireFrame(slot, kind());
    const Vec3 outer = f.outer.size();
    const Real height = outer.y - overlayAmount(f.topJoint, f.topThickness)
                      - overlayAmount(f.bottomJoint, f.bottomThickness);
    return {thickness(slot), height, outer.z - f.rearOffset()};
}

Vec3 SideWall::updatePosition(const LayoutSlot& slot, const Vec3& size)
{
    const CarcassFrame& f = requireFrame(slot, kind());
    const Vec3& lo = f.outer.min();
    const Real x = side_ == Side::Left ? lo.x : f.outer.max().x - size.x;
    return {x, lo.y + overlayAmount(f.bottomJoint, f.bottomThickness), lo.z + f.rearOffset()};
}

const Material& SideWall::defaultMaterial(const ConstructionRules& r) const
{
    return r.carcassMaterial;
}

EdgeBand SideWall::defaultEdge(EdgeSide side, const LayoutSlot& slot) const
{
    // Width axis is Z: L2 is the front edge.
    return side == EdgeSide::L2 ? slot.r().visibleEdge : EdgeBand{};
}

// ---------------------------------------------------------------------------------------------
// TopPanel / BottomPanel
// ---------------------------------------------------------------------------------------------

namespace {

Vec3 horizontalCarcassPanelSize(const CarcassFrame& f, JointType joint, Real ownThickness)
{
    const Vec3 outer = f.outer.size();
    const Real width =
        joint == JointType::Overlay ? outer.x : outer.x - f.leftThickness - f.rightThickness;
    return {width, ownThickness, outer.z - f.rearOffset()};
}

Real horizontalCarcassPanelX(const CarcassFrame& f, JointType joint)
{
    return f.outer.min().x + (joint == JointType::Inset ? f.leftThickness : 0.0);
}

EdgeBand horizontalCarcassPanelEdge(EdgeSide side, JointType joint, const ConstructionRules& r)
{
    if (side == EdgeSide::L2) {
        return r.visibleEdge; // front edge
    }
    // Overlay panels show their ends on the outside of the carcass.
    if (joint == JointType::Overlay && (side == EdgeSide::W1 || side == EdgeSide::W2)) {
        return r.visibleEdge;
    }
    return {};
}

} // namespace

TopPanel::TopPanel() : PanelComponent(Axis::Y, Axis::X) {}

Vec3 TopPanel::updateSize(const LayoutSlot& slot)
{
    const CarcassFrame& f = requireFrame(slot, kind());
    return horizontalCarcassPanelSize(f, f.topJoint, thickness(slot));
}

Vec3 TopPanel::updatePosition(const LayoutSlot& slot, const Vec3& size)
{
    const CarcassFrame& f = requireFrame(slot, kind());
    return {horizontalCarcassPanelX(f, f.topJoint), f.outer.max().y - size.y,
            f.outer.min().z + f.rearOffset()};
}

const Material& TopPanel::defaultMaterial(const ConstructionRules& r) const
{
    return r.carcassMaterial;
}

EdgeBand TopPanel::defaultEdge(EdgeSide side, const LayoutSlot& slot) const
{
    return horizontalCarcassPanelEdge(side, requireFrame(slot, kind()).topJoint, slot.r());
}

BottomPanel::BottomPanel() : PanelComponent(Axis::Y, Axis::X) {}

Vec3 BottomPanel::updateSize(const LayoutSlot& slot)
{
    const CarcassFrame& f = requireFrame(slot, kind());
    return horizontalCarcassPanelSize(f, f.bottomJoint, thickness(slot));
}

Vec3 BottomPanel::updatePosition(const LayoutSlot& slot, const Vec3&)
{
    const CarcassFrame& f = requireFrame(slot, kind());
    return {horizontalCarcassPanelX(f, f.bottomJoint), f.outer.min().y,
            f.outer.min().z + f.rearOffset()};
}

const Material& BottomPanel::defaultMaterial(const ConstructionRules& r) const
{
    return r.carcassMaterial;
}

EdgeBand BottomPanel::defaultEdge(EdgeSide side, const LayoutSlot& slot) const
{
    return horizontalCarcassPanelEdge(side, requireFrame(slot, kind()).bottomJoint, slot.r());
}

// ---------------------------------------------------------------------------------------------
// BackPanel
// ---------------------------------------------------------------------------------------------

BackPanel::BackPanel() : PanelComponent(Axis::Z, Axis::Y) {}

Aabb BackPanel::computeBox(const LayoutSlot& slot) const
{
    const CarcassFrame& f = requireFrame(slot, kind());
    if (f.backMount == nullptr) {
        throw std::logic_error("BackPanel: carcass has no BackMount");
    }
    return f.backMount->backPanelBox(f, slot.r(), thickness(slot));
}

Vec3 BackPanel::updateSize(const LayoutSlot& slot)
{
    mountType_ = requireFrame(slot, kind()).backMount->type();
    return computeBox(slot).size();
}

Vec3 BackPanel::updatePosition(const LayoutSlot& slot, const Vec3&)
{
    return computeBox(slot).min();
}

const Material& BackPanel::defaultMaterial(const ConstructionRules& r) const
{
    return r.backMaterial;
}

bool BackPanel::allowsOverlapWith(const FurnitureComponent& other) const
{
    return mountType_ && makeBackMount(*mountType_)->isEmbeddedInto(other.kind());
}

// ---------------------------------------------------------------------------------------------
// Rail / Plinth
// ---------------------------------------------------------------------------------------------

Rail::Rail(RailPosition position, bool upright)
    : PanelComponent(upright ? Axis::Z : Axis::Y, Axis::X), position_(position), upright_(upright)
{
}

Vec3 Rail::updateSize(const LayoutSlot& slot)
{
    const CarcassFrame& f = requireFrame(slot, kind());
    const Real length = f.outer.size().x - f.leftThickness - f.rightThickness;
    const Real width = slot.r().kitchen.railWidth;
    return upright_ ? Vec3(length, width, thickness(slot)) : Vec3(length, thickness(slot), width);
}

Vec3 Rail::updatePosition(const LayoutSlot& slot, const Vec3& size)
{
    const CarcassFrame& f = requireFrame(slot, kind());
    const Vec3& lo = f.outer.min();
    const Vec3& hi = f.outer.max();
    Real z = hi.z - size.z; // the front rail is flush with the carcass front edge
    if (position_ == RailPosition::Rear) {
        // Flat: on the rear edges like a top panel, the back panel in its groove. Upright: in
        // front of the back panel, which then reaches up to the top.
        z = upright_ && f.backMount != nullptr
                ? f.backMount->backPanelBox(f, slot.r(), f.backThickness).max().z
                : lo.z + f.rearOffset();
    }
    return {lo.x + f.leftThickness, hi.y - size.y, z};
}

const Material& Rail::defaultMaterial(const ConstructionRules& r) const
{
    return r.carcassMaterial;
}

EdgeBand Rail::defaultEdge(EdgeSide side, const LayoutSlot& slot) const
{
    if (position_ == RailPosition::Rear) {
        return {};
    }
    // Flat: L2 is the front edge. Upright: L1 is the lower edge, seen under the worktop when a
    // door or the top drawer is open.
    const EdgeSide visible = upright_ ? EdgeSide::L1 : EdgeSide::L2;
    return side == visible ? slot.r().visibleEdge : EdgeBand{};
}

Plinth::Plinth() : PanelComponent(Axis::Z, Axis::X) {}

Vec3 Plinth::updateSize(const LayoutSlot& slot)
{
    const Vec3 zone = slot.box.size();
    return {zone.x, zone.y, thickness(slot)};
}

Vec3 Plinth::updatePosition(const LayoutSlot& slot, const Vec3& size)
{
    const CarcassFrame& f = requireFrame(slot, kind());
    const Real face = f.outer.max().z - slot.r().kitchen.plinthSetback;
    return {slot.box.min().x, slot.box.min().y, face - size.z};
}

const Material& Plinth::defaultMaterial(const ConstructionRules& r) const
{
    return r.carcassMaterial;
}

EdgeBand Plinth::defaultEdge(EdgeSide side, const LayoutSlot& slot) const
{
    // Width axis is Y: L1 stands on the floor and takes the moisture of mopping.
    return side == EdgeSide::L1 ? slot.r().hiddenEdge : EdgeBand{};
}

// ---------------------------------------------------------------------------------------------
// Shelf (adjustable)
// ---------------------------------------------------------------------------------------------

Shelf::Shelf(std::optional<Real> elevation, ShelfMount mount)
    : PanelComponent(Axis::Y, Axis::X), elevation_(elevation), mount_(mount)
{
}

void Shelf::setElevation(std::optional<Real> elevation)
{
    elevation_ = elevation;
    requestLayout();
}

void Shelf::setMount(ShelfMount mount)
{
    mount_ = mount;
    requestLayout();
}

ShelfOffsets shelfOffsets(ShelfMount mount, const ConstructionRules& r) noexcept
{
    switch (mount) {
    case ShelfMount::Confirmat:
        return {0.0, r.confirmatShelfFrontSetback, r.confirmatShelfBackGap};
    case ShelfMount::Supports:
        break;
    }
    return {r.shelfSideGap, r.shelfFrontSetback, r.shelfBackGap};
}

Vec3 Shelf::updateSize(const LayoutSlot& slot)
{
    const ShelfOffsets o = shelfOffsets(mount_, slot.r());
    const Vec3 band = slot.box.size();
    return {band.x - 2.0 * o.side, thickness(slot), band.z - o.front - o.back};
}

Vec3 Shelf::updatePosition(const LayoutSlot& slot, const Vec3&)
{
    const ShelfOffsets o = shelfOffsets(mount_, slot.r());
    const Vec3& lo = slot.box.min();
    return {lo.x + o.side, lo.y, lo.z + o.back};
}

const Material& Shelf::defaultMaterial(const ConstructionRules& r) const
{
    return r.carcassMaterial;
}

EdgeBand Shelf::defaultEdge(EdgeSide side, const LayoutSlot& slot) const
{
    return side == EdgeSide::L2 ? slot.r().visibleEdge : EdgeBand{};
}

// ---------------------------------------------------------------------------------------------
// FixedShelf / Partition
// ---------------------------------------------------------------------------------------------

FixedShelf::FixedShelf() : PanelComponent(Axis::Y, Axis::X) {}

Vec3 FixedShelf::updateSize(const LayoutSlot& slot)
{
    const Vec3 band = slot.box.size();
    return {band.x, thickness(slot), band.z - slot.r().partitionFrontSetback};
}

Vec3 FixedShelf::updatePosition(const LayoutSlot& slot, const Vec3&)
{
    return slot.box.min();
}

const Material& FixedShelf::defaultMaterial(const ConstructionRules& r) const
{
    return r.carcassMaterial;
}

EdgeBand FixedShelf::defaultEdge(EdgeSide side, const LayoutSlot& slot) const
{
    return side == EdgeSide::L2 ? slot.r().visibleEdge : EdgeBand{};
}

Partition::Partition() : PanelComponent(Axis::X, Axis::Y) {}

Vec3 Partition::updateSize(const LayoutSlot& slot)
{
    const Vec3 band = slot.box.size();
    return {thickness(slot), band.y, band.z - slot.r().partitionFrontSetback};
}

Vec3 Partition::updatePosition(const LayoutSlot& slot, const Vec3&)
{
    return slot.box.min();
}

const Material& Partition::defaultMaterial(const ConstructionRules& r) const
{
    return r.carcassMaterial;
}

EdgeBand Partition::defaultEdge(EdgeSide side, const LayoutSlot& slot) const
{
    return side == EdgeSide::L2 ? slot.r().visibleEdge : EdgeBand{};
}

// ---------------------------------------------------------------------------------------------
// Fronts
// ---------------------------------------------------------------------------------------------

FrontPanel::FrontPanel(Axis lengthAxis, HandlePlacement defaultPlacement)
    : PanelComponent(Axis::Z, lengthAxis), handle_(std::move(defaultPlacement))
{
}

void FrontPanel::setHandle(std::optional<HandlePlacement> placement)
{
    handle_ = std::move(placement);
    requestLayout(); // drilling of this front is now stale
}

const HandleModel* FrontPanel::handleModel(const ConstructionRules& rules) const
{
    if (!handle_) {
        return nullptr;
    }
    const std::string& id = handle_->modelId.empty() ? defaultHandleId(rules) : handle_->modelId;
    return rules.handles.find(id);
}

Real FrontPanel::handleCenterX(const HandlePlacement&, const HandleModel&) const
{
    return box().center().x;
}

std::optional<HandleGeometry> FrontPanel::handleGeometry(const ConstructionRules& rules) const
{
    const HandleModel* model = handleModel(rules);
    if (model == nullptr) {
        return std::nullopt;
    }
    const HandlePlacement& p = *handle_;
    const Aabb& front = box();
    const bool vertical = p.orientation == HandleOrientation::Vertical;
    const Real half = model->type == HandleType::Bar ? model->holeSpacing / 2.0 : 0.0;

    Real y = front.center().y;
    if (p.anchor == HandleAnchor::Top) {
        y = front.max().y - p.anchorOffset - (vertical ? half : 0.0);
    } else if (p.anchor == HandleAnchor::Bottom) {
        y = front.min().y + p.anchorOffset + (vertical ? half : 0.0);
    }

    HandleGeometry g;
    g.model = model;
    g.center = Vec3(handleCenterX(p, *model), y, front.max().z); // the outer face looks to +Z
    g.along = vertical ? Axis::Y : Axis::X;
    if (model->type == HandleType::Bar) {
        for (const Real offset : {-half, half}) {
            Vec3 hole = g.center;
            hole[index(g.along)] += offset;
            g.holes.push_back(hole);
        }
    } else {
        g.holes.push_back(g.center);
    }

    // Fit check: holes keep their distance from the edges, the body stays on the front.
    const Real margin = model->holeDiameter / 2.0 + rules.handles.minEdgeDistance;
    for (const Vec3& hole : g.holes) {
        for (const int axis : {0, 1}) {
            if (hole[axis] - margin < front.min()[axis] - kEpsilon
                || hole[axis] + margin > front.max()[axis] + kEpsilon) {
                g.fits = false;
            }
        }
    }
    const int a = index(g.along);
    const int c = 1 - a; // the other in-plane axis (X <-> Y)
    if (g.center[a] - model->length / 2.0 < front.min()[a] - kEpsilon
        || g.center[a] + model->length / 2.0 > front.max()[a] + kEpsilon
        || g.center[c] - model->width / 2.0 < front.min()[c] - kEpsilon
        || g.center[c] + model->width / 2.0 > front.max()[c] + kEpsilon) {
        g.fits = false;
    }
    return g;
}

Vec3 FrontPanel::updateSize(const LayoutSlot& slot)
{
    const Vec3 rect = slot.frontRect().size();
    return {rect.x, rect.y, thickness(slot)};
}

Vec3 FrontPanel::updatePosition(const LayoutSlot& slot, const Vec3&)
{
    const Vec3 lo = slot.frontRect().min(); // copy: frontRect() is a temporary
    return {lo.x, lo.y, slot.frontZ};
}

const Material& FrontPanel::defaultMaterial(const ConstructionRules& r) const
{
    return r.frontMaterial;
}

EdgeBand FrontPanel::defaultEdge(EdgeSide, const LayoutSlot& slot) const
{
    return slot.r().visibleEdge; // every edge of a front is visible
}

Door::Door(HingeSide hinge)
    : FrontPanel(Axis::Y, HandlePlacement{{}, HandleOrientation::Vertical, HandleAnchor::Center})
    , hinge_(hinge)
{
}

void Door::setHinge(HingeSide hinge)
{
    hinge_ = hinge;
    requestLayout(); // hinge cups and the handle move to the other side
}

const std::string& Door::defaultHandleId(const ConstructionRules& r) const
{
    return r.handles.defaultDoorHandle;
}

Real Door::handleCenterX(const HandlePlacement& placement, const HandleModel& model) const
{
    // The handle goes to the opening edge: opposite the hinges.
    const bool hingeLeft = hinge_ == HingeSide::Left;
    const Real openingEdge = hingeLeft ? box().max().x : box().min().x;
    const Real inwards = hingeLeft ? -1.0 : 1.0;
    Real distance = placement.edgeOffset;
    if (placement.orientation == HandleOrientation::Horizontal && model.type == HandleType::Bar) {
        distance += model.holeSpacing / 2.0; // edgeOffset is to the nearest hole
    }
    return openingEdge + inwards * distance;
}

DrawerFront::DrawerFront()
    : FrontPanel(Axis::X, HandlePlacement{{}, HandleOrientation::Horizontal, HandleAnchor::Center})
{
}

const std::string& DrawerFront::defaultHandleId(const ConstructionRules& r) const
{
    return r.handles.defaultDrawerHandle;
}

// ---------------------------------------------------------------------------------------------
// Worktop
// ---------------------------------------------------------------------------------------------

WorktopCutout WorktopCutout::standard(CutoutKind kind) noexcept
{
    switch (kind) {
    case CutoutKind::Hob:
        return {CutoutKind::Hob, 560.0, 490.0, 5.0, 60.0};
    case CutoutKind::Sink:
        break;
    }
    return {CutoutKind::Sink, 560.0, 480.0, 15.0, 60.0};
}

Worktop::Worktop() : PanelComponent(Axis::Y, Axis::X)
{
    setInCuttingPlan(false);
}

Vec3 Worktop::updateSize(const LayoutSlot& slot)
{
    const Vec3 zone = slot.box.size();
    return {zone.x, thickness(slot), zone.z};
}

Vec3 Worktop::updatePosition(const LayoutSlot& slot, const Vec3&)
{
    return slot.box.min();
}

std::vector<PanelCutout> Worktop::automaticOpenings(const LayoutSlot& slot) const
{
    // Panel x runs along X, panel y along Z, both from the box minimum.
    std::vector<PanelCutout> cutouts;
    if (slot.kitchen != nullptr) {
        for (const PanelCutout& c : slot.kitchen->cutouts) {
            cutouts.push_back({Vec2(c.center.x - box().min().x, c.center.y - box().min().z),
                               c.length, c.width, c.radius});
        }
    }
    return cutouts;
}

const Material& Worktop::defaultMaterial(const ConstructionRules& r) const
{
    return r.worktopMaterial;
}

EdgeBand Worktop::defaultEdge(EdgeSide side, const LayoutSlot& slot) const
{
    // Width axis is Z: L1 lies against the wall, the front edge is visible, and so is an end —
    // unless it meets a side wall or the other worktop at a corner.
    const KitchenFrame* k = slot.kitchen;
    const bool closed = side == EdgeSide::L1
                     || (k != nullptr && side == EdgeSide::W1 && k->startClosed)
                     || (k != nullptr && side == EdgeSide::W2 && k->endClosed);
    return closed ? EdgeBand{} : slot.r().visibleEdge;
}

// ---------------------------------------------------------------------------------------------
// Parts of an L corner cabinet
// ---------------------------------------------------------------------------------------------

EndWall::EndWall(Side side) : SideWall(side, Axis::Z, Axis::Y) {}

Vec3 EndWall::updateSize(const LayoutSlot& slot)
{
    return slot.box.size();
}

Vec3 EndWall::updatePosition(const LayoutSlot& slot, const Vec3&)
{
    return slot.box.min();
}

EdgeBand EndWall::defaultEdge(EdgeSide edge, const LayoutSlot& slot) const
{
    // Width axis is X: the front edge looks into the room — towards X min at a right corner
    // (the end wall's hand is right), towards X max at a left one.
    const EdgeSide front = side() == Side::Right ? EdgeSide::L1 : EdgeSide::L2;
    return edge == front ? slot.r().visibleEdge : EdgeBand{};
}

std::vector<ShapeFeature> LShapedPart::notchFeature() const
{
    if (!notch_) {
        return {};
    }
    return {*notch_};
}

CornerShelf::CornerShelf() : Shelf(std::nullopt, ShelfMount::Supports) {}

Vec3 CornerShelf::updateSize(const LayoutSlot& slot)
{
    return slot.box.size();
}

Vec3 CornerShelf::updatePosition(const LayoutSlot& slot, const Vec3&)
{
    return slot.box.min();
}

std::vector<ShapeFeature> CornerShelf::automaticShape(const LayoutSlot&) const
{
    return notchFeature();
}

EdgeBand CornerShelf::defaultEdge(EdgeSide, const LayoutSlot&) const
{
    return {}; // the edges seen are the notch's
}

CornerPanel::CornerPanel(Role role, Axis thicknessAxis, Axis lengthAxis)
    : PanelComponent(thicknessAxis, lengthAxis), role_(role)
{
}

ComponentKind CornerPanel::kind() const noexcept
{
    switch (role_) {
    case Role::Bottom: return ComponentKind::BottomPanel;
    case Role::Top:    return ComponentKind::TopPanel;
    case Role::Back:   return ComponentKind::BackPanel;
    case Role::Plinth: break;
    }
    return ComponentKind::Plinth;
}

Vec3 CornerPanel::updateSize(const LayoutSlot& slot)
{
    return slot.box.size();
}

Vec3 CornerPanel::updatePosition(const LayoutSlot& slot, const Vec3&)
{
    return slot.box.min();
}

std::vector<ShapeFeature> CornerPanel::automaticShape(const LayoutSlot&) const
{
    return notchFeature();
}

const Material& CornerPanel::defaultMaterial(const ConstructionRules& r) const
{
    return role_ == Role::Back ? r.backMaterial : r.carcassMaterial;
}

EdgeBand CornerPanel::defaultEdge(EdgeSide side, const LayoutSlot& slot) const
{
    // A plinth's width axis is Y: L1 stands on the floor. The bottom and the top show only
    // their notch, the back nothing.
    return role_ == Role::Plinth && side == EdgeSide::L1 ? slot.r().hiddenEdge : EdgeBand{};
}

FoldingDoor::FoldingDoor(HingeSide fold) : Door(fold) {}

// ---------------------------------------------------------------------------------------------
// BlindPanel
// ---------------------------------------------------------------------------------------------

BlindPanel::BlindPanel() : PanelComponent(Axis::Z, Axis::Y) {}

Vec3 BlindPanel::updateSize(const LayoutSlot& slot)
{
    const Vec3 zone = slot.box.size();
    return {zone.x, zone.y, thickness(slot)};
}

Vec3 BlindPanel::updatePosition(const LayoutSlot& slot, const Vec3&)
{
    return {slot.box.min().x, slot.box.min().y, slot.frontZ};
}

const Material& BlindPanel::defaultMaterial(const ConstructionRules& r) const
{
    return r.frontMaterial;
}

EdgeBand BlindPanel::defaultEdge(EdgeSide, const LayoutSlot& slot) const
{
    return slot.r().visibleEdge; // a front: every edge is seen, like a door's
}

// ---------------------------------------------------------------------------------------------
// Drawer box
// ---------------------------------------------------------------------------------------------

namespace {

/// The drawer's own box parts, or the workshop's materials for a part laid out on its own.
DrawerBoxFrame boxFrame(const LayoutSlot& slot)
{
    if (slot.drawerBox != nullptr) {
        return *slot.drawerBox;
    }
    const ConstructionRules& r = slot.r();
    return {r.drawerBoxMaterial.thickness, r.drawerBoxMaterial.thickness,
            r.drawerBottomMaterial.thickness};
}

} // namespace

DrawerBoxSide::DrawerBoxSide(Side side) : PanelComponent(Axis::X, Axis::Z), side_(side) {}

Vec3 DrawerBoxSide::updateSize(const LayoutSlot& slot)
{
    const Vec3 body = slot.box.size();
    return {thickness(slot), body.y - boxFrame(slot).bottom, body.z};
}

Vec3 DrawerBoxSide::updatePosition(const LayoutSlot& slot, const Vec3& size)
{
    const Vec3& lo = slot.box.min();
    const Real x = side_ == Side::Left ? lo.x : slot.box.max().x - size.x;
    return {x, lo.y + boxFrame(slot).bottom, lo.z};
}

const Material& DrawerBoxSide::defaultMaterial(const ConstructionRules& r) const
{
    return r.drawerBoxMaterial;
}

EdgeBand DrawerBoxSide::defaultEdge(EdgeSide side, const LayoutSlot& slot) const
{
    // Width axis is Y: L2 is the top edge, visible when the drawer is open.
    return side == EdgeSide::L2 ? slot.r().hiddenEdge : EdgeBand{};
}

DrawerBoxEnd::DrawerBoxEnd(DrawerEnd end) : PanelComponent(Axis::Z, Axis::X), end_(end) {}

Vec3 DrawerBoxEnd::updateSize(const LayoutSlot& slot)
{
    const DrawerBoxFrame f = boxFrame(slot);
    const Vec3 body = slot.box.size();
    return {body.x - f.leftSide - f.rightSide, body.y - f.bottom, thickness(slot)};
}

Vec3 DrawerBoxEnd::updatePosition(const LayoutSlot& slot, const Vec3& size)
{
    const DrawerBoxFrame f = boxFrame(slot);
    const Vec3& lo = slot.box.min();
    const Real z = end_ == DrawerEnd::Back ? lo.z : slot.box.max().z - size.z;
    return {lo.x + f.leftSide, lo.y + f.bottom, z};
}

const Material& DrawerBoxEnd::defaultMaterial(const ConstructionRules& r) const
{
    return r.drawerBoxMaterial;
}

EdgeBand DrawerBoxEnd::defaultEdge(EdgeSide side, const LayoutSlot& slot) const
{
    return side == EdgeSide::L2 ? slot.r().hiddenEdge : EdgeBand{};
}

DrawerBottom::DrawerBottom() : PanelComponent(Axis::Y, Axis::X) {}

Vec3 DrawerBottom::updateSize(const LayoutSlot& slot)
{
    const Vec3 body = slot.box.size();
    return {body.x, thickness(slot), body.z};
}

Vec3 DrawerBottom::updatePosition(const LayoutSlot& slot, const Vec3&)
{
    return slot.box.min();
}

const Material& DrawerBottom::defaultMaterial(const ConstructionRules& r) const
{
    return r.drawerBottomMaterial;
}

} // namespace rc::core
