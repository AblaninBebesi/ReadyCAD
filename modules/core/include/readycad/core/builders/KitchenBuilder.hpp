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
/// Modules go into their row (base or wall) in the order they are added, left to right.
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
    KitchenBuilder& add(CabinetModule module, std::optional<Real> width = std::nullopt);

    /// Throws std::invalid_argument without a base cabinet (nothing to carry the worktop) or for
    /// sizes <= 0.
    [[nodiscard]] std::shared_ptr<Product> build() const;

private:
    struct Module {
        CabinetModule module;
        std::optional<Real> width;
    };

    std::string name_;
    ConstructionRules rules_;
    Real length_ = 2400.0;
    Real height_ = 2120.0;
    Real depth_ = 560.0;
    Kitchen::Sizes sizes_;
    std::vector<Module> modules_;
};

} // namespace rc::core
