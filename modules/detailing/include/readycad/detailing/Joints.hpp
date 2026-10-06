#pragma once

#include "readycad/core/model/PanelComponent.hpp"

#include <vector>

namespace rc::detailing {

/// A butt joint "edge against face", found purely from geometry: the end or long edge of one
/// panel touches the face of another (top on a side wall, bottom between the side walls,
/// partition under the top, drawer box end between the box sides, ...).
///
/// No list of "which part is joined to which" is maintained by hand: any new construction
/// gets its joints automatically.
struct Joint {
    core::PanelComponent* face = nullptr; // its face receives the joint
    core::PanelComponent* edge = nullptr; // its edge touches that face
    core::Axis normal = core::Axis::X;    // perpendicular to the contact plane
    core::Real plane = 0.0;               // coordinate of the contact plane along `normal`
    core::Aabb contact;                   // contact rectangle (flat along `normal`)
    core::Axis across = core::Axis::X;    // the edge panel's thickness axis
    core::Axis along = core::Axis::X;     // connectors are spread along this axis
};

/// Parts that are joined with connectors. Excluded: back panels (groove/nails), drawer bottoms
/// (nails), fronts (hinges / screws from inside), shelves on supports (removable).
[[nodiscard]] bool takesConnectors(const core::FurnitureComponent& component);

/// All edge-to-face joints between connector-taking panels of the subtree.
[[nodiscard]] std::vector<Joint> findJoints(core::FurnitureComponent& root,
                                            core::Real tolerance = 1e-3);

/// Connector positions on [from, to]: `endOffset` from both ends, evenly in between with at most
/// `maxSpacing` between neighbours; a single connector in the middle of a short joint.
[[nodiscard]] std::vector<core::Real> connectorPositions(
    core::Real from, core::Real to, const core::ConstructionRules::Joints& rules);

} // namespace rc::detailing
