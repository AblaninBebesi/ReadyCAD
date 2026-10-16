#pragma once

#include "readycad/core/model/Product.hpp"

#include <vector>

namespace rc::detailing {

/// A part whose shape cannot be made as it is designed.
struct ShapeConflict {
    enum class Reason {
        DoesNotFit,     ///< a feature does not fit: it meets another one, goes through the part,
                        ///< or an opening keeps too little material (core::shapeFits)
        MachiningInCut, ///< a hole or a groove falls into material a feature takes away
    };

    const core::FurnitureComponent* part = nullptr;
    Reason reason = Reason::DoesNotFit;
};

/// The parts with notches, shaped corners or openings that cannot be made. A face hole must lie
/// within the outline and outside the openings with all of its circle; an edge hole must enter
/// through a piece of its edge that is still there and end inside the part; a groove must run
/// through material. The drilling must be up to date (applyDetailing).
[[nodiscard]] std::vector<ShapeConflict> findShapeConflicts(const core::Product& product);

} // namespace rc::detailing
