#include "readycad/core/model/BackMount.hpp"

#include <stdexcept>

namespace rc::core {

namespace {

/// Rear opening of the carcass: between the side walls and between bottom and top panels.
/// Bottom always stands on the floor and the top is always at the top, whatever the joints are.
Aabb rearOpening(const CarcassFrame& frame)
{
    const Vec3& lo = frame.outer.min();
    const Vec3& hi = frame.outer.max();
    return Aabb(Vec3(lo.x + frame.leftThickness, lo.y + frame.bottomThickness, lo.z),
                Vec3(hi.x - frame.rightThickness, hi.y - frame.topThickness, hi.z));
}

class GrooveMount final : public BackMount {
public:
    BackMountType type() const noexcept override { return BackMountType::Groove; }

    Real carcassRearOffset(Real) const noexcept override { return 0.0; }

    Aabb backPanelBox(const CarcassFrame& frame, const ConstructionRules& rules,
                      Real backThickness) const override
    {
        // The back enters every groove by (groove depth - expansion gap): the gap lets HDF
        // swell with humidity without pushing the carcass apart.
        const Aabb opening = rearOpening(frame);
        const Real reach = rules.backGrooveDepth - rules.backExpansionGap;
        const Real z0 = frame.outer.min().z + rules.backGrooveOffset;
        return Aabb(Vec3(opening.min().x - reach, opening.min().y - reach, z0),
                    Vec3(opening.max().x + reach, opening.max().y + reach, z0 + backThickness));
    }

    bool isEmbeddedInto(ComponentKind kind) const noexcept override
    {
        return kind == ComponentKind::SideWall || kind == ComponentKind::TopPanel
            || kind == ComponentKind::BottomPanel;
    }
};

class OverlayMount final : public BackMount {
public:
    BackMountType type() const noexcept override { return BackMountType::Overlay; }

    // The back occupies the rearmost `backThickness` of the carcass depth.
    Real carcassRearOffset(Real backThickness) const noexcept override { return backThickness; }

    Aabb backPanelBox(const CarcassFrame& frame, const ConstructionRules& rules,
                      Real backThickness) const override
    {
        const Vec3& lo = frame.outer.min();
        const Vec3& hi = frame.outer.max();
        const Real inset = rules.backOverlayInset;
        return Aabb(Vec3(lo.x + inset, lo.y + inset, lo.z),
                    Vec3(hi.x - inset, hi.y - inset, lo.z + backThickness));
    }

    bool isEmbeddedInto(ComponentKind) const noexcept override { return false; }
};

class InsetMount final : public BackMount {
public:
    BackMountType type() const noexcept override { return BackMountType::Inset; }

    Real carcassRearOffset(Real) const noexcept override { return 0.0; }

    Aabb backPanelBox(const CarcassFrame& frame, const ConstructionRules&,
                      Real backThickness) const override
    {
        const Aabb opening = rearOpening(frame);
        const Real z0 = frame.outer.min().z;
        return Aabb(Vec3(opening.min().x, opening.min().y, z0),
                    Vec3(opening.max().x, opening.max().y, z0 + backThickness));
    }

    bool isEmbeddedInto(ComponentKind) const noexcept override { return false; }
};

} // namespace

std::unique_ptr<BackMount> makeBackMount(BackMountType type)
{
    switch (type) {
    case BackMountType::Groove:
        return std::make_unique<GrooveMount>();
    case BackMountType::Overlay:
        return std::make_unique<OverlayMount>();
    case BackMountType::Inset:
        return std::make_unique<InsetMount>();
    }
    throw std::invalid_argument("Unknown BackMountType");
}

Real CarcassFrame::rearOffset() const noexcept
{
    return backMount != nullptr ? backMount->carcassRearOffset(backThickness) : 0.0;
}

} // namespace rc::core
