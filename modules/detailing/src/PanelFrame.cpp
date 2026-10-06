#include "readycad/detailing/PanelFrame.hpp"

#include <stdexcept>

namespace rc::detailing {

using core::Axis;
using core::PanelFace;
using core::Vec3;

namespace {

const core::Panel& requirePanel(const core::FurnitureComponent& component)
{
    if (!component.hasPanel()) {
        throw std::invalid_argument("PanelFrame: component has no panel");
    }
    return *component.panel();
}

} // namespace

PanelFrame::PanelFrame(const core::FurnitureComponent& component)
    : box_(component.box())
    , length_(requirePanel(component).lengthAxis())
    , width_(requirePanel(component).widthAxis())
    , thickness_(requirePanel(component).thicknessAxis())
{
}

Vec3 PanelFrame::extents() const noexcept
{
    const Vec3 size = box_.size();
    return {size[index(length_)], size[index(width_)], size[index(thickness_)]};
}

Vec3 PanelFrame::toLocal(const Vec3& world) const noexcept
{
    const Vec3 d = world - box_.min();
    return {d[index(length_)], d[index(width_)], d[index(thickness_)]};
}

Vec3 PanelFrame::toWorld(const Vec3& local) const noexcept
{
    Vec3 world = box_.min();
    world[index(length_)] += local.x;
    world[index(width_)] += local.y;
    world[index(thickness_)] += local.z;
    return world;
}

PanelFace PanelFrame::faceAt(Axis worldAxis, bool atMax) const noexcept
{
    if (worldAxis == thickness_) {
        return atMax ? PanelFace::Top : PanelFace::Bottom;
    }
    if (worldAxis == length_) {
        return atMax ? PanelFace::EdgeW2 : PanelFace::EdgeW1;
    }
    return atMax ? PanelFace::EdgeL2 : PanelFace::EdgeL1;
}

PanelFace PanelFrame::faceToward(const Vec3& worldPoint) const noexcept
{
    const int t = index(thickness_);
    return worldPoint[t] > box_.center()[t] ? PanelFace::Top : PanelFace::Bottom;
}

Vec3 PanelFrame::projectOnto(PanelFace face, Vec3 local) const noexcept
{
    const Vec3 size = extents();
    switch (face) {
    case PanelFace::Top:    local.z = size.z; break;
    case PanelFace::Bottom: local.z = 0.0; break;
    case PanelFace::EdgeL1: local.y = 0.0; break;
    case PanelFace::EdgeL2: local.y = size.y; break;
    case PanelFace::EdgeW1: local.x = 0.0; break;
    case PanelFace::EdgeW2: local.x = size.x; break;
    }
    return local;
}

PanelFace opposite(PanelFace face) noexcept
{
    switch (face) {
    case PanelFace::Top:    return PanelFace::Bottom;
    case PanelFace::Bottom: return PanelFace::Top;
    case PanelFace::EdgeL1: return PanelFace::EdgeL2;
    case PanelFace::EdgeL2: return PanelFace::EdgeL1;
    case PanelFace::EdgeW1: return PanelFace::EdgeW2;
    case PanelFace::EdgeW2: return PanelFace::EdgeW1;
    }
    return face;
}

} // namespace rc::detailing
