#pragma once

#include "readycad/core/Hardware.hpp"
#include "readycad/core/model/Product.hpp"

#include <string>
#include <vector>

namespace rc::detailing {

using core::HardwareKind;

/// One line of the hardware list (фурнитура): what to take from the store for this product.
struct HardwareItem {
    HardwareKind kind = HardwareKind::Confirmat;
    std::string spec;     ///< size or model: "7 × 50", "Ø35", "Bar 128 mm", "450 mm"
    std::string article;  ///< supplier's article (handles), empty otherwise
    int quantity = 0;     ///< pieces; drawer slides: pairs
};

/// Counted from the drilling (applyDetailing() must have run) and from the model:
///  * confirmats, dowels, eccentrics — one hole in an edge per connector;
///  * shelf supports — 4 per shelf on supports;
///  * hinges — hinge cups in the doors;
///  * handles — per catalogue model on the fronts;
///  * drawer slides — pairs, per slide length;
///  * legs and plinth clips — under cabinets with a plinth; hangers — two per wall cabinet.
/// Kinds with zero pieces are left out.
[[nodiscard]] std::vector<HardwareItem> hardwareList(const core::Product& product);

} // namespace rc::detailing
