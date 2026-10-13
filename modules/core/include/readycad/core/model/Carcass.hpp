#pragma once

#include "readycad/core/model/BackMount.hpp"
#include "readycad/core/model/FurnitureComponent.hpp"
#include "readycad/core/model/Parts.hpp"
#include "readycad/core/model/Section.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace rc::core {

/// Корпус: two side walls, a top (a panel or two rails), bottom, back panel and one interior
/// Section; optionally a plinth on legs under it.
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

    /// A top panel (default) or two rails under a worktop. Rails always stand between the side
    /// walls, whatever topJoint() says; switching keeps the top panel's own choices.
    [[nodiscard]] CarcassTop topStyle() const noexcept { return topStyle_; }
    void setTopStyle(CarcassTop style);

    /// A plinth on legs under the carcass (base cabinets of a kitchen). The box of the carcass
    /// keeps its size: the carcass itself starts rules.kitchen.plinthHeight higher.
    [[nodiscard]] bool hasPlinth() const noexcept;
    void setPlinth(bool plinth);

    /// Width of the cabinet in a CabinetRow; nullopt = shares the free width with the other
    /// automatic cabinets. Throws std::invalid_argument for a width <= 0.
    [[nodiscard]] const std::optional<Real>& fixedWidth() const noexcept { return fixedWidth_; }
    void setFixedWidth(std::optional<Real> width);

    /// The opening this cabinet needs in the worktop above it (a sink, a hob). Used by a Kitchen
    /// for its base cabinets. Throws std::invalid_argument for sizes <= 0 or a radius that does
    /// not fit.
    [[nodiscard]] const std::optional<WorktopCutout>& cutout() const noexcept { return cutout_; }
    void setCutout(std::optional<WorktopCutout> cutout);

    [[nodiscard]] SideWall& leftSide() const noexcept { return *left_; }
    [[nodiscard]] SideWall& rightSide() const noexcept { return *right_; }
    /// The top panel. Part of the carcass only with CarcassTop::Panel.
    [[nodiscard]] TopPanel& top() const noexcept { return *top_; }
    /// The rails; nullptr with a top panel.
    [[nodiscard]] Rail* frontRail() const noexcept;
    [[nodiscard]] Rail* rearRail() const noexcept;
    [[nodiscard]] BottomPanel& bottom() const noexcept { return *bottom_; }
    [[nodiscard]] BackPanel& back() const noexcept { return *back_; }
    [[nodiscard]] Section& interior() const noexcept { return *interior_; }
    /// nullptr without a plinth.
    [[nodiscard]] Plinth* plinth() const noexcept;

    /// The walls, the top panel or the rails, and the bottom — as the carcass has them now.
    [[nodiscard]] std::vector<PanelComponent*> frameParts() const;

    /// Free space inside the carcass (between walls, in front of the back), product coords.
    [[nodiscard]] const Aabb& interiorBox() const noexcept { return interior_->box(); }
    [[nodiscard]] const CarcassFrame& frame() const noexcept { return frame_; }

    /// Origin of the System 32 hole grid: the centre plane of the bottom panel. Holes of this
    /// carcass are at system32OriginY() + k * pitch.
    [[nodiscard]] Real system32OriginY() const noexcept { return bottom_->box().center().y; }

protected:
    void layoutChildren(const LayoutSlot& slot) override;

private:
    [[nodiscard]] std::size_t indexOf(const FurnitureComponent& child) const noexcept;

    JointType topJoint_ = JointType::Overlay;
    JointType bottomJoint_ = JointType::Inset;
    CarcassTop topStyle_ = CarcassTop::Panel;
    std::unique_ptr<BackMount> backMount_;
    CarcassFrame frame_;
    std::optional<Real> fixedWidth_;
    std::optional<WorktopCutout> cutout_;

    std::shared_ptr<SideWall> left_;
    std::shared_ptr<SideWall> right_;
    std::shared_ptr<TopPanel> top_;
    std::shared_ptr<Rail> frontRail_; // with rails only
    std::shared_ptr<Rail> rearRail_;
    std::shared_ptr<BottomPanel> bottom_;
    std::shared_ptr<BackPanel> back_;
    std::shared_ptr<Section> interior_;
    std::shared_ptr<Plinth> plinth_; // in the tree only with a plinth
};

} // namespace rc::core
