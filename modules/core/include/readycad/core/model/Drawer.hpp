#pragma once

#include "readycad/core/model/FurnitureComponent.hpp"
#include "readycad/core/model/Parts.hpp"

#include <memory>
#include <vector>

namespace rc::core {

/// Выдвижной ящик: an overlay front plus a box (two sides, front/back ends, HDF bottom).
///
/// Slot: `box` = the drawer's zone inside the section (where the box body may go),
/// `overlays` = how far its front extends beyond that zone.
/// The body is: zone narrowed by the slide clearances, as deep as the longest standard slide
/// that fits, as high as the zone minus top/bottom clearances (capped).
class Drawer final : public FurnitureComponent {
public:
    Drawer();
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::Drawer; }

    /// Outer box of the drawer body (without the front), product coordinates.
    [[nodiscard]] const Aabb& body() const noexcept { return body_; }
    /// Chosen slide length, mm (0 if no standard slide fits).
    [[nodiscard]] Real slideLength() const noexcept { return slideLength_; }

    [[nodiscard]] DrawerFront& front() const noexcept { return *front_; }

protected:
    void layoutChildren(const LayoutSlot& slot) override;

private:
    std::shared_ptr<DrawerFront> front_;
    Aabb body_;
    Real slideLength_ = 0.0;
};

/// Блок ящиков: N drawers stacked bottom to top with equal front heights and frontGap between.
class DrawerStack final : public FurnitureComponent {
public:
    explicit DrawerStack(int count = 0);
    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::DrawerStack;
    }

    [[nodiscard]] int count() const noexcept { return static_cast<int>(children().size()); }
    /// Rebuilds the stack with `count` drawers.
    void setCount(int count);
    [[nodiscard]] std::vector<Drawer*> drawers() const;

    [[nodiscard]] bool canAccept(const FurnitureComponent& child) const override;

protected:
    void layoutChildren(const LayoutSlot& slot) override;
};

} // namespace rc::core
