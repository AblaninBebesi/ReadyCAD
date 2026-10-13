#pragma once

#include "readycad/core/model/Carcass.hpp"
#include "readycad/core/model/FurnitureComponent.hpp"
#include "readycad/core/model/Parts.hpp"

#include <memory>
#include <vector>

namespace rc::core {

enum class RowLevel {
    Base, // alt dolaplar: on the floor, under the worktop
    Wall, // üst dolaplar: hung on the wall
};

/// Dolap dizisi: cabinets (carcasses) side by side, left to right — the columns of a kitchen
/// run. Widths work like the columns of a Section: a cabinet with a fixed width keeps it, the
/// others share the rest equally. Every cabinet is a complete carcass of its own: its fronts
/// keep frontEdgeGap from its own edges, so two neighbours have a frontGap between their fronts.
class CabinetRow final : public FurnitureComponent {
public:
    explicit CabinetRow(RowLevel level);
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::CabinetRow; }
    [[nodiscard]] RowLevel level() const noexcept { return level_; }

    [[nodiscard]] std::vector<Carcass*> cabinets() const;
    /// Moves a cabinet `steps` places to the right (negative: to the left), clamped to the ends.
    /// Throws std::invalid_argument if it is not a cabinet of this row.
    void moveCabinet(ComponentId cabinet, int steps);

    [[nodiscard]] bool canAccept(const FurnitureComponent& child) const override;

protected:
    void layoutChildren(const LayoutSlot& slot) override;

private:
    RowLevel level_;
};

/// Mutfak: a straight kitchen run against a wall. Base cabinets stand on the floor on their
/// plinths, the worktop lies on them, wall cabinets hang with their tops on one line.
///
/// The product box is the run: width = its length, height = the top line of the wall cabinets,
/// depth = the depth of the base cabinets (from the wall). Its own sizes give the rest:
///
///       ┌──────── wall cabinets ────────┐  ─ top line (product height)
///       │                               │    wallHeight, wallDepth deep
///       └───────────────────────────────┘
///
///   ════════════════ worktop ═══════════════  rules.worktopMaterial thick, worktopDepth deep
///       ┌──────── base cabinets ────────┐
///       │                               │    baseHeight (the carcass)
///       └───────────────────────────────┘
///         ▔▔▔▔▔▔▔▔ plinth ▔▔▔▔▔▔▔▔▔▔▔▔▔       rules.kitchen.plinthHeight
class Kitchen final : public FurnitureComponent {
public:
    struct Sizes {
        Real baseHeight = 720.0;   // carcass of a base cabinet, without its plinth
        Real wallHeight = 720.0;
        Real wallDepth = 320.0;
        Real worktopDepth = 600.0; // from the wall: covers the base cabinets and their fronts

        friend bool operator==(const Sizes&, const Sizes&) = default;
    };

    Kitchen();
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::Kitchen; }

    [[nodiscard]] const Sizes& sizes() const noexcept { return sizes_; }
    /// Throws std::invalid_argument for a size <= 0.
    void setSizes(const Sizes& sizes);

    [[nodiscard]] CabinetRow& baseRow() const noexcept { return *baseRow_; }
    [[nodiscard]] CabinetRow& wallRow() const noexcept { return *wallRow_; }
    [[nodiscard]] Worktop& worktop() const noexcept { return *worktop_; }

    /// Base cabinets whose worktop cut-out cannot be made: it leaves less than
    /// rules.kitchen.cutoutMargin of worktop to an edge or to another cut-out, or it does not
    /// fall into the cabinet's opening — between its side walls, in front of its back, clear of
    /// its rails — so the sink bowl or the hob would hit a part. Valid after layout.
    [[nodiscard]] std::vector<const Carcass*> misplacedCutouts(const ConstructionRules& rules) const;

protected:
    void layoutChildren(const LayoutSlot& slot) override;

private:
    /// The cut-out of a cabinet in plan: centre in product X / Z, size along X / Z.
    [[nodiscard]] PanelCutout planOf(const Carcass& cabinet) const;

    Sizes sizes_;
    std::shared_ptr<CabinetRow> baseRow_;
    std::shared_ptr<Worktop> worktop_;
    std::shared_ptr<CabinetRow> wallRow_;
    KitchenFrame frame_; // the worktop's cut-outs, rebuilt on every layout
};

} // namespace rc::core
