#include "readycad/core/model/Parts.hpp"

#include <stdexcept>
#include <string>

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

Vec3 SideWall::updateSize(const LayoutSlot& slot)
{
    const CarcassFrame& f = requireFrame(slot, kind());
    const Vec3 outer = f.outer.size();
    const Real height = outer.y - overlayAmount(f.topJoint, f.thickness)
                      - overlayAmount(f.bottomJoint, f.thickness);
    return {thickness(slot), height, outer.z - f.rearOffset()};
}

Vec3 SideWall::updatePosition(const LayoutSlot& slot, const Vec3& size)
{
    const CarcassFrame& f = requireFrame(slot, kind());
    const Vec3& lo = f.outer.min();
    const Real x = side_ == Side::Left ? lo.x : f.outer.max().x - size.x;
    return {x, lo.y + overlayAmount(f.bottomJoint, f.thickness), lo.z + f.rearOffset()};
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
    const Real width = joint == JointType::Overlay ? outer.x : outer.x - 2.0 * f.thickness;
    return {width, ownThickness, outer.z - f.rearOffset()};
}

Real horizontalCarcassPanelX(const CarcassFrame& f, JointType joint)
{
    return f.outer.min().x + (joint == JointType::Inset ? f.thickness : 0.0);
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

namespace {

struct ShelfOffsets {
    Real side;  // per side
    Real front; // from the front edge of the band
    Real back;  // from the rear of the band (the back panel)
};

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

} // namespace

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

FrontPanel::FrontPanel(Axis lengthAxis) : PanelComponent(Axis::Z, lengthAxis) {}

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

Door::Door(HingeSide hinge) : FrontPanel(Axis::Y), hinge_(hinge) {}

DrawerFront::DrawerFront() : FrontPanel(Axis::X) {}

// ---------------------------------------------------------------------------------------------
// Drawer box
// ---------------------------------------------------------------------------------------------

DrawerBoxSide::DrawerBoxSide(Side side) : PanelComponent(Axis::X, Axis::Z), side_(side) {}

Vec3 DrawerBoxSide::updateSize(const LayoutSlot& slot)
{
    const Vec3 body = slot.box.size();
    return {thickness(slot), body.y - slot.r().drawerBottomMaterial.thickness, body.z};
}

Vec3 DrawerBoxSide::updatePosition(const LayoutSlot& slot, const Vec3& size)
{
    const Vec3& lo = slot.box.min();
    const Real x = side_ == Side::Left ? lo.x : slot.box.max().x - size.x;
    return {x, lo.y + slot.r().drawerBottomMaterial.thickness, lo.z};
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
    const ConstructionRules& r = slot.r();
    const Vec3 body = slot.box.size();
    return {body.x - 2.0 * r.drawerBoxMaterial.thickness,
            body.y - r.drawerBottomMaterial.thickness, thickness(slot)};
}

Vec3 DrawerBoxEnd::updatePosition(const LayoutSlot& slot, const Vec3& size)
{
    const ConstructionRules& r = slot.r();
    const Vec3& lo = slot.box.min();
    const Real z = end_ == DrawerEnd::Back ? lo.z : slot.box.max().z - size.z;
    return {lo.x + r.drawerBoxMaterial.thickness, lo.y + r.drawerBottomMaterial.thickness, z};
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
