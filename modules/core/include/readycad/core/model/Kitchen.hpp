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

/// The walls of a kitchen. The back wall always has a run of cabinets; one side wall has one
/// too in an L kitchen (Г-образная), both in a U kitchen (П-образная).
enum class KitchenWall { Back, Left, Right };

/// Köşe: where the back wall's run meets a side wall's run, at an end of the back run. Its
/// cabinets belong to the back run — its coordinates — and stand at that end; the side run
/// starts in front of them.
///
/// Blind corner (kör köşe): the cabinets are ordinary ones (Carcass::setBlindCorner) as wide as
/// their fixedWidth, whose end runs on behind the side run's cabinets; a blind panel closes
/// that part of their fronts.
///
///      back wall                     side wall (right)
///     ─────────────────────────────┐
///      … │   corner cabinet        │
///        │  door │fill│ hidden     │
///     ───┴───────┴────┼────────────┤
///                     │ side run   │
class KitchenCorner final : public FurnitureComponent {
public:
    /// `end`: the end of the back run it stands at. The cabinets come ready made
    /// (makeCorner() in KitchenBuilder.hpp); throws std::invalid_argument for null.
    KitchenCorner(Side end, std::shared_ptr<Carcass> base, std::shared_ptr<Carcass> wall);
    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::KitchenCorner;
    }
    [[nodiscard]] Side end() const noexcept { return end_; }

    [[nodiscard]] Carcass& baseCabinet() const noexcept { return *base_; }
    [[nodiscard]] Carcass& wallCabinet() const noexcept { return *wall_; }
    /// What the corner takes of the back run, base and wall: its cabinets' widths.
    [[nodiscard]] Real baseWidth() const noexcept;
    [[nodiscard]] Real wallWidth() const noexcept;

    static constexpr Real kBaseWidth = 1100.0; // without a fixed width
    static constexpr Real kWallWidth = 800.0;

protected:
    void layoutChildren(const LayoutSlot& slot) override;

private:
    Side end_;
    std::shared_ptr<Carcass> base_;
    std::shared_ptr<Carcass> wall_;
    CornerFrame baseFrame_; // rebuilt on every layout
    CornerFrame wallFrame_;
};

/// The cabinets along one wall: base cabinets on their plinths, the worktop on them, wall
/// cabinets hanging with their tops on one line.
///
/// A run is laid out in coordinates of its own, as a straight kitchen: its length along X, its
/// front towards +Z, the back wall at Z = 0. The kitchen places it on its wall (ownPlacement):
/// the cabinets, their drilling and their parts never learn which wall they stand at.
///
/// The back run holds the corners where side runs meet it, at its ends; a side run's cabinets
/// start in front of them, its worktop at the back run's worktop edge (a butt joint), while the
/// back run's worktop runs on to the side wall.
class KitchenRun final : public FurnitureComponent {
public:
    explicit KitchenRun(KitchenWall wall);
    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::KitchenRun; }
    [[nodiscard]] KitchenWall wall() const noexcept { return wall_; }

    [[nodiscard]] CabinetRow& baseRow() const noexcept { return *baseRow_; }
    [[nodiscard]] CabinetRow& wallRow() const noexcept { return *wallRow_; }
    [[nodiscard]] Worktop& worktop() const noexcept { return *worktop_; }
    /// The corner at an end of the back run; nullptr without a side run there.
    [[nodiscard]] KitchenCorner* corner(Side end) const noexcept;
    /// Every cabinet of the run, base and wall, the corners' included.
    [[nodiscard]] std::vector<Carcass*> cabinets() const;

    /// A side run's length along its wall: from the room's front to the back wall. (The back
    /// run is as long as the product is wide.) Throws std::invalid_argument for a length <= 0.
    [[nodiscard]] Real length() const noexcept { return length_; }
    void setLength(Real length);

    /// Base cabinets whose worktop cut-out cannot be made: it leaves less than
    /// rules.kitchen.cutoutMargin of worktop to an edge or to another cut-out, or it does not
    /// fall into the cabinet's opening — between its side walls, in front of its back, clear of
    /// its rails — so the sink bowl or the hob would hit a part. Valid after layout.
    [[nodiscard]] std::vector<const Carcass*> misplacedCutouts(const ConstructionRules& rules) const;

protected:
    void layoutChildren(const LayoutSlot& slot) override;
    [[nodiscard]] const Placement* ownPlacement() const noexcept override { return &placement_; }

private:
    friend class Kitchen; // places the run and its corners

    /// The cut-out of a cabinet in plan: centre in run X / Z, size along X / Z.
    [[nodiscard]] PanelCutout planOf(const Carcass& cabinet) const;

    KitchenWall wall_;
    Real length_ = 1800.0;
    Placement placement_; // set by the kitchen before every layout
    std::shared_ptr<CabinetRow> baseRow_;
    std::shared_ptr<Worktop> worktop_;
    std::shared_ptr<CabinetRow> wallRow_;
    std::shared_ptr<KitchenCorner> corners_[2]; // at the left and the right end (back run only)
    KitchenFrame frame_; // the worktop's: cut-outs and closed ends, rebuilt on every layout
};

/// Mutfak: kitchen runs against the walls of a room — a straight kitchen on the back wall, an
/// L kitchen with a run on one side wall, a U kitchen with runs on both.
///
/// The product box is the back run: width = its length, height = the top line of the wall
/// cabinets, depth = the depth of the base cabinets (from the wall). Its own sizes give the rest;
/// a side run is as long as its own length() and stands at the product's left (X = 0) or right
/// side, from the back wall (Z = 0) towards the room:
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

    [[nodiscard]] KitchenRun& backRun() const noexcept { return *backRun_; }
    /// The run on a side wall; nullptr if that wall has none.
    [[nodiscard]] KitchenRun* sideRun(Side side) const noexcept;
    /// The runs that are there: the back run first, then the left and the right one.
    [[nodiscard]] std::vector<KitchenRun*> runs() const;
    /// Puts a run on a side wall with the corner where it meets the back run (replacing what
    /// was there), or takes both away (nullptr, nullptr). Throws std::invalid_argument for a
    /// run of another wall, a corner of the other end, or only one of the two.
    void setSideRun(Side side, std::shared_ptr<KitchenRun> run, std::shared_ptr<KitchenCorner> corner);

    /// The back run's rows and worktop (all a straight kitchen has).
    [[nodiscard]] CabinetRow& baseRow() const noexcept { return backRun_->baseRow(); }
    [[nodiscard]] CabinetRow& wallRow() const noexcept { return backRun_->wallRow(); }
    [[nodiscard]] Worktop& worktop() const noexcept { return backRun_->worktop(); }

    /// KitchenRun::misplacedCutouts() of every run.
    [[nodiscard]] std::vector<const Carcass*> misplacedCutouts(const ConstructionRules& rules) const;

protected:
    void layoutChildren(const LayoutSlot& slot) override;

private:
    Sizes sizes_;
    std::shared_ptr<KitchenRun> backRun_;
    std::shared_ptr<KitchenRun> sideRuns_[2]; // left, right
    KitchenFrame frame_; // what the runs get, rebuilt on every layout
};

} // namespace rc::core
