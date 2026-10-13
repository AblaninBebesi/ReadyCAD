#include "readycad/detailing/Joints.hpp"

#include "readycad/core/model/Parts.hpp"

#include <glm/common.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace rc::detailing {

using core::Axis;
using core::ComponentKind;
using core::Real;

bool takesConnectors(const core::FurnitureComponent& component)
{
    switch (component.kind()) {
    case ComponentKind::SideWall:
    case ComponentKind::TopPanel:
    case ComponentKind::BottomPanel:
    case ComponentKind::FixedShelf:
    case ComponentKind::Partition:
    case ComponentKind::DrawerBoxSide:
    case ComponentKind::DrawerBoxEnd:
    case ComponentKind::Rail:
        return true;
    case ComponentKind::Shelf:
        return static_cast<const core::Shelf&>(component).mount() == core::ShelfMount::Confirmat;
    default:
        return false;
    }
}

namespace {

constexpr std::array kAxes = {Axis::X, Axis::Y, Axis::Z};

Axis remainingAxis(Axis a, Axis b) noexcept
{
    return static_cast<Axis>(3 - index(a) - index(b));
}

/// Tries to build a joint between two touching panels; false if they do not form one.
bool makeJoint(core::PanelComponent& a, core::PanelComponent& b, Real tol, Joint& out)
{
    const core::Aabb& ba = a.box();
    const core::Aabb& bb = b.box();

    for (const Axis axis : kAxes) {
        const int n = index(axis);
        Real plane = 0.0;
        if (std::abs(ba.max()[n] - bb.min()[n]) <= tol) {
            plane = ba.max()[n];
        } else if (std::abs(bb.max()[n] - ba.min()[n]) <= tol) {
            plane = bb.max()[n];
        } else {
            continue;
        }

        // The contact must be an area, not a line or a corner.
        core::Vec3 lo = glm::max(ba.min(), bb.min());
        core::Vec3 hi = glm::min(ba.max(), bb.max());
        bool isArea = true;
        for (const Axis other : kAxes) {
            if (other != axis && hi[index(other)] - lo[index(other)] <= tol) {
                isArea = false;
            }
        }
        if (!isArea) {
            continue;
        }

        // Exactly one of them must present its face (thickness along the normal).
        const bool aIsFace = a.panel()->thicknessAxis() == axis;
        const bool bIsFace = b.panel()->thicknessAxis() == axis;
        if (aIsFace == bIsFace) {
            return false; // face-to-face or edge-to-edge: not a connector joint
        }

        lo[n] = plane;
        hi[n] = plane;
        out.face = aIsFace ? &a : &b;
        out.edge = aIsFace ? &b : &a;
        out.normal = axis;
        out.plane = plane;
        out.contact = core::Aabb(lo, hi);
        out.across = out.edge->panel()->thicknessAxis();
        out.along = remainingAxis(axis, out.across);
        return true;
    }
    return false;
}

} // namespace

std::vector<Joint> findJoints(core::FurnitureComponent& root, Real tolerance)
{
    std::vector<core::PanelComponent*> parts;
    for (core::PanelComponent* part : core::findAll<core::PanelComponent>(root)) {
        if (takesConnectors(*part)) {
            parts.push_back(part);
        }
    }

    std::vector<Joint> joints;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        for (std::size_t j = i + 1; j < parts.size(); ++j) {
            Joint joint;
            if (makeJoint(*parts[i], *parts[j], tolerance, joint)) {
                joints.push_back(joint);
            }
        }
    }
    return joints;
}

std::vector<Real> connectorPositions(Real from, Real to, const core::ConstructionRules::Joints& rules)
{
    const Real length = to - from;
    if (length <= 0.0) {
        return {};
    }
    if (length <= 2.0 * rules.endOffset) {
        return {from + length / 2.0};
    }
    const Real inner = length - 2.0 * rules.endOffset;
    const int segments = std::max(1, static_cast<int>(std::ceil(inner / rules.maxSpacing - 1e-9)));
    std::vector<Real> positions;
    for (int k = 0; k <= segments; ++k) {
        positions.push_back(from + rules.endOffset + inner * k / segments);
    }
    return positions;
}

} // namespace rc::detailing
