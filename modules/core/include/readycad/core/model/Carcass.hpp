#pragma once

#include "readycad/core/model/BackMount.hpp"
#include "readycad/core/model/FurnitureComponent.hpp"
#include "readycad/core/model/Parts.hpp"
#include "readycad/core/model/Section.hpp"

#include <memory>

namespace rc::core {

/// Корпус: two side walls, top, bottom, back panel and one interior Section.
///
/// Layout order — "sizes go up":
///   1. structural parts compute themselves from the CarcassFrame (joints, back mount);
///   2. the interior opening is READ from their resulting boxes (not recomputed), so a new
///      joint type or a thicker top never needs a second copy of a formula;
///   3. the interior Section gets that opening plus front overlays for the outer walls.
///
/// Its structure is fixed: add shelves, partitions, doors and drawers through interior().
class Carcass final : public FurnitureComponent {
public:
    Carcass();
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::Carcass; }

    [[nodiscard]] JointType topJoint() const noexcept { return topJoint_; }
    [[nodiscard]] JointType bottomJoint() const noexcept { return bottomJoint_; }
    [[nodiscard]] BackMountType backMountType() const noexcept { return backMount_->type(); }
    void setTopJoint(JointType joint);
    void setBottomJoint(JointType joint);
    void setBackMount(BackMountType type);

    [[nodiscard]] SideWall& leftSide() const noexcept { return *left_; }
    [[nodiscard]] SideWall& rightSide() const noexcept { return *right_; }
    [[nodiscard]] TopPanel& top() const noexcept { return *top_; }
    [[nodiscard]] BottomPanel& bottom() const noexcept { return *bottom_; }
    [[nodiscard]] BackPanel& back() const noexcept { return *back_; }
    [[nodiscard]] Section& interior() const noexcept { return *interior_; }

    /// Free space inside the carcass (between walls, in front of the back), product coords.
    [[nodiscard]] const Aabb& interiorBox() const noexcept { return interior_->box(); }
    [[nodiscard]] const CarcassFrame& frame() const noexcept { return frame_; }

protected:
    void layoutChildren(const LayoutSlot& slot) override;

private:
    JointType topJoint_ = JointType::Overlay;
    JointType bottomJoint_ = JointType::Inset;
    std::unique_ptr<BackMount> backMount_;
    CarcassFrame frame_;

    std::shared_ptr<SideWall> left_;
    std::shared_ptr<SideWall> right_;
    std::shared_ptr<TopPanel> top_;
    std::shared_ptr<BottomPanel> bottom_;
    std::shared_ptr<BackPanel> back_;
    std::shared_ptr<Section> interior_;
};

} // namespace rc::core
