#pragma once

#include "readycad/core/ConstructionRules.hpp"
#include "readycad/core/Panel.hpp"
#include "readycad/core/model/FurnitureComponent.hpp"

#include <array>
#include <string>
#include <vector>

namespace rc::detailing {

/// Size the panel saw cuts (пильный размер).
struct CutSize {
    core::Real length = 0.0;
    core::Real width = 0.0;
};

/// cut = finish - edge band thickness + pre-milling allowance, for every banded edge;
/// rounded to rules.cutting.rounding.
///
/// Example: shelf 762 x 519 finish, 2 mm ABS on the front edge, 1 mm pre-milling:
/// width 519 - 2 + 1 = 518, length 762 (no edges on the ends).
[[nodiscard]] CutSize cutSize(const core::Panel& panel, const core::ConstructionRules& rules);

/// One line of the cut list (деталировка): identical parts are merged with a quantity.
struct CutListRow {
    core::ComponentKind kind = core::ComponentKind::Shelf;
    std::string name; // the component's own name; empty -> the UI shows the localised kind
    core::Material material;
    core::Real finishLength = 0.0;
    core::Real finishWidth = 0.0;
    core::Real thickness = 0.0;
    CutSize cut;
    std::array<core::EdgeBand, 4> edges{}; // indexed by EdgeSide
    bool keepGrainDirection = false;       // decor with grain: nesting must not rotate it
    int quantity = 0;
    std::vector<core::ComponentId> components;
};

/// Cut list of all panels in the subtree, in tree order (carcass first), identical parts merged.
[[nodiscard]] std::vector<CutListRow> buildCutList(const core::FurnitureComponent& root,
                                                   const core::ConstructionRules& rules);

/// Edge banding consumption per band type (кромкование).
struct EdgeBandUsage {
    core::EdgeBand band;
    core::Real totalLength = 0.0; // mm, including the trim allowance of every piece
    int pieces = 0;
};

[[nodiscard]] std::vector<EdgeBandUsage> edgeBandUsage(const std::vector<CutListRow>& cutList,
                                                       const core::ConstructionRules& rules);

} // namespace rc::detailing
