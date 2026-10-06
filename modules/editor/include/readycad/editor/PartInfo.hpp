#pragma once

#include "readycad/core/ConstructionRules.hpp"
#include "readycad/core/model/FurnitureComponent.hpp"
#include "readycad/detailing/CutList.hpp"

#include <array>
#include <map>
#include <optional>
#include <string>

namespace rc::editor {

/// Read-only technical data of one part for the lower half of the properties panel.
struct PanelInfo {
    core::Material material;
    core::Real finishLength = 0.0;
    core::Real finishWidth = 0.0;
    core::Real thickness = 0.0;
    detailing::CutSize cut;
    std::array<core::EdgeBand, 4> edges{};
    std::map<core::FastenerType, int> holes; // per hole type
    int holeCount = 0;
    int grooveCount = 0;
};

struct PartInfo {
    core::ComponentKind kind = core::ComponentKind::Product;
    std::string name;
    core::Size3D size;      // of the box, mm
    core::Vec3 position{0}; // minimum corner, product coordinates
    std::optional<PanelInfo> panel;
};

[[nodiscard]] PartInfo describe(const core::FurnitureComponent& component,
                                const core::ConstructionRules& rules);

} // namespace rc::editor
