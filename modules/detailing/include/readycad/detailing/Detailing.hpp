#pragma once

#include "readycad/core/model/Product.hpp"

#include <string>
#include <vector>

namespace rc::detailing {

struct DetailingReport {
    int joints = 0;
    int drillings = 0;
    int grooves = 0;
    std::vector<std::string> warnings; // e.g. "no wall found for a hinge"
};

/// Generates all machining of the product's panels (присадка):
///
///  * joints     — confirmats / eccentrics / dowels for every edge-to-face joint;
///  * shelves    — shelf-support holes under every shelf on supports (System 32 rows);
///  * doors      — 35 mm hinge cups in the doors, mounting-plate holes in the walls, on the grid;
///  * back panel — grooves in sides, top and bottom when the back is mounted in a groove.
///
/// Holes and grooves are derived data: every relayout clears them, so call this after changes
/// (the editor does it automatically). Calling it twice gives the same result.
DetailingReport applyDetailing(core::Product& product);

} // namespace rc::detailing
