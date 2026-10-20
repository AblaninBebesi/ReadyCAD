#pragma once

#include "readycad/core/model/BackMount.hpp"
#include "readycad/core/model/FurnitureComponent.hpp"
#include "readycad/core/model/Parts.hpp"

#include <memory>
#include <vector>

namespace rc::core {

/// L köşe dolabı: a kitchen corner cabinet shaped like an L — one leg along the back wall, the
/// other along the side wall, open where they meet, without a post between them. A door on each
/// leg's front, folding together (katlanır kapak): the first is hung on the end of its leg, the
/// second on the first with folding hinges, and the handle is on the second.
///
/// It stands at an end of the back run, in the back run's coordinates; its box is the L's
/// bounding box (the back leg along X, the side leg along Z from the back wall). The side run
/// starts at the end of the side leg.
///
///     back wall                      side wall (right)
///    ─────────────────────────────┬─┐
///     │side │  back leg            │ │ back B
///     │     │                      │ │
///     └─────┴───── door ──────┐    │ │
///                       fold ▶│side│ │
///                        door │leg │ │
///                             ├────┤ │ end wall
///                             │ side run …
///
/// Construction: the bottom carries the walls (overlay), the top stands between them, the backs
/// are nailed on — the two of them meet in the corner. The bottom, the top and the shelves are
/// L-shaped panels: a rectangle with a corner notch; their inner edges, the notch, are banded by
/// hand (no edgebander follows an inner corner). A base cabinet stands on a plinth that turns
/// the inner corner.
class CornerCabinet final : public FurnitureComponent {
public:
    /// `end`: the end of the back run it stands at.
    CornerCabinet(RowLevel level, Side end);
    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::CornerCabinet;
    }
    [[nodiscard]] RowLevel level() const noexcept { return level_; }
    [[nodiscard]] Side end() const noexcept { return end_; }

    /// The lengths of the legs from the corner: along the back wall and along the side wall.
    /// Each must be longer than the cabinet is deep (the editor refuses parts without size).
    /// Throws std::invalid_argument for a length <= 0.
    [[nodiscard]] Real legAlongBack() const noexcept { return legBack_; }
    [[nodiscard]] Real legAlongSide() const noexcept { return legSide_; }
    void setLegs(Real alongBack, Real alongSide);

    /// Shelves spread evenly over the interior (a shelf with its own elevation keeps it).
    /// Throws std::invalid_argument outside 0..kMaxShelves.
    [[nodiscard]] int shelfCount() const noexcept { return static_cast<int>(shelves_.size()); }
    void setShelfCount(int count);

    [[nodiscard]] SideWall& side() const noexcept { return *side_; }       // end of the back leg
    [[nodiscard]] EndWall& endWall() const noexcept { return *endWall_; }  // end of the side leg
    [[nodiscard]] CornerPanel& bottom() const noexcept { return *bottom_; }
    [[nodiscard]] CornerPanel& top() const noexcept { return *top_; }
    [[nodiscard]] BackPanel& back() const noexcept { return *back_; }        // on the back wall
    [[nodiscard]] CornerPanel& endBack() const noexcept { return *endBack_; } // on the side wall
    [[nodiscard]] std::vector<CornerShelf*> shelves() const;
    /// The first door: on the back leg, hung on side().
    [[nodiscard]] Door& door() const noexcept { return *door_; }
    /// The second door: on the side leg, hung on door().
    [[nodiscard]] FoldingDoor& foldingDoor() const noexcept { return *foldingDoor_; }
    /// The plinth under the back leg and the one under the side leg; nullptr in a wall cabinet.
    [[nodiscard]] Plinth* plinth() const noexcept { return plinth_.get(); }
    [[nodiscard]] CornerPanel* endPlinth() const noexcept { return endPlinth_.get(); }
    /// The walls, the bottom and the top.
    [[nodiscard]] std::vector<PanelComponent*> frameParts() const;

    /// Origin of the System 32 hole grid: the centre plane of the bottom.
    [[nodiscard]] Real system32OriginY() const noexcept { return bottom_->box().center().y; }

    static constexpr Real kBaseLeg = 900.0; // a base corner cabinet 900 x 900
    static constexpr Real kWallLeg = 600.0; // a wall corner cabinet 600 x 600
    static constexpr int kMaxShelves = 3;

protected:
    /// The L's bounding box: the slot's width (the back leg), height, and the side leg's length.
    Vec3 updateSize(const LayoutSlot& slot) override;
    void layoutChildren(const LayoutSlot& slot) override;

private:
    RowLevel level_;
    Side end_;
    Real legBack_;
    Real legSide_;
    std::unique_ptr<BackMount> backMount_; // nailed on
    CarcassFrame frame_;                    // of the back leg: its wall, back and plinth

    std::shared_ptr<SideWall> side_;
    std::shared_ptr<EndWall> endWall_;
    std::shared_ptr<CornerPanel> bottom_;
    std::shared_ptr<CornerPanel> top_;
    std::shared_ptr<BackPanel> back_;
    std::shared_ptr<CornerPanel> endBack_;
    std::vector<std::shared_ptr<CornerShelf>> shelves_;
    std::shared_ptr<Door> door_;
    std::shared_ptr<FoldingDoor> foldingDoor_;
    std::shared_ptr<Plinth> plinth_;          // base only
    std::shared_ptr<CornerPanel> endPlinth_;  // base only
};

} // namespace rc::core
