#pragma once

#include "readycad/core/model/Kitchen.hpp"
#include "readycad/core/model/Product.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rc::core {

/// The kitchen modules the builder (and the editor's "add a cabinet") knows. Each one is an
/// ordinary carcass with a recipe; everything can be changed afterwards.
enum class CabinetModule {
    BaseDoors,   // alt dolap: doors and a shelf, flat rails
    BaseDrawers, // çekmeceli alt dolap: three drawers, flat rails
    Sink,        // evye dolabı: doors, no shelf, upright rails, a sink cut-out in the worktop
    Hob,         // ocak dolabı: doors and a shelf, upright rails, a hob cut-out in the worktop
    WallDoors,   // üst dolap: doors and a shelf
    WallOpen,    // açık üst dolap: a shelf, no doors
};

/// The row a module belongs to.
[[nodiscard]] RowLevel levelOf(CabinetModule module) noexcept;

/// A cabinet of a module, ready for a CabinetRow:
///  * base cabinets stand on a plinth, the sides on the bottom (it carries the legs), rails in
///    place of a top panel (the worktop lies on the sides);
///  * wall cabinets have a top between the sides (the hangers go into its corners) and the
///    sides on the bottom (seen from below, the bottom's edges are banded, not the sides' ends);
///  * door handles at the top of base doors, at the bottom of wall doors.
/// `width`: fixed width in the row, nullopt = automatic. `nominalWidth`: what the cabinet will
/// be about; up to 500 mm it gets one door, wider two.
[[nodiscard]] std::shared_ptr<Carcass> makeCabinet(CabinetModule module,
                                                   std::optional<Real> width, Real nominalWidth);

/// The filler of a new blind corner cabinet: the strip of its blind panel between its door and
/// the other wall's fronts — room for the handles of both when the doors open.
inline constexpr Real kCornerFiller = 50.0;

/// The corner where a side run meets the back run, at the back run's `end`:
///  * Blind: blind corner cabinets, a base one (KitchenCorner::kBaseWidth wide, a door and a
///    shelf, like a base cabinet) and a wall one (kWallWidth), their doors hinged on the side
///    away from the corner, `filler` showing between the door and the side run;
///  * LShaped: L corner cabinets, 900 x 900 below, 600 x 600 above, one shelf each.
[[nodiscard]] std::shared_ptr<KitchenCorner> makeCorner(Side end, CornerType type = CornerType::Blind,
                                                        Real filler = kCornerFiller);

/// Builder of a straight kitchen run:
///
///     auto kitchen = KitchenBuilder()
///                        .length(2400)
///                        .add(CabinetModule::BaseDrawers, 400)
///                        .add(CabinetModule::Sink, 600)
///                        .add(CabinetModule::BaseDoors)        // takes the rest: 1400
///                        .add(CabinetModule::WallDoors, 600)
///                        .add(CabinetModule::WallDoors)
///                        .build();
///
/// Modules go into their row (base or wall) in the order they are added, left to right. With
/// sideWall() the kitchen gets a run on a side wall too (an L kitchen; both sides: a U), its
/// modules added with that wall; the corners are blind corner cabinets. Left to right in a run
/// is as one stands facing its wall: the left run ends at the back wall, the right run starts
/// there.
class KitchenBuilder {
public:
    KitchenBuilder& name(std::string name);
    KitchenBuilder& rules(ConstructionRules rules);
    /// Length of the run (product width).
    KitchenBuilder& length(Real length);
    /// Top line of the wall cabinets, from the floor (product height).
    KitchenBuilder& height(Real height);
    /// Depth of the base cabinets (product depth).
    KitchenBuilder& depth(Real depth);
    KitchenBuilder& sizes(const Kitchen::Sizes& sizes);
    KitchenBuilder& add(CabinetModule module, std::optional<Real> width = std::nullopt,
                        KitchenWall wall = KitchenWall::Back);
    /// A run on a side wall, `length` mm along it from the back wall, and its corner.
    KitchenBuilder& sideWall(Side side, Real length, CornerType corner = CornerType::Blind);

    /// Throws std::invalid_argument for a run without a base cabinet (nothing to carry its
    /// worktop), modules for a wall without a run, or sizes <= 0.
    [[nodiscard]] std::shared_ptr<Product> build() const;

private:
    struct Module {
        CabinetModule module;
        std::optional<Real> width;
        KitchenWall wall;
    };

    std::string name_;
    ConstructionRules rules_;
    Real length_ = 2400.0;
    Real height_ = 2120.0;
    Real depth_ = 560.0;
    Kitchen::Sizes sizes_;
    std::optional<Real> sideLengths_[2]; // left, right
    CornerType corners_[2] = {CornerType::Blind, CornerType::Blind};
    std::vector<Module> modules_;
};

} // namespace rc::core
